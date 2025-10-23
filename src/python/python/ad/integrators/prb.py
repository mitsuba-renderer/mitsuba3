from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import RBIntegrator, mis_weight, solid_angle_to_area_jacobian

class PRBIntegrator(RBIntegrator):
    r"""
    .. _integrator-prb:

    Path Replay Backpropagation (:monosp:`prb`)
    -------------------------------------------

    .. pluginparameters::

     * - max_depth
       - |int|
       - Specifies the longest path depth in the generated output image (where -1
         corresponds to :math:`\infty`). A value of 1 will only render directly
         visible light sources. 2 will lead to single-bounce (direct-only)
         illumination, and so on. (Default: 6)

     * - rr_depth
       - |int|
       - Specifies the path depth, at which the implementation will begin to use
         the *russian roulette* path termination criterion. For example, if set to
         1, then path generation many randomly cease after encountering directly
         visible surfaces. (Default: 5)

     * - hide_emitters
       - |bool|
       - Hide directly visible emitters. (Default: no, i.e. |false|)

    This plugin implements a basic Path Replay Backpropagation (PRB) integrator
    with the following properties:

    - Emitter sampling (a.k.a. next event estimation).

    - Russian Roulette stopping criterion.

    - No projective sampling. This means that the integrator cannot be used for
      shape optimization (it will return incorrect/biased gradients for
      geometric parameters like vertex positions.)

    - Detached sampling. This means that the properties of ideal specular
      objects (e.g., the IOR of a glass vase) cannot be optimized.

    See ``prb_basic.py`` for an even more reduced implementation that removes
    the first two features.

    See the papers :cite:`Vicini2021` and :cite:`Zeltner2021MonteCarlo`
    for details on PRB, attached/detached sampling.

    .. warning::
        This integrator is not supported in variants which track polarization
        states.

    .. tabs::

        .. code-tab:: python

            'type': 'prb',
            'max_depth': 8
    """

    @dr.syntax
    def sample(self,
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               ray: mi.Ray3f,
               δL: Optional[mi.Spectrum],
               state_in: Optional[mi.Spectrum],
               active: mi.Bool,
               **kwargs # Absorbs unused arguments
    ) -> Tuple[mi.Spectrum, mi.Bool, List[mi.Float], mi.Spectrum]:
        """
        See ``ADIntegrator.sample()`` for a description of this interface and
        the role of the various parameters and return values.
        """

        # Rendering a primal image? (vs performing forward/reverse-mode AD)
        primal = mode == dr.ADMode.Primal

        # Standard BSDF evaluation context for path tracing
        bsdf_ctx = mi.BSDFContext()

        # --------------------- Configure loop state ----------------------

        # Copy input arguments to avoid mutating the caller's state
        ray = mi.Ray3f(dr.detach(ray))                   # Current ray
        depth = mi.UInt32(0)                             # Depth of current vertex
        L = mi.Spectrum(0 if primal else state_in)       # Radiance accumulator
        δL = mi.Spectrum(δL if δL is not None else 0)    # Differential/adjoint radiance
        β = mi.Spectrum(1)                               # Path throughput weight
        η = mi.Float(1)                                  # Index of refraction
        active = mi.Bool(active)                         # Active SIMD lanes
        pi = scene.ray_intersect_preliminary(ray,        # Current interaction
                                             coherent=True,
                                             reorder=False,
                                             active=active)

        # Variables caching information from the previous bounce
        ray_prev        = mi.Ray3f(ray)
        pi_prev         = dr.zeros(mi.PreliminaryIntersection3f)
        si_prev         = dr.zeros(mi.SurfaceInteraction3f)
        bsdf_pdf_prev   = mi.Float(1.0)
        bsdf_delta_prev = mi.Bool(True)

        # ---------------------- Hide area emitters ----------------------

        if dr.hint(self.hide_emitters, mode='scalar'):
            # Did we hit an area emitter? If so, skip all area emitters along this ray
            skip_emitters = pi.is_valid() & (pi.shape.emitter() != None) & active
            si_skip = pi.compute_surface_interaction(ray, mi.RayFlags.Minimal, skip_emitters)
            ray_skip = si_skip.spawn_ray(ray.d)
            pi_after_skip = self.skip_area_emitters(scene, ray_skip, True, skip_emitters)
            pi[skip_emitters] = pi_after_skip

        while dr.hint(active,
                      max_iterations=self.max_depth,
                      label="Path Replay Backpropagation (%s)" % mode.name):
            active_next = mi.Bool(active)

            # Compute a surface interaction that tracks derivatives arising
            # from differentiable shape parameters (position, normals, etc.)
            # In primal mode, this is just an ordinary ray tracing operation.
            with dr.resume_grad(when=not primal):
                si = pi.compute_surface_interaction(ray, ray_flags=mi.RayFlags.All)

                # Recompute an attached si.wi to account for motion of the
                # previous surface interaction
                if (not primal) & mi.Bool(depth >= 1):
                    si_prev_diff = pi_prev.compute_surface_interaction(
                        ray_prev, ray_flags=mi.RayFlags.Minimal
                    )
                    si_prev = dr.replace_grad(si_prev, si_prev_diff)
                    si_detached = dr.detach(si) # Ignore motion of current point
                    wi_global = dr.normalize(si_prev.p - si_detached.p)
                    si.wi = dr.replace_grad(si.wi, si_detached.to_local(wi_global))

            # Get the BSDF, potentially computes texture-space differentials
            bsdf = si.bsdf(ray)

            # ---------------------- Direct emission ----------------------

            # Hide the environment emitter if necessary
            if dr.hint(self.hide_emitters, mode='scalar'):
                active_next &= ~((depth == 0) & ~si.is_valid())

            # Compute MIS weight for emitter sample from previous bounce
            ds = mi.DirectionSample3f(scene, si=si, ref=si_prev)

            mis = mis_weight(
                bsdf_pdf_prev,
                scene.pdf_emitter_direction(si_prev, ds, ~bsdf_delta_prev)
            )

            with dr.resume_grad(when=not primal):
                Le = β * mis * ds.emitter.eval(si, active_next)

            # ---------------------- Emitter sampling ----------------------

            # Should we continue tracing to reach one more vertex?
            active_next &= (depth + 1 < self.max_depth) & si.is_valid()

            # Is emitter sampling even possible on the current vertex?
            active_em = active_next & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)

            # If so, randomly sample an emitter without derivative tracking.
            ds, em_weight = scene.sample_emitter_direction(
                si, sampler.next_2d(), True, active_em)
            active_em &= (ds.pdf != 0.0)

            with dr.resume_grad(when=not primal):
                if dr.hint(not primal, mode='scalar'):
                    is_surface = mi.has_flag(ds.emitter.flags(), mi.EmitterFlags.Surface)
                    is_infinite = mi.has_flag(ds.emitter.flags(), mi.EmitterFlags.Infinite)
                    is_spatially_varying = mi.has_flag(ds.emitter.flags(), mi.EmitterFlags.SpatiallyVarying)

                    # For textured area lights, we need to track UV changes on
                    # the emitter if it is moving
                    textured_area_em = active_em & is_surface & is_spatially_varying
                    ray_em = si.spawn_ray_to(ds.p)
                    # Move ray origin closer, visibibliy is already accounted for
                    ray_em.o = dr.fma(ray_em.d, ray_em.maxt, ray_em.o)
                    ray_em.maxt = dr.largest(ray_em.maxt)
                    si_em = scene.ray_intersect(ray_em, textured_area_em)

                    # Re-attach gradients for the the `ds` struct
                    ds_diff = mi.DirectionSample3f(scene, si_em, si)
                    ds_diff = dr.select(textured_area_em, ds_diff, dr.zeros(mi.DirectionSample3f))
                    ds_diff.d = dr.select(textured_area_em, ds_diff.d, dr.normalize(ds.p - si.p))
                    ds_diff.d = dr.select(~is_infinite, ds_diff.d, ds.d)
                    ds = dr.replace_grad(ds, ds_diff)

                    # If the current interaction point is moving, we need
                    # to differentiate the solid angle to surface area
                    # reparameterization.
                    J = solid_angle_to_area_jacobian(
                        si.p, dr.detach(ds.p), dr.detach(ds.n), active_em & is_surface
                    )

                    # Given the detached emitter sample, *recompute* its
                    # contribution with AD to enable light source optimization
                    em_val_diff = scene.eval_emitter_direction(si, ds, active_em)
                    em_weight_diff = (em_val_diff / dr.detach(ds.pdf)) * (J / dr.detach(J))
                    em_weight = dr.replace_grad(em_weight, em_weight_diff)

                # Evaluate BSDF * cos(theta) differentiably
                wo = si.to_local(ds.d)
                bsdf_value_em, bsdf_pdf_em = bsdf.eval_pdf(bsdf_ctx, si, wo, active_em)
                mis_em = dr.select(ds.delta, 1, mis_weight(ds.pdf, bsdf_pdf_em))
                Lr_dir = β * mis_em * bsdf_value_em * em_weight

            # ------------------ Detached BSDF sampling -------------------

            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                   sampler.next_1d(),
                                                   sampler.next_2d(),
                                                   active_next)

            # ---- Update loop variables based on current interaction -----

            L = (L + Le + Lr_dir) if primal else (L - Le - Lr_dir)
            ray_next = si.spawn_ray(si.to_world(bsdf_sample.wo))
            η *= bsdf_sample.eta
            β *= bsdf_weight

            # Information about the current vertex needed by the next iteration

            si_prev = dr.detach(si, True)
            bsdf_pdf_prev = bsdf_sample.pdf
            bsdf_delta_prev = mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Delta)

            # -------------------- Stopping criterion ---------------------

            # Don't run another iteration if the throughput has reached zero
            β_max = dr.max(mi.unpolarized_spectrum(β))
            active_next &= (β_max != 0)

            # Russian roulette stopping probability (must cancel out ior^2
            # to obtain unitless throughput, enforces a minimum probability)
            rr_prob = dr.minimum(β_max * η**2, .95)

            # Apply only further along the path since, this introduces variance
            rr_active = depth >= self.rr_depth
            β[rr_active] *= dr.rcp(rr_prob)
            rr_continue = sampler.next_1d() < rr_prob
            active_next &= ~rr_active | rr_continue

            # ----------------- Find the next interaction -----------------

            pi_next = scene.ray_intersect_preliminary(ray_next,
                                                      coherent=False,
                                                      reorder=False,
                                                      active=active_next)

            # ------------------ Differential phase only ------------------

            if dr.hint(not primal, mode='scalar'):
                si_next = pi_next.compute_surface_interaction(ray_next,
                                                              ray_flags=mi.RayFlags.Minimal,
                                                              active=active_next)

                with dr.resume_grad():
                    # If the current interaction point is moving, we need
                    # to differentiate the solid angle to surface area
                    # reparameterization.
                    J = solid_angle_to_area_jacobian(
                        si.p, si_next.p, si_next.n, active_next & si_next.is_valid()
                    )

                    # 'L' stores the reflected radiance at the current vertex
                    # but does not track parameter derivatives. The following
                    # addresses this by canceling the detached BSDF value and
                    # replacing it with an equivalent term that has derivative
                    # tracking enabled.

                    # Recompute 'wo' to propagate derivatives to cosine term
                    wo_world_diff = dr.normalize(si_next.p - si.p)
                    wo_world = dr.replace_grad(
                        ray_next.d,
                        dr.select(si_next.is_valid(), wo_world_diff, ray_next.d)
                    )
                    wo = si.to_local(wo_world)

                    # Re-evaluate BSDF * cos(theta) differentiably
                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active_next)

                    # Differentiable version of the reflected radiance.
                    Lr_ind = L * dr.replace_grad(
                        1,
                        (bsdf_val / dr.detach(bsdf_val)) *
                        (J / dr.detach(J))
                    )

                    # Differentiable Monte Carlo estimate of all contributions
                    Lo = Le + Lr_dir + Lr_ind

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

            # ----------- Reorder threads for the next iteration --------

            # hint layout: [shape ID (bits 1–31) | active flag (LSB)]
            reorder_hint = dr.reinterpret_array(mi.UInt32, pi_next.shape)
            reorder_hint = (reorder_hint << 1) | dr.select(active_next, 1, 0)
            depth = dr.reorder_threads(reorder_hint, 16, depth)

            # ------------------ Update loop variables ------------------

            depth[si.is_valid()] += 1
            active = active_next

            pi_prev = pi
            pi = pi_next
            ray_prev = ray
            ray = ray_next

        return (
            L if primal else δL, # Radiance/differential radiance
            depth != 0,          # Ray validity flag for alpha blending
            [],                  # Empty typle of AOVs
            L                    # State for the differential phase
        )

mi.register_integrator("prb", lambda props: PRBIntegrator(props))

del RBIntegrator
