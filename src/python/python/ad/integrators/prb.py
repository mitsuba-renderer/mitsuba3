from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import RBIntegrator, mis_weight, reattach_wi, reattach_wo

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
        See `ADIntegrator.sample` for a description of this interface and
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

        # The camera mask hides emitters marked as invisible
        pi = scene.ray_intersect_preliminary(ray,        # Current interaction
                                             coherent=True,
                                             reorder=False,
                                             active=active,
                                             ray_mask=mi.RayMask.Primary)

        # Variables caching information from the previous bounce. Null
        # crossings are not path vertices and leave them unchanged.
        ray_prev        = mi.Ray3f(ray)
        pi_prev         = dr.zeros(mi.PreliminaryIntersection3f)
        si_prev         = dr.zeros(mi.SurfaceInteraction3f)
        bsdf_pdf_prev   = mi.Float(1.0)
        bsdf_delta_prev = mi.Bool(True)
        ray_mask        = mi.UInt32(mi.RayMask.Primary)

        # Null tests fold to literals in scenes without null shapes
        has_null = scene.has_null_shapes()

        while dr.hint(active,
                      max_iterations=self.max_depth,
                      label="Path Replay Backpropagation (%s)" % mode.name):
            active_next = mi.Bool(active)

            # Compute a surface interaction that tracks derivatives arising
            # from differentiable shape parameters (position, normals, etc.)
            # In primal mode, this is just an ordinary ray tracing operation.
            with dr.resume_grad(when=not primal):
                si = scene.compute_surface_interaction(ray, pi, ray_flags=mi.RayFlags.Default)

                # Recompute an attached si.wi to account for motion of the
                # previous surface interaction
                if (not primal) & mi.Bool(depth >= 1):
                    si_prev_diff = dr.replace_grad(si_prev, scene.compute_surface_interaction(
                        ray_prev, pi_prev, ray_flags=mi.RayFlags.Minimal))
                    reattach_wi(si, si_prev_diff.p)

            # Get the BSDF
            bsdf = si.bsdf()
            bsdf_flags = bsdf.flags()

            # ---------------------- Direct emission ----------------------

            # The emitter lookup uses the mask of the ray that produced si
            ds = mi.DirectionSample3f(scene, si=si, ref=si_prev,
                                      ray_mask=ray_mask)

            mis = mis_weight(
                bsdf_pdf_prev,
                scene.pdf_emitter_direction(si_prev, ds, ~bsdf_delta_prev)
            )

            with dr.resume_grad(when=not primal):
                Le = β * mis * ds.emitter.eval(si, active_next)

            # ---------------------- Emitter sampling ----------------------

            # Continue tracing? Null crossings don't count towards max_depth
            depth_ok = depth + 1 < self.max_depth
            can_cross = mi.has_flag(bsdf_flags, mi.BSDFFlags.Null) if has_null else mi.Bool(False)
            active_next &= si.is_valid() & (depth_ok | can_cross)

            # Is emitter sampling even possible on the current vertex?
            active_em = active_next & depth_ok & \
                        mi.has_flag(bsdf_flags, mi.BSDFFlags.Smooth)

            # If so, sample an emitter. The sample is detached, and its
            # weight is attached to the emitter and to the motion of 'si'.
            with dr.resume_grad(when=not primal):
                ds, em_weight = scene.sample_emitter_direction(
                    si, sampler.next_2d(), test_visibility=True, active=active_em)
                active_em &= (ds.pdf != 0.0)

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

            # Information about the current vertex needed by the next
            # iteration (unchanged by null crossings)
            null = bsdf_sample.is_null() if has_null else mi.Bool(False)
            scattered = si.is_valid() & ~null
            si_prev[scattered]         = dr.detach(si, True)
            pi_prev[scattered]         = pi
            ray_prev[scattered]        = ray
            bsdf_pdf_prev[scattered]   = bsdf_sample.pdf
            bsdf_delta_prev[scattered] = bsdf_sample.is_delta()
            ray_mask[scattered]        = mi.RayMask.Secondary
            depth[scattered]          += 1
            active_next &= depth_ok | ~scattered

            # ----------------- Find the next interaction -----------------

            pi_next = scene.ray_intersect_preliminary(ray_next,
                                                      coherent=False,
                                                      reorder=False,
                                                      active=active_next,
                                                      ray_mask=ray_mask)

            # ------------------ Differential phase only ------------------

            if dr.hint(not primal, mode='scalar'):
                si_next = scene.compute_surface_interaction(
                    ray_next, pi_next, ray_flags=mi.RayFlags.Minimal,
                    active=active_next)

                with dr.resume_grad():
                    # 'L' stores the reflected radiance at the current vertex
                    # but does not track parameter derivatives. The following
                    # addresses this by canceling the detached BSDF value and
                    # replacing it with an equivalent term that has derivative
                    # tracking enabled. The direction to the next vertex and
                    # the geometry term account for the motion of 'si'.
                    wo, J = reattach_wo(si, si_next, ray_next, active_next)

                    # Re-evaluate BSDF * cos(theta) differentiably. A null
                    # crossing continues with the null transmission instead.
                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active_next)
                    if dr.hint(has_null, mode='scalar'):
                        null &= active_next
                        bsdf_val[null] = bsdf.eval_null(si, null)

                    # Differentiable version of the reflected radiance.
                    Lr_ind = L * dr.relative_grad(bsdf_val) * dr.relative_grad(J)

                    # Differentiable Monte Carlo estimate of all contributions
                    Lo = Le + Lr_dir + Lr_ind

                    attached_contrib = dr.flag(dr.JitFlag.SymbolicCalls) and not dr.grad_enabled(Lo)
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

            active = active_next
            pi = pi_next
            ray = ray_next

        return (
            L if primal else δL, # Radiance/differential radiance
            depth != 0,          # Ray validity flag for alpha blending
            [],                  # Empty typle of AOVs
            L                    # State for the differential phase
        )

mi.register_integrator("prb", lambda props: PRBIntegrator(props))

del RBIntegrator
