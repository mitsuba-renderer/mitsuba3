from __future__ import annotations # Delayed parsing of type annotations
from typing import Optional, Tuple, Callable, Any, Union

import drjit as dr
import mitsuba as mi
import gc

from .common import RBIntegrator, mis_weight
from .acoustic_ad import AcousticADIntegrator

class AcousticPRBIntegrator(AcousticADIntegrator):
    """This Integrator is biased for when moving geometry"""
    
    @dr.syntax
    def sample(self,
               scene: mi.Scene,
               sampler: mi.Sampler,
               sensor: mi.Sensor,
               ray: mi.Ray3f,
               block: mi.ImageBlock,
               active: mi.Bool,
               mode: Optional[dr.ADMode] = dr.ADMode.Primal,
               δH: Optional[mi.ImageBlock] = None,
               state_in_δHL: Optional[mi.Spectrum] = None,
               state_in_δHdT: Optional[mi.Spectrum] = None,
               **_ # Absorbs unused arguments
    ) -> Tuple[mi.Spectrum, mi.Bool, mi.Spectrum]:

        # Rendering a primal image? (vs performing forward/reverse-mode AD)
        prb_mode = δH is not None
        primal = mode == dr.ADMode.Primal
        adjoint = prb_mode and (not primal)
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
                pos = mi.Point2f(ray.wavelengths[0] - mi.Float(1.0), block.size().y * T / max_distance)
                δHL = dr.detach(Le) * δH.read(pos=pos, active=active)[0]

                dr.forward_from(T)
                δHdLedT = dr.detach(dr.grad(δHL))
                dr.disable_grad(T)
                    
            return δHdLedT

        while dr.hint(active,
                      max_iterations=self.max_depth,
                      label="Acoustic Path Replay Backpropagation (%s)" % mode.name):
            active_next = mi.Bool(active)

            # The first path vertex requires some special handling (see below)
            first_vertex = (depth == 0)            

            with dr.resume_grad(when=not primal):
                prev_si = prev_pi.compute_surface_interaction(prev_ray, ray_flags=mi.RayFlags.All)

                # The previous intersection defines the origin of the ray, and it moves with the intersected shape
                # This only captures part of the gradient since moving a single vertex moves the *full* path suffix,
                # assuming sampling of directions (i.e., `prev_si` also affects all intersections after `si`)
                ray.o = dr.replace_grad(ray.o, dr.select(~first_vertex, prev_si.p, 0))

            # Compute a surface interaction that tracks derivatives arising
            # from differentiable shape parameters (position, normals, etc.)
            # In primal mode, this is just an ordinary ray tracing operation.
            pi = scene.ray_intersect_preliminary(ray, coherent=first_vertex)
            with dr.resume_grad(when=not primal):
                si = pi.compute_surface_interaction(ray, ray_flags=mi.RayFlags.All)

            # Get the BSDF, potentially computes texture-space differentials
            bsdf = si.bsdf(ray)

            # ---------------------- Direct emission ----------------------

            # Hide the environment emitter if necessary
            if dr.hint(self.hide_emitters, mode='scalar'):
                active_next &= ~((depth == 0) & ~si.is_valid())
                

            # Compute MIS weight for emitter sample from previous bounce
            ds = mi.DirectionSample3f(scene, si=si, ref=prev_si)

            mis = mis_weight(
                prev_bsdf_pdf,
                scene.pdf_emitter_direction(prev_si, ds, ~prev_bsdf_delta)
            )

            # Intensity of current emitter weighted by importance (def. by prev bsdf hits)
            with dr.resume_grad(when=not primal):
                Le = β * mis * ds.emitter.eval(si, active_next)

            with dr.resume_grad(when=not primal):
                τ = dr.select(first_vertex, dr.norm(si.p - ray.o), dr.norm(si.p - prev_si.p))

            # 
            T       = distance + τ
            δHdLedT = compute_δH_dot_dLedT(Le, T, ray, active=active_next & si.is_valid()) \
                      if dr.hint(prb_mode and self.track_time_derivatives, mode='scalar') else 0

            # ---------------------- Emitter sampling ----------------------

            # Should we continue tracing to reach one more vertex?
            active_next &= (depth + 1 < self.max_depth) & si.is_valid()

            # Is emitter sampling even possible on the current vertex?
            active_em = active_next & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)

            # If so, randomly sample an emitter without derivative tracking.
            ds_em, em_weight = scene.sample_emitter_direction(
                si, sampler.next_2d(), True, active_em)
            active_em &= (ds_em.pdf != 0.0)

            with dr.resume_grad(when=not primal):
                if dr.hint(not primal, mode='scalar'):
                    # Retrace the ray towards the emitter because ds_em is directly sampled
                    # from the emitter shape instead of tracing a ray against it.
                    # This contradicts the definition of "sampling of *directions*"
                    si_em       = scene.ray_intersect(si.spawn_ray(ds_em.d), active=active_em)
                    ds_em_attached = mi.DirectionSample3f(scene, si_em, ref=si)
                    ds_em_attached.pdf, ds_em_attached.delta, ds_em_attached.uv, ds_em_attached.n = (ds_em.pdf, ds_em.delta, si_em.uv, si_em.n)
                    ds_em = ds_em_attached

                    # The sampled emitter direction and the pdf must be detached
                    # Recompute `em_weight = em_val / ds_em.pdf` with only `em_val` attached
                    dr.disable_grad(ds_em.d, ds_em.pdf)
                    em_val    = scene.eval_emitter_direction(si, ds_em, active_em)
                    em_weight = dr.replace_grad(em_weight, dr.select((ds_em.pdf != 0), em_val / ds_em.pdf, 0))

                # Evaluate BSDF * cos(theta) differentiably
                wo = si.to_local(ds_em.d)
                bsdf_value_em, bsdf_pdf_em = bsdf.eval_pdf(bsdf_ctx, si, wo, active_em)
                dr.disable_grad(bsdf_pdf_em)
            
            mis_em = dr.select(ds_em.delta, 1, mis_weight(ds_em.pdf, bsdf_pdf_em))

            with dr.resume_grad(when=not primal):
                Lr_dir = β * mis_em * bsdf_value_em * em_weight

            with dr.resume_grad(when=not primal):
                τ_dir = dr.norm(ds_em.p - si.p)

            # 
            T_dir       = distance + τ + τ_dir
            δHdLr_dirdT = compute_δH_dot_dLedT(Lr_dir, T_dir, ray, active=active_em) \
                          if dr.hint(prb_mode and self.track_time_derivatives, mode='scalar') else 0

            # ------------------ Detached BSDF sampling -------------------

            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                   sampler.next_1d(),
                                                   sampler.next_2d(),
                                                   active_next)

            # ------------------ put and accumulate current (differential) radiance -------------------

            δHdLdT_τ_cur = mi.Float(0.)
            if dr.hint(prb_mode and self.track_time_derivatives, mode='scalar'):
                if dr.hint(primal, mode='scalar'):
                    δHdLdT = δHdLdT + δHdLedT + δHdLr_dirdT
                else: # adjoint:
                    with dr.resume_grad():
                        δHdLdT_τ_cur = τ * δHdLdT + τ_dir * δHdLr_dirdT
                    δHdLdT = δHdLdT - δHdLedT - δHdLr_dirdT

            Le_pos     = mi.Point2f(ray.wavelengths[0] - mi.Float(1.0), block.size().y * T     / max_distance)
            Lr_dir_pos = mi.Point2f(ray.wavelengths[0] - mi.Float(1.0), block.size().y * T_dir / max_distance)
            if dr.hint(prb_mode, mode='scalar'):
                # backward_from(δHLx) is the same as splatting_and_backward_gradient_image but we can store it this way
                with dr.resume_grad(when=not primal):
                    δHLe     = Le     * δH.read(pos=Le_pos)[0]
                    δHLr_dir = Lr_dir * δH.read(pos=Lr_dir_pos)[0]
                if dr.hint(primal, mode='scalar'):
                    δHL = δHL + δHLe + δHLr_dir
                else: # adjoint:
                    δHL = δHL - δHLe - δHLr_dir
            else: # primal
                # FIXME (MW): Why are we ignoring active and active_em when writing to the block?
                #       Should still work for samples that don't hit geometry (because distance will be inf)
                #       but what about other reasons for becoming inactive?                             
                block.put(pos=Le_pos,     values=[Le[0],     1.0], active=(Le[0]     > 0.))
                block.put(pos=Lr_dir_pos, values=[Lr_dir[0], 1.0], active=(Lr_dir[0] > 0.))

            # ---- Update loop variables based on current interaction -----

            # Information about the current vertex needed by the next iteration
            prev_ray = dr.detach(ray, True)
            prev_pi  = dr.detach(pi, True)
            prev_bsdf_pdf = bsdf_sample.pdf
            prev_bsdf_delta = mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Delta)

            ray = si.spawn_ray(si.to_world(bsdf_sample.wo))
            η *= bsdf_sample.eta
            β *= bsdf_weight
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

            # ------------------ Differential phase only ------------------

            if dr.hint(not primal, mode='scalar'):
                with dr.resume_grad():
                    # Recompute 'wo' to propagate derivatives to cosine term
                    wo = si.to_local(ray.d)

                    # Re-evaluate BSDF * cos(theta) differentiably
                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active_next)

                    # Detached version of the above term and inverse
                    bsdf_val_detach = bsdf_weight * bsdf_sample.pdf
                    inv_bsdf_val_detach = dr.select((bsdf_val_detach != 0),
                                                    dr.rcp(bsdf_val_detach), 0)

                    # Differentiable version of the reflected indirect radiance
                    δHLr_ind = δHL * dr.replace_grad(1, inv_bsdf_val_detach * bsdf_val)

                    # Differentiable Monte Carlo estimate of all contributions
                    δHLo = δHLe + δHLr_dir + δHLr_ind  + δHdLdT_τ_cur

                    attached_contrib = dr.flag(dr.JitFlag.VCallRecord) and not dr.grad_enabled(δHLo)
                    if dr.hint(attached_contrib, mode='scalar'):
                        raise Exception(
                            "The contribution computed by the differential "
                            "rendering phase is not attached to the AD graph! "
                            "Raising an exception since this is usually "
                            "indicative of a bug (for example, you may have "
                            "forgotten to call dr.enable_grad(..) on one of "
                            "the scene parameters, or you may be trying to "
                            "optimize a parameter that does not generate "
                            "derivatives in detached PRB.)")

                    # Propagate derivatives from/to 'Lo' based on 'mode'
                    if dr.hint(mode == dr.ADMode.Backward, mode='scalar'):
                        dr.backward_from(δHLo)

            depth[si.is_valid()] += 1
            active = active_next

        return (
            δHL,                   # differential radiance
            (depth != 0),          # Ray validity flag for alpha blending
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
            δH = mi.ImageBlock(grad_in,
                               rfilter=film.rfilter(),
                               border=film.sample_border(),
                               y_only=True)
            block = film.create_block()

            # Launch the Monte Carlo sampling process in primal mode (1)
            δHL, valid, δHdT = self.sample(
                scene=scene,
                sampler=sampler.clone(),
                sensor=sensor,
                ray=ray,
                block=block,
                active=mi.Bool(True),
                mode=dr.ADMode.Primal,
                δH=δH,
                state_in_δHL=None,
                state_in_δHdT=None
            )

            # Launch Monte Carlo sampling in backward AD mode (2)
            L_2, valid_2, state_out_δHdT_2 = self.sample(
                scene=scene,
                sampler=sampler,
                sensor=sensor,
                ray=ray,
                block=block,
                active=mi.Bool(True),
                mode=dr.ADMode.Backward,
                δH=δH,
                state_in_δHL=δHL,
                state_in_δHdT=δHdT
            )

            # We don't need any of the outputs here
            del δHL, L_2, valid, valid_2, δHdT, state_out_δHdT_2, \
                ray, weight, sampler #, pos

            gc.collect()

            # Run kernel representing side effects of the above
            dr.eval()

mi.register_integrator("acoustic_prb", lambda props: AcousticPRBIntegrator(props))
