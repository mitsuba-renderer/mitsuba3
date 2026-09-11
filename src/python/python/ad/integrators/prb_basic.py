from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import RBIntegrator, reattach_wi, reattach_wo

class BasicPRBIntegrator(RBIntegrator):
    r"""
    .. _integrator-prb_basic:

    Basic Path Replay Backpropagation (:monosp:`prb_basic`)
    -------------------------------------------------------

    .. pluginparameters::

     * - max_depth
       - |int|
       - Specifies the longest path depth in the generated output image (where -1
         corresponds to :math:`\infty`). A value of 1 will only render directly
         visible light sources. 2 will lead to single-bounce (direct-only)
         illumination, and so on. (Default: 6)

    Basic Path Replay Backpropagation-style integrator *without* next event
    estimation, multiple importance sampling, Russian Roulette, and
    projective sampling. The lack of all of these features means that gradients
    are noisy and don't correctly account for visibility discontinuities. The
    lack of a Russian Roulette stopping criterion means that generated light
    paths may be unnecessarily long and costly to generate.

    This class is not meant to be used in practice, but merely exists to
    illustrate how a very basic rendering algorithm can be implemented in
    Python along with efficient forward/reverse-mode derivatives. See the file
    ``prb.py`` for a more feature-complete Path Replay Backpropagation
    integrator, and ``prb_reparam.py`` for one that also handles visibility.

    .. warning::
        This integrator is not supported in variants which track polarization
        states.

    .. tabs::

        .. code-tab:: python

            'type': 'prb_basic',
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
        ray = mi.Ray3f(ray)                              # Current ray
        ray_prev = mi.Ray3f(ray)                         # Ray for the previous bounce
        depth = mi.UInt32(0)                             # Depth of current vertex
        L = mi.Spectrum(0 if primal else state_in)       # Radiance accumulator
        δL = mi.Spectrum(δL if δL is not None else 0)    # Differential/adjoint radiance
        β = mi.Spectrum(1)                               # Path throughput weight
        active = mi.Bool(active)                         # Active SIMD lanes
        pi_prev = dr.zeros(mi.PreliminaryIntersection3f) # Interaction of the previous bounce
        ray_mask = mi.UInt32(mi.RayMask.Primary)         # Mask of the current ray

        # Null tests fold to literals in scenes without null shapes
        has_null = scene.has_null_shapes()

        # The camera mask hides emitters marked as invisible
        pi = scene.ray_intersect_preliminary(ray,        # Current interaction
                                             coherent=True,
                                             reorder=False,
                                             active=active,
                                             ray_mask=mi.RayMask.Primary)

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
                    si_prev = scene.compute_surface_interaction(
                        ray_prev, pi_prev, ray_flags=mi.RayFlags.Minimal)
                    reattach_wi(si, si_prev.p)

            # ---------------------- Direct emission ----------------------

            # Differentiable evaluation of intersected emitter / envmap. The
            # lookup uses the mask of the ray that produced si.
            with dr.resume_grad(when=not primal):
                emitter = si.emitter(scene, ray_mask=ray_mask)
                Le = β * emitter.eval(si, active_next)

            # Get the BSDF
            bsdf = si.bsdf()

            # Continue tracing? Null crossings don't count towards max_depth
            depth_ok = depth + 1 < self.max_depth
            can_cross = bsdf.has_flag(mi.BSDFFlags.Null) if has_null else mi.Bool(False)
            active_next &= si.is_valid() & (depth_ok | can_cross)

            # ------------------ Detached BSDF sampling -------------------

            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                   sampler.next_1d(),
                                                   sampler.next_2d(),
                                                   active_next)

            # ---- Update loop variables based on current interaction -----

            L = L + Le if primal else L - Le
            β *= bsdf_weight

            # Don't run another iteration if the throughput has reached zero
            β_max = dr.max(mi.unpolarized_spectrum(β))
            active_next &= (β_max != 0)

            # Information about the current vertex needed by the next
            # iteration (unchanged by null crossings)
            null = bsdf_sample.is_null() if has_null else mi.Bool(False)
            scattered = si.is_valid() & ~null
            pi_prev[scattered]  = pi
            ray_prev[scattered] = ray
            ray_mask[scattered] = mi.RayMask.Secondary
            depth[scattered]   += 1
            active_next &= depth_ok | ~scattered

            # ------------------ Find the next ineraction ------------------

            ray_next = si.spawn_ray(si.to_world(bsdf_sample.wo))
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
                    Lr = L * dr.relative_grad(bsdf_val) * dr.relative_grad(J)

                    # Differentiable Monte Carlo estimate of all contributions
                    Lo = Le + Lr

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
            L                    # State the for differential phase
        )

mi.register_integrator("prb_basic", lambda props: BasicPRBIntegrator(props))

del RBIntegrator
