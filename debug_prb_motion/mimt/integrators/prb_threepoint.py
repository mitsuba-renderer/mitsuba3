from __future__ import annotations as __annotations__
from typing import Any, List, Optional, Sequence, Tuple, Union

import drjit as dr
import mitsuba as mi

from mitsuba.ad.integrators.common import RBIntegrator, ADIntegrator, mis_weight
import gc

from .common import det_over_det, solid_to_surface_reparam_det, sensor_to_surface_reparam_det

class PRBThreePointIntegrator(RBIntegrator):

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

            # Launch the Monte Carlo sampling process in primal mode (1)
            L, valid, aovs, state_out = self.sample(
                mode=dr.ADMode.Primal,
                scene=scene,
                sampler=sampler.clone(),
                ray=ray,
                depth=mi.UInt32(0),
                δL=None,
                state_in=None,
                active=mi.Bool(True)
            )
                
            # Launch the Monte Carlo sampling process in forward mode (2)
            δL, valid_2, δaovs, state_out_2 = self.sample(
                mode=dr.ADMode.Forward,
                scene=scene,
                sampler=sampler,
                ray=ray,
                depth=mi.UInt32(0),
                δL=None,
                δaovs=None,
                state_in=state_out,
                active=mi.Bool(True)
            )

            # Prepare an ImageBlock as specified by the film
            block = film.create_block()

            # Only use the coalescing feature when rendering enough samples
            block.set_coalesce(block.coalesce() and spp >= 4)

            # Accumulate into the image block
            ADIntegrator._splat_to_block(
                block, film, pos,
                value=δL * weight,
                weight=1.0,
                alpha=dr.select(valid_2, mi.Float(1), mi.Float(0)),
                aovs=[δaov * weight for δaov in δaovs],
                wavelengths=ray.wavelengths
            )
            
            # Perform the weight division and return an image tensor
            film.put_block(block)
            result_grad = film.develop()
            
            film.clear()

            # The shared state contains the first intersection point
            pi = state_out[1]

            with dr.resume_grad():
                block = film.create_block()

                # Only use the coalescing feature when rendering enough samples
                block.set_coalesce(block.coalesce() and spp >= 4)

                # Recompute the first intersection point with derivative tracking
                si = pi.compute_surface_interaction(ray, 
                                                    ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                                    active=valid)
                pos = dr.select(valid, sensor.sample_direction(si, [0, 0], active=valid)[0].uv, pos)

                D = sensor_to_surface_reparam_det(sensor, si, ignore_near_plane=True, active=valid)

                #Accumulate into the image block
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
                dr.forward_to(result_img, flags=dr.ADFlag.Default | dr.ADFlag.AllowNoGrad)
                first_hit = dr.grad(result_img)

        # Explicitly delete any remaining unused variables
        del sampler, ray, weight, pos, L, valid, aovs, δL, δaovs, \
            valid_2, params, state_out, state_out_2#, block
        
        return first_hit + result_grad
    
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

            def splatting_and_backward_gradient_image(value: mi.Spectrum,
                                                      weight: mi.Float,
                                                      alpha: mi.Float,
                                                      aovs: Sequence[mi.Float]):
                '''
                Backward propagation of the gradient image through the sample
                splatting and weight division steps.
                '''

                # Prepare an ImageBlock as specified by the film
                block = film.create_block()

                # Only use the coalescing feature when rendering enough samples
                block.set_coalesce(block.coalesce() and spp >= 4)

                ADIntegrator._splat_to_block(
                    block, film, pos,
                    value=value,
                    weight=weight,
                    alpha=alpha,
                    aovs=aovs,
                    wavelengths=ray.wavelengths
                )

                film.put_block(block)

                image = film.develop()

                dr.set_grad(image, grad_in)
                dr.enqueue(dr.ADMode.Backward, image)
                dr.traverse(dr.ADMode.Backward)

            # Differentiate sample splatting and weight division steps to
            # retrieve the adjoint radiance (e.g. 'δL')
            with dr.resume_grad():
                with dr.suspend_grad(pos, ray, weight):
                    L = dr.full(mi.Spectrum, 1.0, dr.width(ray))
                    dr.enable_grad(L)
                    aovs = []
                    for _ in self.aov_names():
                        aov = dr.ones(mi.Float, dr.width(ray))
                        dr.enable_grad(aov)
                        aovs.append(aov)
                    splatting_and_backward_gradient_image(
                        value=L * weight,
                        weight=1.0,
                        alpha=1.0,
                        aovs=[aov * weight for aov in aovs]
                    )

                    δL = dr.grad(L)
                    δaovs = dr.grad(aovs)

            # Clear the dummy data splatted on the film above
            film.clear()

            # Launch the Monte Carlo sampling process in primal mode (1)
            L, valid, aovs, state_out = self.sample(
                mode=dr.ADMode.Primal,
                scene=scene,
                sampler=sampler.clone(),
                ray=ray,
                depth=mi.UInt32(0),
                δL=None,
                δaovs=None,
                state_in=None,
                active=mi.Bool(True)
            )

            # Launch Monte Carlo sampling in backward AD mode (2)
            L_2, valid_2, aovs_2, state_out_2 = self.sample(
                mode=dr.ADMode.Backward,
                scene=scene,
                sampler=sampler,
                ray=ray,
                depth=mi.UInt32(0),
                δL=δL,
                δaovs=δaovs,
                state_in=state_out,
                active=mi.Bool(True)
            )

            # The shared state contains the first intersection point
            pi = state_out[1]

            with dr.resume_grad():
                # Recompute the first intersection point with derivative tracking
                si = pi.compute_surface_interaction(ray, 
                                                    ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                                    active=valid)
                pos = dr.select(valid, sensor.sample_direction(si, [0, 0], active=valid)[0].uv, pos)

                D = sensor_to_surface_reparam_det(sensor, si, ignore_near_plane=True, active=valid)

                # Prepare an ImageBlock as specified by the film
                block2 = film.create_block()

                # Only use the coalescing feature when rendering enough samples
                block2.set_coalesce(block2.coalesce() and spp >= 4)

                # Accumulate into the image block
                ADIntegrator._splat_to_block(
                    block2, film, pos,
                    value=L * weight * det_over_det(D),
                    weight=det_over_det(D),
                    alpha=dr.select(valid, mi.Float(1), mi.Float(0)),
                    aovs=aovs,
                    wavelengths=ray.wavelengths
                )

                # film the weight division and return an image tensor
                film.put_block(block2)
                image = film.develop()
                dr.set_grad(image, grad_in)

                dr.enqueue(dr.ADMode.Backward, image)
                dr.traverse(dr.ADMode.Backward)
                
                dr.eval()
                
            # We don't need any of the outputs here
            del L_2, valid_2, aovs_2, state_out, state_out_2, \
                δL, δaovs, ray, weight, pos, sampler


            # Run kernel representing side effects of the above
            dr.eval()

    @dr.syntax
    def sample(self,
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               ray: mi.Ray3f,
               δL: Optional[mi.Spectrum],
               state_in: Optional[Tuple[mi.Spectrum, mi.PreliminaryIntersection3f]],
               active: mi.Bool,
               **kwargs # Absorbs unused arguments
    ) -> Tuple[mi.Spectrum, mi.Bool, List[mi.Float], mi.Spectrum]:
        
        # Rendering a primal image? (vs performing forward/reverse-mode AD)
        primal = mode == dr.ADMode.Primal

        # Standard BSDF evaluation context for path tracing
        bsdf_ctx = mi.BSDFContext()

        # --------------------- Configure loop state ----------------------

        # Copy input arguments to avoid mutating the caller's state
        ray = mi.Ray3f(dr.detach(ray))
        depth = mi.UInt32(0)                          # Depth of current vertex
        L = mi.Spectrum(0 if primal else state_in[0]) # Radiance accumulator
        δL = mi.Spectrum(δL if δL is not None else 0) # Differential/adjoint radiance
        β = mi.Spectrum(1)                            # Path throughput weight (includes geometry terms)
        η = mi.Float(1)                               # Index of refraction
        active = mi.Bool(active)                      # Active SIMD lanes

        # Variables caching information from the previous bounce
        prev_ray        = mi.Ray3f(dr.detach(ray))
        prev_pi         = dr.zeros(mi.PreliminaryIntersection3f)
        prev_bsdf_pdf   = mi.Float(1.0)
        prev_bsdf_delta = mi.Bool(True)
        
        # Variables caching information from the current bounce
        pi = dr.zeros(mi.PreliminaryIntersection3f)

        # Output variables of the primal phase
        pi_first = dr.zeros(mi.PreliminaryIntersection3f)

        while dr.hint(active,
                      max_iterations=self.max_depth,
                      label="Path Replay Backpropagation (%s)" % mode.name):
            active_next = mi.Bool(active)

            # Trace the first intersection point if in primal phase, or re-use if in adjoint phase
            # At later intersections, pi is already set to pi_next at the end of this loop
            # TODO: This is a bit ugly, but tracing the first intersection point outside of the recorded
            #       loop does not work. It seems as if pi is not properly updated to pi_next.
            pi = dr.select(depth != 0, pi,
                           scene.ray_intersect_preliminary(ray, 
                                                           coherent=(depth == 0),
                                                           active=active_next) if primal else state_in[1])
            pi_first = dr.select(depth == 0, pi, pi_first)

            with dr.resume_grad(when=not primal):
                prev_si = prev_pi.compute_surface_interaction(dr.detach(prev_ray),
                                                              ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                                              active=(depth != 0) & active_next)
                
                si = pi.compute_surface_interaction(dr.detach(ray),
                                                    ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                                    active=active_next)

                # Since `ray` was detached when computing `si`, we need to recompute `si.wi` to follow `prev_si`
                prev_p = dr.select(depth == 0, ray.o, prev_si.p)
                si.wi  = dr.select(si.is_valid(), si.to_local(dr.normalize(prev_p - si.p)), si.wi)

            # Get the BSDF, potentially computes texture-space differentials
            bsdf = si.bsdf(ray)

            # ---------------------- Direct emission ----------------------

            # Hide the environment emitter if necessary
            if dr.hint(self.hide_emitters, mode='scalar'):
                active_next &= ~((depth == 0) & ~si.is_valid())

            # Compute MIS weight for emitter sample from previous bounce
            ds = mi.DirectionSample3f(scene, si=si, ref=prev_si)

            si_pdf = scene.pdf_emitter_direction(prev_si, ds, ~prev_bsdf_delta)
            
            with dr.resume_grad(when=not primal):
                D = solid_to_surface_reparam_det(si, prev_si.p, active=active_next)

                # Track how the radiance emitted from this point is warped by the determinant
                LD = dr.select(depth != 0, L * det_over_det(D), 0)       
            
            # Since D is contained in both pdfs it cancels in the MIS weight
            mis = mis_weight(
                prev_bsdf_pdf, # * D 
                si_pdf # * D
            )

            # Remember: β contains (determinant/pdf warping) = 1, but detached
            # β *= dr.detach(D) / dr.detach(D)

            with dr.resume_grad(when=not primal):
                Le = β * mis * ds.emitter.eval(si, active_next)


            # ---------------------- Emitter sampling ----------------------

            # Should we continue tracing to reach one more vertex?
            active_next &= (depth + 1 < self.max_depth) & si.is_valid()

            # Is emitter sampling even possible on the current vertex?
            active_em = active_next & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)

            # If so, randomly sample an emitter without derivative tracking.
            ds_em, em_weight = scene.sample_emitter_direction(si, sampler.next_2d(), True, active_em)
            active_em &= (ds_em.pdf != 0.0)

            ray_em = si.spawn_ray(ds_em.d)
            with dr.resume_grad(when=not primal):
                # Retrace the emitter ray so that the intersection point follows the emitter,
                # and to obtain an intersection point with the differentials si_em.dp_du and si_em.dp_dv
                # (only if the emitter is a surface)
                si_em = scene.ray_intersect(ray_em, 
                                            ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                            coherent=mi.Bool(False),
                                            active=active_em & mi.has_flag(ds_em.emitter.flags(), mi.EmitterFlags.Surface))


                # Compute the BSDF with an outgoing direction that follows the shapes
                # TODO: For DeltaPosition emitters (point/spot), this is not fully consistent 
                #       with `ad_threepoint` because ds_em.p is attached there but detached here.
                wo_em = si.to_local(dr.normalize(dr.select(si_em.is_valid(), si_em.p - si.p, ds_em.p - si.p)))
                bsdf_value_em, bsdf_pdf_em = bsdf.eval_pdf(bsdf_ctx, si, wo_em, active_em)
                
                # For non-surface, `si_em` will be invalid, in which case `D_em` is one, 
                # and the sample is processed as being a solid angle sample.
                D_em = solid_to_surface_reparam_det(si_em, si.p, active_em)

                if dr.hint(not primal, mode='scalar'):
                    # Evaluate the emitter with gradient tracking (with detached pdf), and transform the sample to a surface sample
                    em_val = scene.eval_emitter_direction(si, ds_em, active_em)
                    em_weight = dr.replace_grad(em_weight, dr.select((ds_em.pdf != 0), em_val / ds_em.pdf, 0)) * det_over_det(D_em)


            # Since D is contained in both pdfs it cancels in the MIS weight
            mis_em = dr.select(ds_em.delta, 1, mis_weight(ds_em.pdf, bsdf_pdf_em))

            with dr.resume_grad(when=not primal):
                Lr_dir = β * mis_em * bsdf_value_em * em_weight


            # ------------------ Detached BSDF sampling -------------------

            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                   sampler.next_1d(),
                                                   sampler.next_2d(),
                                                   active_next)

            # ---- Update loop variables based on current interaction -----

            L = (L + Le + Lr_dir) if primal else (L - Le - Lr_dir)

            η *= bsdf_sample.eta
            # Detached Sampling
            β *= bsdf_weight

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

            ray_next = si.spawn_ray(si.to_world(bsdf_sample.wo))

            pi_next = scene.ray_intersect_preliminary(ray_next,
                                                      coherent=mi.Bool(False),
                                                      active=active_next)

            # ------------------ Differential phase only ------------------

            if dr.hint(not primal, mode='scalar'):
                with dr.resume_grad():
                    # 'L' stores the indirectly reflected radiance at the
                    # current vertex but does not track parameter derivatives.
                    # The following addresses this by canceling the detached
                    # BSDF value and replacing it with an equivalent term that
                    # has derivative tracking enabled. (nit picking: the
                    # direct/indirect terminology isn't 100% accurate here,
                    # since there may be a direct component that is weighted
                    # via multiple importance sampling)
                    si_next = pi_next.compute_surface_interaction(ray_next,
                                                                  ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                                                  active=active_next)
                    
                    # Recompute 'wo' to propagate derivatives to cosine term
                    wo = si.to_local(dr.normalize(si_next.p - si.p))

                    # Re-evaluate BSDF * cos(theta) differentiably
                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active_next & si_next.is_valid())

                    # Detached version of the above term and inverse
                    bsdf_val_det = bsdf_weight * bsdf_sample.pdf
                    inv_bsdf_val_det = dr.select(bsdf_val_det != 0,
                                                 dr.rcp(bsdf_val_det), 0)

                    # Differentiable version of the reflected indirect
                    # radiance. Minor optional tweak: indicate that the primal
                    # value of the second term is always 1.
                    tmp = inv_bsdf_val_det * bsdf_val
                    tmp_replaced = dr.replace_grad(dr.ones(mi.Float, dr.width(tmp)), tmp) #FIXME
                    Lr_ind = L * tmp_replaced

                    # Differentiable Monte Carlo estimate of all contributions
                    Lo = Le + LD + Lr_dir + Lr_ind

                    attached_contrib = dr.flag(dr.JitFlag.VCallRecord) and not dr.grad_enabled(Lo)
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
                        dr.backward_from(δL * Lo)
                    else:
                        δL += dr.forward_to(Lo)

            # ------------------ Prepare next iteration ------------------
            # Information about the current vertex needed by the next iteration
            prev_bsdf_pdf = bsdf_sample.pdf
            prev_bsdf_delta = mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Delta)
            
            depth[si.is_valid()] += 1
            active = active_next

            prev_ray = ray
            prev_pi  = pi
            ray = ray_next
            pi  = pi_next

        return (
            L if primal else δL, # Radiance/differential radiance
            depth != 0,          # Ray validity flag for alpha blending
            [],                  # Empty tuple of AOVs.
            (L, pi_first),       # State for the differential phase
        )

mi.register_integrator("prb_threepoint", lambda props: PRBThreePointIntegrator(props))
