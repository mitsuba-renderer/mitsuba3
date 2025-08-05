from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import ADIntegrator, mis_weight, solid_angle_to_area_jacobian

class DirectPyIntegrator(ADIntegrator):
    def __init__(self, props):
        super().__init__(props)

        # Override the max depth to 2 since this is a direct illumination
        # integrator
        self.max_depth = 2
        self.rr_depth = 2

    def sample(self,
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               ray: mi.Ray3f,
               active: mi.Bool,
               **kwargs # Absorbs unused arguments
    ) -> Tuple[mi.Spectrum, mi.Bool, List[mi.Float], Any]:
        """
        See ``PSIntegrator.sample()`` for a description of this interface and
        the role of the various parameters and return values.
        """
        del kwargs  # Unused

        # Rendering a primal image? (vs performing forward/reverse-mode AD)
        primal = mode == dr.ADMode.Primal

        with dr.suspend_grad(when=not primal):

            # Standard BSDF evaluation context
            bsdf_ctx = mi.BSDFContext()

            L = mi.Spectrum(0)

            # ---------------------- Direct emission ----------------------

            with dr.resume_grad(when=not primal):
                si = scene.ray_intersect(ray, ray_flags=mi.RayFlags.All,
                             coherent=True, active=active)

            active_next = active & si.is_valid() & (self.max_depth > 1)

            # Get the BSDF
            bsdf = si.bsdf(ray)

            # ---------------------- Emitter sampling ----------------------

            # Is emitter sampling possible on the current vertex?
            active_em_ = active_next & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)

            # If so, pick an emitter and sample a detached emitter direction
            ds_em, emitter_val = scene.sample_emitter_direction(
                si, sampler.next_2d(active_em_), test_visibility=True, active=active_em_)
            active_em = active_em_ & (ds_em.pdf != 0.0)

            with dr.resume_grad(when=not primal):
                # Re-compute some values with AD attached only in differentiable
                # phase
                if dr.hint(not primal, mode='scalar'):
                    is_surface = mi.has_flag(ds_em.emitter.flags(), mi.EmitterFlags.Surface)
                    is_infinite = mi.has_flag(ds_em.emitter.flags(), mi.EmitterFlags.Infinite)
                    is_spatially_varying = mi.has_flag(ds_em.emitter.flags(), mi.EmitterFlags.SpatiallyVarying)

                    ## For textured area lights, we need to do a differentiable
                    ## ray intersection to track UV changes
                    textured_area_em = active_em & is_surface & is_spatially_varying

                    # Visibility is already accounted for, we can move the ray
                    # origin closer to the surface
                    ray_em = si.spawn_ray_to(ds_em.p)
                    ray_em.o = dr.fma(ray_em.d, ray_em.maxt, ray_em.o)
                    ray_em.maxt = dr.largest(ray_em.maxt)
                    si_em = scene.ray_intersect(ray_em,
                                                ray_flags=mi.RayFlags.All,
                                                coherent=False,
                                                reorder=False,
                                                active=textured_area_em)

                    # Re-attach gradients for the the `ds_em` struct
                    ds_em_diff = mi.DirectionSample3f(scene, si_em, si)
                    ds_em_diff = dr.select(textured_area_em, ds_em_diff, dr.zeros(mi.DirectionSample3f))
                    ds_em_diff.d = dr.select(~is_infinite, ds_em_diff.d, ds_em.d)
                    ds_em = dr.replace_grad(ds_em, ds_em_diff)

                    # If the current interaction point is moving, we need
                    # to differentiate the solid angle to surface area
                    # reparameterization.
                    J = solid_angle_to_area_jacobian(
                        si.p, dr.detach(ds_em.p), dr.detach(ds_em.n), active_em & is_surface
                    )

                    # Re-compute attached `emitter_val` to enable emitter optimization
                    spec_em = scene.eval_emitter_direction(si, ds_em, active_em)
                    emitter_val_diff = dr.select(
                        active_em,
                        (spec_em / dr.detach(ds_em.pdf)) * (J / dr.detach(J)),
                        0
                    )
                    emitter_val = dr.replace_grad(emitter_val, emitter_val_diff)

                # Evaluate the BSDF (foreshortening term included)
                wo = si.to_local(ds_em.d)
                bsdf_val, bsdf_pdf = bsdf.eval_pdf(bsdf_ctx, si, wo, active_em)

                # Compute the detached MIS weight for the emitter sample
                mis_em = dr.select(ds_em.delta, 1.0, mis_weight(ds_em.pdf, bsdf_pdf))

                L[active_em] += emitter_val * bsdf_val * mis_em

            # ---------------------- BSDF sampling ----------------------

            # Perform detached BSDF sampling
            sample_bsdf, weight_bsdf = bsdf.sample(bsdf_ctx, si, sampler.next_1d(active_next),
                                                   sampler.next_2d(active_next), active_next)

            active_bsdf = active_next & dr.any(weight_bsdf != 0.0)
            delta_bsdf = mi.has_flag(sample_bsdf.sampled_type, mi.BSDFFlags.Delta)

            # Construct the BSDF sampled ray
            ray_bsdf = si.spawn_ray(si.to_world(sample_bsdf.wo))

            with dr.resume_grad(when=not primal):
                # Trace the BSDF sampled ray
                si_bsdf = scene.ray_intersect(
                    ray_bsdf, ray_flags=mi.RayFlags.All, coherent=False, active=active_bsdf)

                if dr.hint(not primal, mode='scalar'):
                    si_bsdf_detached = dr.detach(si_bsdf)

                    # Re-compute `weight_bsdf` with AD attached only in
                    # differentiable phase
                    J = solid_angle_to_area_jacobian(
                        si.p, si_bsdf_detached.p, si_bsdf_detached.n, active_next & si_bsdf.is_valid()
                    )

                    # Recompute 'wo' to propagate derivatives to cosine term
                    wo_world_diff = dr.normalize(si_bsdf_detached.p - si.p)
                    wo_world = dr.replace_grad(
                        ray_bsdf.d,
                        dr.select(si_bsdf.is_valid(), wo_world_diff, ray_bsdf.d)
                    )
                    wo = si.to_local(wo_world)


                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active_bsdf)
                    weight_bsdf = dr.replace_grad(weight_bsdf,
                        (bsdf_val / dr.detach(bsdf_val)) *
                        (J / dr.detach(J))
                    )

                    # Re-attach si_bsdf.wi if si.p was moving
                    wi_global = dr.normalize(si.p - si_bsdf_detached.p)
                    si.wi = dr.replace_grad(si.wi, si_bsdf_detached.to_local(wi_global))

                # Evaluate the emitter
                L_bsdf = si_bsdf.emitter(scene, active_bsdf).eval(si_bsdf, active_bsdf)

                # Compute the detached MIS weight for the BSDF sample
                ds_bsdf = mi.DirectionSample3f(scene, si=si_bsdf, ref=si)
                pdf_emitter = scene.pdf_emitter_direction(ref=si, ds=ds_bsdf, active=active_bsdf & ~delta_bsdf)
                mis_bsdf = dr.select(delta_bsdf, 1.0, mis_weight(sample_bsdf.pdf, pdf_emitter))

                L[active_bsdf] += L_bsdf * weight_bsdf * mis_bsdf

            return L, active, [], None


mi.register_integrator("direct_py", lambda props: DirectPyIntegrator(props))

del ADIntegrator
