from __future__ import annotations # Delayed parsing of type annotations
from typing import Optional, Tuple, Callable, Any, Union

import drjit as dr
import mitsuba as mi
import gc

from .common import RBIntegrator, mis_weight

class AcousticADIntegrator(RBIntegrator):
    def __init__(self, props = mi.Properties()):
        super().__init__(props)

        self.max_time    = props.get("max_time", 1.)
        self.speed_of_sound = props.get("speed_of_sound", 343.)
        if self.max_time <= 0. or self.speed_of_sound <= 0.:
            raise Exception("\"max_time\" and \"speed_of_sound\" must be set to a value greater than zero!")

        self.is_detached = props.get("is_detached", True)

        self.skip_direct = props.get("skip_direct", False)

        self.track_time_derivatives = props.get("track_time_derivatives", True)

        max_depth = props.get('max_depth', 6)
        if max_depth < 0 and max_depth != -1:
            raise Exception("\"max_depth\" must be set to -1 (infinite) or a value >= 0")

        # Map -1 (infinity) to 2^32-1 bounces
        self.max_depth = max_depth if max_depth != -1 else 0xffffffff

        self.rr_depth = props.get('rr_depth', self.max_depth)
        if self.rr_depth <= 0:
            raise Exception("\"rr_depth\" must be set to a value greater than zero!")


    def render(self: mi.SamplingIntegrator,
               scene: mi.Scene,
               sensor: Union[int, mi.Sensor] = 0,
               seed: int = 0,
               spp: int = 0,
               develop: bool = True,
               evaluate: bool = True) -> mi.TensorXf:

        if not develop:
            raise Exception("develop=True must be specified when "
                            "invoking AD integrators")

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(
                sensor=sensor,
                seed=seed,
                spp=spp,
                aovs=self.aov_names()
            )

            # Generate a set of rays starting at the sensor
            ray, weight = self.sample_rays(scene, sensor, sampler)

            # Prepare an ImageBlock as specified by the film
            block = film.create_block()

            # Launch the Monte Carlo sampling process in primal mode
            self.sample(
                scene=scene,
                sampler=sampler,
                sensor=sensor,
                ray=ray,
                block=block,
                active=mi.Bool(True)
            )

            # Explicitly delete any remaining unused variables
            del sampler, ray, weight#, pos, L, valid
            gc.collect()

            # Perform the weight division and return an image tensor
            film.put_block(block)
            self.primal_image = film.develop()

            return self.primal_image

    def sample_rays(
        self,
        scene: mi.Scene,
        sensor: mi.Sensor,
        sampler: mi.Sampler
    ) -> Tuple[mi.RayDifferential3f, mi.Spectrum, mi.Vector2f, mi.Float]:

        film = sensor.film()
        film_size = film.crop_size()
        rfilter = film.rfilter()
        border_size = rfilter.border_size()

        if film.sample_border():
            film_size += 2 * border_size

        spp = sampler.sample_count()

        # In the acoustic setting, film_size.x = number of wavelengths * number of microphones 
        # (the latter can be > 1 for batch sensors)

        # Compute discrete sample position
        idx = dr.arange(mi.UInt32, film_size.x * spp) #dr.prod(film_size) * spp)

        # Try to avoid a division by an unknown constant if we can help it
        log_spp = dr.log2i(spp)
        if 1 << log_spp == spp:
            idx >>= dr.opaque(mi.UInt32, log_spp)
        else:
            idx //= dr.opaque(mi.UInt32, spp)

        # Compute the position on the image plane
        pos = mi.Vector2i(idx, 0 * idx)

        # Re-scale the position to [0, 1]^2
        scale = dr.rcp(mi.ScalarVector2f(film_size))
        pos_adjusted = mi.Vector2f(pos) * scale

        aperture_sample = mi.Vector2f(0.0)
        if sensor.needs_aperture_sample():
            aperture_sample = sampler.next_2d()

        time = 0.0 # sensor.shutter_open()

        # NOTE (MW): The spectrum indexing is assumed to be 1-based in the scene construction.
        #            If we change it here, we also have to change the Python scripts and notebooks.
        # FIXME (MW): Why was this set to 0 if the indexing is 1-based (and we are subtracting 1 from ray.wavelengths.x)?
        #             Maybe because it doesn't matter as mi.is_spectral is true for acoustic..
        wavelength_sample = 0. 
        if mi.is_spectral:
            # FIXME (MW): This wavelength sampling scheme is broken for batch sensors,
            #             because in this case `idx` does not correspond to a wavelength.
            wavelength_sample = mi.Float(idx) + 1.0

        with dr.resume_grad():
            ray, weight = sensor.sample_ray_differential(
                time=time,
                sample1=wavelength_sample,
                sample2=pos_adjusted,
                sample3=aperture_sample
            )

        return ray, weight

    def prepare(self,
                sensor: mi.Sensor,
                seed: int = 0,
                spp: int = 0,
                aovs: list = []):

        film = sensor.film()
        sampler = sensor.sampler().clone()

        if spp != 0:
            sampler.set_sample_count(spp)

        spp = sampler.sample_count()
        sampler.set_samples_per_wavefront(spp)

        film_size = film.crop_size()

        if film.sample_border():
            film_size += 2 * film.rfilter().border_size()

        wavefront_size = film_size.x * spp # dr.prod(film_size) * spp

        if wavefront_size > 2**32:
            raise Exception(
                "The total number of Monte Carlo samples required by this "
                "rendering task (%i) exceeds 2^32 = 4294967296. Please use "
                "fewer samples per pixel or render using multiple passes."
                % wavefront_size)

        sampler.seed(seed, wavefront_size)
        film.prepare(aovs)

        return sampler, spp

    def sample(self,
               scene: mi.Scene,
               sampler: mi.Sampler,
               sensor: mi.Sensor,
               ray: mi.Ray3f,
               block: mi.ImageBlock,
               active: mi.Bool,
               **_ # Absorbs unused arguments
    ) -> None:

        # Standard BSDF evaluation context for path tracing
        bsdf_ctx = mi.BSDFContext()

        # --------------------- Configure loop state ----------------------

        distance     = mi.Float(0.0)
        max_distance = self.max_time * self.speed_of_sound

        # Copy input arguments to avoid mutating the caller's state
        ray = mi.Ray3f(ray)
        depth = mi.UInt32(0)  # Depth of current vertex
        β = mi.Spectrum(1)                                 # Path throughput weight
        η = mi.Float(1)                                    # Index of refraction
        active = mi.Bool(active)                           # Active SIMD lanes

        # Variables caching information from the previous bounce
        prev_si         = dr.zeros(mi.SurfaceInteraction3f)
        prev_bsdf_pdf   = mi.Float(0.) if self.skip_direct else mi.Float(1.)
        prev_bsdf_delta = mi.Bool(True)

        for it in range(self.max_depth):
            active_next = mi.Bool(active)

            # Compute a surface interaction that tracks derivatives arising
            # from differentiable shape parameters (position, normals, etc.)
            # In primal mode, this is just an ordinary ray tracing operation.
            si = scene.ray_intersect(ray,
                                        ray_flags=mi.RayFlags.All,
                                        coherent=(depth == 0))
            
            τ = si.t

            # Get the BSDF, potentially computes texture-space differentials
            bsdf = si.bsdf(ray)

            # ---------------------- Direct emission ----------------------

            # Hide the environment emitter if necessary
            if self.hide_emitters:
                active_next &= ~((depth == 0) & ~si.is_valid())

            # Compute MIS weight for emitter sample from previous bounce
            ds = mi.DirectionSample3f(scene, si=si, ref=prev_si)

            si_pdf = scene.pdf_emitter_direction(prev_si, ds, ~prev_bsdf_delta)
            if self.is_detached:
                dr.disable_grad(si_pdf)

            mis = mis_weight(
                prev_bsdf_pdf,
                si_pdf
            )

            # Intensity of current emitter weighted by importance (def. by prev bsdf hits)
            Le = β * mis * ds.emitter.eval(si, active_next)

            # Store (direct) intensity to the image block
            T      = distance + τ
            active_next &= si.is_valid()
            Le_pos = mi.Point2f(ray.wavelengths[0] - mi.Float(1.0),
                                block.size().y * T / max_distance)
            block.put(pos=Le_pos, values=mi.Vector2f(Le[0], 1.0), active=active_next & (Le[0] > 0))

            # ---------------------- Emitter sampling ----------------------

            # Should we continue tracing to reach one more vertex?
            active_next &= (depth + 1 < self.max_depth)

            # Is emitter sampling even possible on the current vertex?
            active_em = active_next & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)

            # If so, randomly sample an emitter without derivative tracking.
            with dr.suspend_grad(when=not self.is_detached):
                ds, em_weight = scene.sample_emitter_direction(si, sampler.next_2d(), True, active_em)

            # Retrace the ray towards the emitter because ds is directly sampled
            # from the emitter shape instead of tracing a ray against it.
            # This contradicts the definition of "sampling of *directions*"
            si_em       = scene.ray_intersect(si.spawn_ray(ds.d), active=active_em)
            ds_attached = mi.DirectionSample3f(scene, si_em, ref=si)
            ds_attached.pdf, ds_attached.delta, ds_attached.uv, ds_attached.n = (ds.pdf, ds.delta, si_em.uv, si_em.n)
            ds = ds_attached

            if self.is_detached:
                # The sampled emitter direction and the pdf must be detached
                # Recompute `em_weight = em_val / ds.pdf` with only `em_val` attached
                dr.disable_grad(ds.d, ds.pdf)
                em_val    = scene.eval_emitter_direction(si, ds, active_em)
                em_weight = dr.replace_grad(em_weight, dr.select((ds.pdf != 0), em_val / ds.pdf, 0))

            active_em &= (ds.pdf != 0.0)

            # Evaluate BSDF * cos(theta) differentiably (and detach the bsdf pdf)
            wo = si.to_local(ds.d)
            bsdf_value_em, bsdf_pdf_em = bsdf.eval_pdf(bsdf_ctx, si, wo, active_em)
            if self.is_detached:
                dr.disable_grad(bsdf_pdf_em)
            mis_em = dr.select(ds.delta, 1, mis_weight(ds.pdf, bsdf_pdf_em))
            Lr_dir = β * mis_em * bsdf_value_em * em_weight

            # Store (emission sample) intensity to the image block
            τ_dir      = ds.dist
            T_dir      = T + τ_dir
            Lr_dir_pos = mi.Point2f(ray.wavelengths[0] - mi.Float(1.0),
                                    block.size().y * T_dir / max_distance)
            block.put(pos=Lr_dir_pos, values=mi.Vector2f(Lr_dir[0], 1.0), active=active_em & (Lr_dir[0] > 0))

            # ------------------ Detached BSDF sampling -------------------

            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                sampler.next_1d(),
                                                sampler.next_2d(),
                                                active_next)
            
            if self.is_detached:
                # The sampled bsdf direction and the pdf must be detached
                # Recompute `bsdf_weight = bsdf_val / bsdf_sample.pdf` with only `bsdf_val` attached
                dr.disable_grad(bsdf_sample.wo, bsdf_sample.pdf)
                bsdf_val    = bsdf.eval(bsdf_ctx, si, bsdf_sample.wo, active_next)
                bsdf_weight = dr.replace_grad(bsdf_weight, dr.select((bsdf_sample.pdf != 0), bsdf_val / bsdf_sample.pdf, 0))

            # ---- Update loop variables based on current interaction -----

            wo_world = si.to_world(bsdf_sample.wo)
            if self.is_detached:
                # The direction in *world space* is detached
                wo_world = dr.detach(wo_world)

            ray = si.spawn_ray(wo_world) 
            η *= bsdf_sample.eta
            β *= bsdf_weight

            # Information about the current vertex needed by the next iteration

            prev_si = si
            prev_bsdf_pdf = bsdf_sample.pdf
            prev_bsdf_delta = mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Delta)
            distance = T

            # -------------------- Stopping criterion ---------------------

            # Don't run another iteration if the throughput has reached zero
            β_max = dr.max(β)
            active_next &= (β_max != 0)
            active_next &= distance <= max_distance

            # Russian roulette stopping probability (must cancel out ior^2
            # to obtain unitless throughput, enforces a minimum probability)
            rr_prob = dr.minimum(β_max * η**2, .95)

            # Apply only further along the path since, this introduces variance
            rr_active = depth >= self.rr_depth
            β[rr_active] *= dr.rcp(rr_prob)
            rr_continue = sampler.next_1d() < rr_prob
            active_next &= ~rr_active | rr_continue


            depth[si.is_valid()] += 1
            active = active_next

        return block

    def render_forward(self: mi.SamplingIntegrator,
                       scene: mi.Scene,
                       params: Any,
                       sensor: Union[int, mi.Sensor] = 0,
                       seed: int = 0,
                       spp: int = 0) -> mi.TensorXf:

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(
                sensor=sensor,
                seed=seed,
                spp=spp,
                aovs=self.aov_names()
            )

            # Generate a set of rays starting at the sensor
            ray, weight = self.sample_rays(scene, sensor, sampler)

            # Prepare an ImageBlock as specified by the film
            block = film.create_block()

            with dr.resume_grad():
                # Launch the Monte Carlo sampling process in primal mode (1)
                # Contrary to the light case we already need the input gradient as we return δH dot L to avoid storing L which is a function. 
                self.sample(
                    scene=scene,
                    sampler=sampler.clone(),
                    sensor=sensor,
                    ray=ray,
                    block=block,
                    active=mi.Bool(True)
                )
                
                film.put_block(block)
                result_img = film.develop()

                # Propagate the gradients to the image tensor
                dr.forward_to(result_img)

        return dr.grad(result_img)


    def render_backward(self: mi.SamplingIntegrator,
                        scene: mi.Scene,
                        params: Any,
                        grad_in: mi.TensorXf,
                        sensor: Union[int, mi.Sensor] = 0,
                        seed: int = 0,
                        spp: int = 0) -> None:

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(
                sensor=sensor,
                seed=seed,
                spp=spp,
                aovs=self.aov_names()
            )

            # Generate a set of rays starting at the sensor
            ray, weight = self.sample_rays(scene, sensor, sampler)

            # Prepare an ImageBlock as specified by the film
            block = film.create_block()

            with dr.resume_grad():
                # Launch the Monte Carlo sampling process in primal mode (1)
                # Contrary to the light case we already need the input gradient as we return δH dot L to avoid storing L which is a function. 
                self.sample(
                    scene=scene,
                    sampler=sampler.clone(),
                    sensor=sensor,
                    ray=ray,
                    block=block,
                    active=mi.Bool(True)
                )
                
                film.put_block(block)
                result_img = film.develop()

                # Differentiate sample splatting and weight division steps to
                # retrieve the adjoint radiance
                dr.set_grad(result_img, grad_in)
                dr.enqueue(dr.ADMode.Backward, result_img)
                dr.traverse(dr.ADMode.Backward)

                # We don't need any of the outputs here
                del ray, weight, block, sampler

                gc.collect()

                # Run kernel representing side effects of the above
                dr.eval()


mi.register_integrator("acoustic_ad", lambda props: AcousticADIntegrator(props))
