from __future__ import annotations # Delayed parsing of type annotations
from typing import Optional, Tuple, Callable, Any, Union

import drjit as dr
import mitsuba as mi
import gc

from mitsuba.ad.integrators.common import RBIntegrator, _ReparamWrapper, mis_weight

class PRBThreePointAcousticIntegrator(RBIntegrator):
    def __init__(self, props = mi.Properties()):
        super().__init__(props)

        self.max_time    = props.get("max_time", 1.)
        self.speed_of_sound = props.get("speed_of_sound", 343.)
        if self.max_time <= 0. or self.speed_of_sound <= 0.:
            raise Exception("\"max_time\" and \"speed_of_sound\" must be set to a value greater than zero!")

        self.skip_direct = props.get("skip_direct", False)

        self.track_time_derivatives = props.get("track_time_derivatives", True)

        self.debug = [props.get("debug0", 1), props.get("debug1", 1), props.get("debug2", 1), props.get("debug3", 1), props.get("debug4", 1), props.get("debug5", 1), props.get("debug6", 1)]

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
                aovs=self.aovs()
            )

            # Generate a set of rays starting at the sensor
            ray, weight = self.sample_rays(scene, sensor, sampler)

            # Prepare an ImageBlock as specified by the film
            block = film.create_block()

            # Launch the Monte Carlo sampling process in primal mode
            self.sample(
                mode=dr.ADMode.Primal,
                scene=scene,
                sampler=sampler,
                sensor=None,
                ray=ray,
                block=block,
                δH=None,
                state_in_δHL=None,
                state_in_δHdT=None,
                reparam=None,
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
        sampler: mi.Sampler,
        reparam: Callable[[mi.Ray3f, mi.UInt32, mi.Bool],
                          Tuple[mi.Vector3f, mi.Float]] = None) -> Tuple[mi.RayDifferential3f, mi.Spectrum, mi.Vector2f, mi.Float]:

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
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               sensor: mi.Sensor,
               ray: mi.Ray3f,
               block: mi.ImageBlock,
               δH: Optional[mi.ImageBlock],
               state_in_δHL: Optional[mi.Spectrum],
               state_in_δHdT: Optional[mi.Spectrum],
               active: mi.Bool,
               reparam: Optional[
                   Callable[[mi.Ray3f, mi.Bool], Tuple[mi.Ray3f, mi.Float]]],
               **_ ) -> Tuple[mi.Spectrum, mi.Bool, mi.Spectrum]:

        # Rendering a primal image? (vs performing forward/reverse-mode AD)
        prb_mode = δH is not None
        primal = mode == dr.ADMode.Primal
        adjoint = prb_mode and (mode == dr.ADMode.Backward or mode == dr.ADMode.Forward)
        assert primal or prb_mode

        # Standard BSDF evaluation context for path tracing
        bsdf_ctx = mi.BSDFContext()

        # --------------------- Configure loop state ----------------------

        distance     = mi.Float(0.0)
        max_distance = self.max_time * self.speed_of_sound

        # Copy input arguments to avoid mutating the caller's state
        ray = mi.Ray3f(dr.detach(ray))
        depth = mi.UInt32(0)                               # Depth of current vertex
        δHL    = mi.Spectrum(0) if primal else mi.Spectrum(state_in_δHL)  # Integral of grad_in * Radiance (accumulated)
        δHdLdT = mi.Spectrum(0) if primal else mi.Spectrum(state_in_δHdT) # Integral of grad_in * (Radiance derived wrt time) (accumulated)
        β = mi.Spectrum(1)                                 # Path throughput weight
        η = mi.Float(1)                                    # Index of refraction
        active = mi.Bool(active)                           # Active SIMD lanes

        # Variables caching information from the previous bounce
        prev_ray        = dr.zeros(mi.Ray3f)
        prev_pi         = dr.zeros(mi.PreliminaryIntersection3f)
        prev_bsdf_pdf   = mi.Float(0.) if self.skip_direct else mi.Float(1.)
        prev_bsdf_delta = mi.Bool(True)

        # Helper functions for time derivatives
        def compute_δH_dot_dLedT(Le: mi.Spectrum, T: mi.Float, ray: mi.Ray3f, active: mi.Mask):
            with dr.resume_grad():
                dr.enable_grad(T)
                pos = mi.Point2f(ray.wavelengths.x - mi.Float(1.0), block.size().y * T / max_distance)
                δHL = dr.detach(Le) * δH.read(pos=pos, active=active)[0]

                dr.forward_from(T)
                δHdLedT = dr.detach(dr.grad(δHL))
                dr.disable_grad(T)
                    
            return δHdLedT

        # Record the following loop in its entirety
        loop = mi.Loop(name="PRB Acoustic (%s)" % mode.name,
                       state=lambda: (distance, #block.tensor(),
                                      sampler, ray, depth, δHL, δHdLdT, #δL.tensor(),
                                      β, η, active,
                                      prev_ray, prev_pi, prev_bsdf_pdf, prev_bsdf_delta))

        # Specify the max. number of loop iterations (this can help avoid
        # costly synchronization when when wavefront-style loops are generated)
        loop.set_max_iterations(self.max_depth)

        while loop(active):
            active_next = mi.Bool(active)

            # The first path vertex requires some special handling (see below)
            first_vertex = dr.eq(depth, 0)            

            with dr.resume_grad(when=not primal):
                prev_si = scene.ray_intersect(prev_ray,
                                         ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                         coherent=first_vertex)
                si = scene.ray_intersect(ray,
                                         ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                         coherent=first_vertex)
                # si.wi has a gradient as prev_si might move with pi
                # if dr.hint(not primal, mode='scalar'):
                si.wi = dr.select(first_vertex | ~active_next, si.wi, si.to_local(dr.normalize(prev_si.p - si.p)))

            # Get the BSDF, potentially computes texture-space differentials
            bsdf = si.bsdf(ray)

            # ---------------------- Direct emission ----------------------

            # Hide the environment emitter if necessary
            if self.hide_emitters:
                active_next &= ~(dr.eq(depth, 0) & ~si.is_valid())
                

            # Compute MIS weight for emitter sample from previous bounce
            ds = mi.DirectionSample3f(scene, si=si, ref=prev_si)

            with dr.resume_grad(when=not primal):
                dist_squared = dr.squared_norm(si.p-prev_si.p)
                dp = dr.dot(si.to_world(si.wi), si.n)
                D = dr.select(active_next, dr.norm(dr.cross(si.dp_du, si.dp_dv)) * dp / dist_squared, 1.)
                δHLD = δHL * dr.replace_grad(1., D/dr.detach(D))
                δHLD = dr.select(first_vertex, 0, δHLD)

            mis = mis_weight(
                prev_bsdf_pdf*D,
                scene.pdf_emitter_direction(prev_si, ds, ~prev_bsdf_delta)*D
            )

            # The first samples are sampled differently
            # UF: neccessary? I think mis is 1 for first hit
            mis = dr.select(first_vertex, 1, mis)

            # Intensity of current emitter weighted by importance (def. by prev bsdf hits)
            with dr.resume_grad(when=not primal):
                Le = β * mis * ds.emitter.eval(si, active_next)


            with dr.resume_grad(when=not primal):
                τ = dr.select(first_vertex, dr.norm(si.p - ray.o), dr.norm(si.p - prev_si.p))

            
            active_next &= si.is_valid()

            T       = distance + τ
            δHdLedT = compute_δH_dot_dLedT(Le, T, ray, active=active_next) \
                      if prb_mode and self.track_time_derivatives else 0

            # ---------------------- Emitter sampling ----------------------

            # Should we continue tracing to reach one more vertex?
            active_next &= (depth + 1 < self.max_depth)

            # Is emitter sampling even possible on the current vertex?
            active_em = active_next & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)

            # If so, randomly sample an emitter without derivative tracking.
            ds_em, em_weight = scene.sample_emitter_direction(si, sampler.next_2d(), True, active_em)
            active_em &= dr.neq(ds_em.pdf, 0.0)
            
            with dr.resume_grad(when=not primal):
                # We need to recompute the sample with follow shape so it is a detached uv sample
                si_em = scene.ray_intersect(dr.detach(si.spawn_ray(ds_em.d)), 
                                            ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                            coherent=mi.Bool(False),
                                            active=active_em)

                # calculate the bsdf weight (for path througput) and pdf (for mis weighting)
                diff_em = si_em.p - si.p
                ds_em.d = dr.normalize(diff_em)
                wo = si.to_local(ds_em.d)
                bsdf_value_em, bsdf_pdf_em = bsdf.eval_pdf(bsdf_ctx, si, wo, active_em)
                
                # ds_em.pdf includes the inv geometry term, 
                # and bsdf_pdf_em does not contain the geometry term.
                # -> We need to multiply both with the geometry term:
                dp_em = dr.dot(ds_em.d, si_em.n)
                dist_squared_em = dr.squared_norm(diff_em)
                D_em = dr.select(active_em, dr.norm(dr.cross(si_em.dp_du, si_em.dp_dv)) * -dp_em / dist_squared_em , 0.) 
                
                if not primal:
                    # update gradient of em_weight
                    em_val = scene.eval_emitter_direction(si, ds_em, active_em)
                    em_weight = dr.replace_grad(em_weight, dr.select(dr.neq(ds_em.pdf, 0), em_val / ds_em.pdf, 0)) * dr.replace_grad(1, D_em/dr.detach(D_em))


            mis_em = dr.select(ds_em.delta, 1, mis_weight(ds_em.pdf*D_em, bsdf_pdf_em*D_em))

            with dr.resume_grad(when=not primal):
                Lr_dir = β * mis_em * bsdf_value_em * em_weight

            with dr.resume_grad(when=not primal):
                τ_dir = dr.norm(si_em.p - si.p)

            # 
            T_dir       = distance + τ + τ_dir
            δHdLr_dirdT = compute_δH_dot_dLedT(Lr_dir, T_dir, ray, active=active_em) \
                          if prb_mode and self.track_time_derivatives else 0

            # ------------------ Detached BSDF sampling -------------------

            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                   sampler.next_1d(),
                                                   sampler.next_2d(),
                                                   active_next)

            # ---- Update loop variables based on current interaction -----
            ray_next = si.spawn_ray(si.to_world(bsdf_sample.wo))
                        
            η *= bsdf_sample.eta
            β *= bsdf_weight
            distance = T

            # ------------------ put and accumulate current (differential) radiance -------------------

            δHdLdT_τ_cur = mi.Float(0.)
            if prb_mode and self.track_time_derivatives:
                if primal:
                    δHdLdT = δHdLdT + δHdLedT + δHdLr_dirdT
                elif adjoint:
                    with dr.resume_grad():
                        δHdLdT_τ_cur = τ * δHdLdT + τ_dir * δHdLr_dirdT
                    δHdLdT = δHdLdT - δHdLedT - δHdLr_dirdT

            Le_pos     = mi.Point2f(ray.wavelengths.x - mi.Float(1.0), block.size().y * T     / max_distance)
            Lr_dir_pos = mi.Point2f(ray.wavelengths.x - mi.Float(1.0), block.size().y * T_dir / max_distance)
            if prb_mode:
                # backward_from(δHLx) is the same as splatting_and_backward_gradient_image but we can store it this way
                with dr.resume_grad(when=not primal):
                    δHLe     = Le     * δH.read(pos=Le_pos)[0]
                    δHLr_dir = Lr_dir * δH.read(pos=Lr_dir_pos)[0]
                if primal:
                    δHL = δHL + δHLe + δHLr_dir
                else: # adjoint:
                    δHL = δHL - δHLe - δHLr_dir
            else: # primal
                # FIXME (MW): Why are we ignoring active and active_em when writing to the block?
                #       Should still work for samples that don't hit geometry (because distance will be inf)
                #       but what about other reasons for becoming inactive?                             
                block.put(pos=Le_pos,     values=mi.Vector2f(Le.x,     1.0), active=(Le.x     > 0.))
                block.put(pos=Lr_dir_pos, values=mi.Vector2f(Lr_dir.x, 1.0), active=(Lr_dir.x > 0.))

            # -------------------- Stopping criterion ---------------------

            # Don't run another iteration if the throughput has reached zero
            β_max = dr.max(β)
            active_next &= dr.neq(β_max, 0)
            active_next &= distance <= max_distance

            # Russian roulette stopping probability (must cancel out ior^2
            # to obtain unitless throughput, enforces a minimum probability)
            rr_prob = dr.minimum(β_max * η**2, .95)

            # Apply only further along the path since, this introduces variance
            rr_active = depth >= self.rr_depth
            β[rr_active] *= dr.rcp(rr_prob)
            rr_continue = sampler.next_1d() < rr_prob
            active_next &= ~rr_active | rr_continue

            # ------------------ Differential phase only ------------------

            if not primal:
                with dr.resume_grad():

                    # 'L' stores the indirectly reflected radiance at the
                    # current vertex but does not track parameter derivatives.
                    # The following addresses this by canceling the detached
                    # BSDF value and replacing it with an equivalent term that
                    # has derivative tracking enabled. (nit picking: the
                    # direct/indirect terminology isn't 100% accurate here,
                    # since there may be a direct component that is weighted
                    # via multiple importance sampling)
                    si_next = scene.ray_intersect(ray_next,
                                                  ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                                  coherent=mi.Bool(False),
                                                  active=active_next)
                    
                    # Recompute 'wo' to propagate derivatives to cosine term
                    diff_next = si_next.p - si.p
                    dir_next = dr.normalize(diff_next)
                    wo = si.to_local(dir_next)

                    # Re-evaluate BSDF * cos(theta) differentiably
                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active_next & si_next.is_valid())

                    # Detached version of the above term and inverse
                    bsdf_val_det = bsdf_weight * bsdf_sample.pdf
                    inv_bsdf_val_det = dr.select(dr.neq(bsdf_val_det, 0),
                                                    dr.rcp(bsdf_val_det), 0)

                    # Differentiable version of the reflected indirect radiance
                    δHLr_ind = δHL * dr.replace_grad(1, inv_bsdf_val_det * bsdf_val)

                    ll = [δHLe, δHLD, δHLr_dir, δHLr_ind, δHdLdT_τ_cur]
                    δHLo = δHLe * 0 # I do not know the datatype
                    for i in range(len(ll)):
                        if self.debug[i]:
                            δHLo += ll[i]   
                    # # Differentiable Monte Carlo estimate of all contributions
                    # δHLo = δHLe + δHLD + δHLr_dir + δHLr_ind + δHdLdT_τ_cur

                    # if dr.flag(dr.JitFlag.VCallRecord) and not dr.grad_enabled(δHLo):
                    #     raise Exception(
                    #         "The contribution computed by the differential "
                    #         "rendering phase is not attached to the AD graph! "
                    #         "Raising an exception since this is usually "
                    #         "indicative of a bug (for example, you may have "
                    #         "forgotten to call dr.enable_grad(..) on one of "
                    #         "the scene parameters, or you may be trying to "
                    #         "optimize a parameter that does not generate "
                    #         "derivatives in detached PRB.)")

                    # Propagate derivatives from/to 'Lo' based on 'mode'
                    if mode == dr.ADMode.Backward and dr.grad_enabled(δHLo):
                        dr.backward_from(δHLo)

            # ------------------ Prepare next iteration ------------------
            prev_bsdf_pdf = bsdf_sample.pdf
            prev_bsdf_delta = mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Delta)
            
            depth[si.is_valid()] += 1
            active = active_next
            prev_ray = ray
            ray = ray_next

        return (
            δHL,                   # Radiance/differential radiance
            dr.neq(depth, 0),      # Ray validity flag for alpha blending
            δHdLdT                 # State for the differential phase
        )

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
        aovs = self.aovs()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(sensor, seed, spp, aovs)

            # Generate a set of rays starting at the sensor, keep track of
            # derivatives wrt. sample positions ('pos') if there are any
            ray, weight = self.sample_rays(scene, sensor, sampler, None)

            # Input gradient (a function) represented as discrete (1D-)function
            δH = mi.ImageBlock(grad_in,
                               rfilter=film.rfilter(),
                               border=film.sample_border(),
                               y_only=True)
            
            # actually only to get film width. Is completely ignored for gradient calculation.
            block = film.create_block()

            # Launch the Monte Carlo sampling process in primal mode (1)
            # Contrary to the light case we already need the input gradient as we return δH dot L to avoid storing L which is a function. 
            δHL, valid, δHdT = self.sample(
                mode=dr.ADMode.Primal,
                scene=scene,
                sampler=sampler.clone(),
                sensor=sensor,
                ray=ray,
                block=block,
                δH=δH,
                state_in_δHL=None,
                state_in_δHdT=None,
                reparam=None,
                active=mi.Bool(True)
            )

            # Launch Monte Carlo sampling in backward AD mode (2)
            # given δH dot L and δH dot dL/dT we can calculate the derivatives in one pass
            L_2, valid_2, state_out_δHdT_2 = self.sample(
                mode=dr.ADMode.Backward,
                scene=scene,
                sampler=sampler,
                sensor=sensor,
                ray=ray,
                block=block,
                δH=δH,
                state_in_δHL=δHL,
                state_in_δHdT=δHdT,
                reparam=None,
                active=mi.Bool(True)
            )

            # The sampled rays are de facto weighted with a von Mises-Fischer distribution, therefore we need to add the derivative of this and D. 
            # This can be interpreted as a BRDF that is present at the sensor. we have a zeroth surface interaction (which is not considered in sample)
            with dr.resume_grad():
                si = scene.ray_intersect(dr.detach(ray),
                                         ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                         coherent=mi.Bool(True))
                weight = dr.select(si.is_valid(), sensor.sample_direction(si, [0, 0], active=si.is_valid())[1], dr.detach(weight))
                diff = si.p-ray.o
                dist_squared = dr.squared_norm(diff)
                dp = dr.dot(dr.normalize(diff), si.n)
                D = dr.select(si.is_valid(), dr.norm(dr.cross(si.dp_du, si.dp_dv)) * dp / dist_squared , 1.)
                # print(dr.norm(dr.cross(si.dp_du, si.dp_dv)))
                # print(si.is_valid())
                if not self.debug[5]:
                    D = dr.detach(D)
                if not self.debug[6]:
                    weight = dr.detach(weight)

                extra = dr.replace_grad(1, weight/dr.detach(weight))*dr.detach(δHL)*dr.replace_grad(1, D/dr.detach(D))
                if dr.grad_enabled(extra):
                    dr.backward_from(extra)

                dr.eval()
                

            # We don't need any of the outputs here
            del δHL, L_2, valid, valid_2, δHdT, state_out_δHdT_2, \
                ray, weight, sampler #, pos

            gc.collect()

            # Run kernel representing side effects of the above
            dr.eval()

mi.register_integrator("prb_threepoint_acoustic", lambda props: PRBThreePointAcousticIntegrator(props))
