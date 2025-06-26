from __future__ import annotations as __annotations__

import drjit as dr
import mitsuba as mi

from mitsuba.ad.integrators.common import ADIntegrator, mis_weight

class AutoDiffIntegrator(ADIntegrator):
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

        with dr.resume_grad():
            for iter in range(self.max_depth):
                active_next = mi.Bool(active)
    
                # Compute a surface interaction that tracks derivatives arising
                # from differentiable shape parameters (position, normals, etc.)
                # In primal mode, this is just an ordinary ray tracing operation.
                si = scene.ray_intersect(ray,
                                         ray_flags=mi.RayFlags.All,
                                         coherent=(depth == 0))

                # Get the BSDF, potentially computes texture-space differentials
                bsdf = si.bsdf(ray)
    
                # ---------------------- Direct emission ----------------------
    
                # Hide the environment emitter if necessary
                if self.hide_emitters:
                    active_next &= ~((depth == 0) & ~si.is_valid())
    
                # Compute MIS weight for emitter sample from previous bounce
                ds = mi.DirectionSample3f(scene, si=si, ref=prev_si)

                si_pdf = scene.pdf_emitter_direction(prev_si, ds, ~prev_bsdf_delta)
                dr.disable_grad(si_pdf)
                
                mis = mis_weight(
                    prev_bsdf_pdf,
                    si_pdf
                )
    
                Le = β * mis * ds.emitter.eval(si, active_next)
                L += Le
                
                # ---------------------- Detached Emitter sampling ----------------------
    
                # Should we continue tracing to reach one more vertex?
                active_next &= (depth + 1 < self.max_depth) & si.is_valid()

                # Is emitter sampling even possible on the current vertex?
                active_em = active_next & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)

                # If so, randomly sample an emitter without derivative tracking.
                with dr.suspend_grad():
                    ds, em_weight = scene.sample_emitter_direction(si, sampler.next_2d(), True, active_em)

                # The emitter sample is directly drawn from the surface of an emitter,
                # but this contradicts the definition of "sampling of directions": 
                # the sample position should move on the emitter surface when the ray origin (`si`) moves.
                # Therefore, retrace the corresponding ray to attach the intersection point.
                si_em       = scene.ray_intersect(si.spawn_ray(ds.d), active=active_em)
                ds_attached = mi.DirectionSample3f(scene, si_em, ref=si)
                ds_attached.pdf, ds_attached.delta, ds_attached.uv, ds_attached.n = (ds.pdf, ds.delta, si_em.uv, si_em.n)
                ds = ds_attached

                # The sampled emitter direction and the pdf must be detached
                # Recompute `em_weight = em_val / ds.pdf` with only `em_val` attached
                dr.disable_grad(ds.d, ds.pdf)
                em_val    = scene.eval_emitter_direction(si, ds, active_em)
                em_weight = dr.replace_grad(em_weight, dr.select((ds.pdf != 0), em_val / ds.pdf, 0))

                active_em &= (ds.pdf != 0)

                # Evaluate BSDF * cos(theta) differentiably (and detach the bsdf pdf)
                wo = si.to_local(ds.d)
                bsdf_value_em, bsdf_pdf_em = bsdf.eval_pdf(bsdf_ctx, si, wo, active_em)
                
                dr.disable_grad(bsdf_pdf_em)
                
                mis_em = dr.select(ds.delta, 1, mis_weight(ds.pdf, bsdf_pdf_em))
                Lr_dir = β * mis_em * bsdf_value_em * em_weight
                L += Lr_dir
                
                # ------------------ Detached BSDF sampling -------------------
    

                bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                    sampler.next_1d(),
                                                    sampler.next_2d(),
                                                    active_next)
                
                # The sampled bsdf direction and the pdf must be detached
                # Recompute `bsdf_weight = bsdf_val / bsdf_sample.pdf` with only `bsdf_val` attached
                dr.disable_grad(bsdf_sample.wo, bsdf_sample.pdf)
                bsdf_val    = bsdf.eval(bsdf_ctx, si, bsdf_sample.wo, active_next)
                bsdf_weight = dr.replace_grad(bsdf_weight, dr.select((bsdf_sample.pdf != 0), bsdf_val / bsdf_sample.pdf, 0))
    
                # ---- Update loop variables based on current interaction -----
    
                wo_world = si.to_world(bsdf_sample.wo)
                # The direction in *world space* is detached
                dr.disable_grad(wo_world)

                ray = si.spawn_ray(wo_world) 
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
    
        return (
            L,                   # Radiance/differential radiance
            (depth != 0),    # Ray validity flag for alpha blending
            [],                  # Empty typle of AOVs
            L
        )

mi.register_integrator("ad", lambda props: AutoDiffIntegrator(props))
