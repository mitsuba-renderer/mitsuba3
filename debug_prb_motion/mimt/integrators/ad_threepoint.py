from __future__ import annotations as __annotations__

import drjit as dr
import mitsuba as mi

from mitsuba.ad.integrators.common import ADIntegrator, mis_weight
import gc
from typing import Any, List, Tuple, Union

from .common import det_over_det, solid_to_surface_reparam_det, sensor_to_surface_reparam_det

class ThreePointIntegrator(ADIntegrator):
    def render_forward(self: mi.SamplingIntegrator,
                       scene: mi.Scene,
                       params: Any,
                       sensor: Union[int, mi.Sensor] = 0,
                       seed: mi.UInt32 = 0,
                       spp: int = 0) -> mi.TensorXf:

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(sensor, seed, spp, self.aov_names())

            # Generate a set of rays starting at the sensor, keep track of
            # derivatives wrt. sample positions ('pos') if there are any
            ray, weight, pos = self.sample_rays(scene, sensor, sampler)

            with dr.resume_grad():
                L, valid, aovs, si = self.sample(
                    mode=dr.ADMode.Forward,
                    scene=scene,
                    sampler=sampler,
                    ray=ray,
                    active=mi.Bool(True)
                )
                
                block = film.create_block()
                # Only use the coalescing feature when rendering enough samples
                block.set_coalesce(block.coalesce() and spp >= 4)

                pos = dr.select(valid, sensor.sample_direction(si, [0, 0], active=valid)[0].uv, pos)

                D = sensor_to_surface_reparam_det(sensor, si, ignore_near_plane=True, active=valid)

                # Accumulate into the image block
                ADIntegrator._splat_to_block(
                    block, film, pos,
                    value=L * weight * det_over_det(D),
                    weight=det_over_det(D),
                    alpha=dr.select(valid, mi.Float(1), mi.Float(0)),
                    aovs=aovs,
                    wavelengths=ray.wavelengths
                )
                # Perform the weight division
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
                        seed: mi.UInt32 = 0,
                        spp: int = 0) -> None:

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(sensor, seed, spp, self.aov_names())

            # Generate a set of rays starting at the sensor, keep track of
            # derivatives wrt. sample positions ('pos') if there are any
            ray, weight, pos = self.sample_rays(scene, sensor, sampler)

            with dr.resume_grad():
                L, valid, aovs, si = self.sample(
                    mode=dr.ADMode.Backward,
                    scene=scene,
                    sampler=sampler,
                    ray=ray,
                    active=mi.Bool(True)
                )

                # Prepare an ImageBlock as specified by the film
                block = film.create_block()

                # Only use the coalescing feature when rendering enough samples
                block.set_coalesce(block.coalesce() and spp >= 4)

                pos = dr.select(valid, sensor.sample_direction(si, [0, 0], active=valid)[0].uv, pos)

                D = sensor_to_surface_reparam_det(sensor, si, ignore_near_plane=True, active=valid)

                # Accumulate into the image block
                ADIntegrator._splat_to_block(
                    block, film, pos,
                    value=L * weight * det_over_det(D),
                    weight=det_over_det(D),
                    alpha=dr.select(valid, mi.Float(1), mi.Float(0)),
                    aovs=aovs,
                    wavelengths=ray.wavelengths
                )
                # Perform the weight division
                film.put_block(block)
                result_img = film.develop()

                # Differentiate sample splatting and weight division steps to
                # retrieve the adjoint radiance
                dr.set_grad(result_img, grad_in)
                dr.enqueue(dr.ADMode.Backward, result_img)
                dr.traverse(dr.ADMode.Backward)

            # We don't need any of the outputs here
            del ray, weight, pos, block, sampler

            # Run kernel representing side effects of the above
            dr.eval()

    def sample(self,
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               ray: mi.Ray3f,
               active: mi.Bool,
               **kwargs # Absorbs unused arguments
    ) -> Tuple[mi.Spectrum, mi.Bool, List[mi.Float], mi.Spectrum]:


        # Standard BSDF evaluation context for path tracing
        bsdf_ctx = mi.BSDFContext()

        # --------------------- Configure loop state ----------------------

        # Copy input arguments to avoid mutating the caller's state
        ray = mi.Ray3f(dr.detach(ray))
        depth = mi.UInt32(0)                          # Depth of current vertex
        L = mi.Spectrum(0)                            # Radiance accumulator
        
        β = mi.Spectrum(1)                            # Path throughput weight
        η = mi.Float(1)                               # Index of refraction
        active = mi.Bool(active)                      # Active SIMD lanes

        # Variables caching information from the previous bounce
        prev_si         = dr.zeros(mi.SurfaceInteraction3f)
        prev_bsdf_pdf   = mi.Float(1.0)
        prev_bsdf_delta = mi.Bool(True)
        
        # Compute the first intersection point and adapt the directions to follow the shape
        # (this could also use preliminary interactions, but we would like to only use `ray_intersect`)
        si = scene.ray_intersect(dr.detach(ray),
                                 ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                 coherent=mi.Bool(True),
                                 active=active)
        ray.d = dr.select(si.is_valid(), dr.normalize(si.p - ray.o), ray.d)
        si.wi = dr.select(si.is_valid(), si.to_local(-ray.d), si.wi) 

        first_si = si

        for it in range(self.max_depth):
            active_next = mi.Bool(active)

            # Get the BSDF, potentially computes texture-space differentials
            bsdf = si.bsdf(ray)

            # ---------------------- Direct emission ----------------------

            # Hide the environment emitter if necessary
            if self.hide_emitters:
                active_next &= ~((depth == 0) & ~si.is_valid())

            # Compute MIS weight for emitter sample from previous bounce
            ds = mi.DirectionSample3f(scene, si=si, ref=prev_si)

            # Compute MIS weight but don't bother differentiating it (detached sampling)
            with dr.suspend_grad():
                si_pdf = scene.pdf_emitter_direction(prev_si, ds, ~prev_bsdf_delta)
                # Since D is contained in both pdfs it cancels in the MIS weight
                mis = mis_weight(
                    prev_bsdf_pdf,
                    si_pdf,
                )

            # Transform the solid angle sample into a surface sample
            # (the reparameterization for the first intersection happens on the caller side)
            if it > 0:
                D = solid_to_surface_reparam_det(si, prev_si.p, active=active_next)
                β *= det_over_det(D)

            Le = β * mis * ds.emitter.eval(si, active_next)
            L += Le

            # ---------------------- Detached Emitter sampling ----------------------

            # Should we continue tracing to reach one more vertex?
            active_next &= (depth + 1 < self.max_depth) & si.is_valid()

            # Is emitter sampling even possible on the current vertex?
            active_em = active_next & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)

            # If so, randomly sample an emitter with derivative tracking.
            ds_em, em_weight = scene.sample_emitter_direction(si, sampler.next_2d(), True, active_em)

            active_em &= (ds_em.pdf != 0.0)
            
            # Retrace the emitter ray so that the intersection point follows the emitter,
            # and to obtain an intersection point with the differentials si_em.dp_du and si_em.dp_dv
            # (only if the emitter is a surface)
            ray_em = si.spawn_ray(ds_em.d)
            si_em  = scene.ray_intersect(dr.detach(ray_em), 
                                         ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                         coherent=mi.Bool(False),
                                         active=active_em & mi.has_flag(ds_em.emitter.flags(), mi.EmitterFlags.Surface))
            ray_em.d = dr.select(si_em.is_valid(), dr.normalize(si_em.p - si.p), ray_em.d)

            # For environment emitters, `si_em` will be invalid, in which
            # case `D_em` is one, and the sample is processed as being a solid angle sample.
            D_em = solid_to_surface_reparam_det(si_em, si.p, active=active_em)

            wo_em = si.to_local(ray_em.d)
            bsdf_value_em, bsdf_pdf_em = bsdf.eval_pdf(bsdf_ctx, si, wo_em, active_em)

            # Since D is contained in both pdfs it cancels in the MIS weight
            mis_em = dr.detach(dr.select(ds_em.delta, 1, mis_weight(ds_em.pdf, bsdf_pdf_em)))

            # Detached Sampling, the two multiplications serve the following purpose:
            # (1) cancel the *attached* solid angle pdf contained in em_weight
            # (2) transform the solid angle sample to a surface sample
            em_weight *= dr.replace_grad(1, ds_em.pdf/dr.detach(ds_em.pdf)) * det_over_det(D_em)

            Lr_dir = β * mis_em * bsdf_value_em * em_weight
            L += Lr_dir

            # ------------------ Detached BSDF sampling -------------------

            with dr.suspend_grad():
                bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                       sampler.next_1d(),
                                                       sampler.next_2d(),
                                                       active_next)
            

            ray_next = si.spawn_ray(si.to_world(bsdf_sample.wo))

            # Compute the next intersection point and adapt the directions again (to follow the shape)
            si_next = scene.ray_intersect(dr.detach(ray_next),
                                          ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                          coherent=mi.Bool(False),
                                          active=active_next)
            ray_next.d = dr.select(si_next.is_valid(), dr.normalize(si_next.p - si.p), ray_next.d)
            si_next.wi = dr.select(si_next.is_valid(), si_next.to_local(-ray_next.d), si_next.wi)
            
            # Recompute 'wo' to propagate derivatives to cosine term
            wo = si.to_local(ray_next.d)

            # Recompute the attached BSDF weight with detached pdf (detached sampling)
            bsdf_val    = bsdf.eval(bsdf_ctx, si, wo, active_next)
            bsdf_weight = dr.replace_grad(bsdf_weight, bsdf_val * dr.select(bsdf_sample.pdf != 0, dr.rcp(bsdf_sample.pdf), 0))

            # ---- Update loop variables based on current interaction -----

            η *= bsdf_sample.eta
            β *= bsdf_weight

            # Information about the current vertex needed by the next iteration
            prev_si = si
            prev_bsdf_pdf = bsdf_sample.pdf
            prev_bsdf_delta = mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Delta)

            # -------------------- Stopping criterion ---------------------

            # Don't run another iteration if the throughput has reached zero
            β_max = dr.max(β)
            active_next &= (β_max != 0)

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
            si  = si_next
            ray = ray_next
    
        return (
            L,                   # Radiance/differential radiance
            (depth != 0),    # Ray validity flag for alpha blending
            [],                  # Empty typle of AOVs
            first_si             # Necessary for screen position
        )

mi.register_integrator("ad_threepoint", lambda props: ThreePointIntegrator(props))
