from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import RBIntegrator, solid_angle_to_area_jacobian

class BasicPRBIntegrator(RBIntegrator):
    r"""
    .. _integrator-prb_basic:

    Basic Path Replay Backpropagation (:monosp:`prb_basic`)
    ---------------------------------------------------------

    .. pluginparameters::

     * - max_depth
       - |int|
       - Specifies the longest path depth in the generated output image (where -1
         corresponds to :math:`\infty`). A value of 1 will only render directly
         visible light sources. 2 will lead to single-bounce (direct-only)
         illumination, and so on. (Default: 6)

     * - hide_emitters
       - |bool|
       - Hide directly visible emitters. (Default: no, i.e. |false|)

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
        See ``ADIntegrator.sample()`` for a description of this interface and
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
        pi = scene.ray_intersect_preliminary(ray,        # Current interaction
                                             coherent=True,
                                             reorder=False,
                                             active=active)

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
                    si_prev = pi_prev.compute_surface_interaction(ray_prev, ray_flags=mi.RayFlags.All)
                    # We should not account for the current interaction's motion
                    si_detach = dr.detach(si)
                    wi_global = dr.normalize(si_prev.p - si_detach.p)
                    si_wi_diff = si_detach.to_local(wi_global)
                    si.wi = dr.replace_grad(si.wi, si_wi_diff)

            # ---------------------- Direct emission ----------------------

            # Hide the environment emitter if necessary
            if dr.hint(self.hide_emitters, mode='scalar'):
                active_next &= ~((depth == 0) & ~si.is_valid())

            # Differentiable evaluation of intersected emitter / envmap
            with dr.resume_grad(when=not primal):
                Le = β * si.emitter(scene).eval(si, active_next)

            # Should we continue tracing to reach one more vertex?
            active_next &= (depth + 1 < self.max_depth) & si.is_valid()

            # Get the BSDF. Potentially computes texture-space differentials.
            bsdf = si.bsdf(ray)

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

            # ------------------ Find the next ineraction ------------------

            ray_next = si.spawn_ray(si.to_world(bsdf_sample.wo))
            pi_next = scene.ray_intersect_preliminary(ray_next,
                                                      coherent=False,
                                                      reorder=False,
                                                      active=active_next)

            # ------------------ Differential phase only ------------------

            if dr.hint(not primal, mode='scalar'):
                si_next = pi_next.compute_surface_interaction(ray_next,
                                                              ray_flags=mi.RayFlags.All,
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
                    Lr = L * dr.replace_grad(
                        1,
                        (bsdf_val / dr.detach(bsdf_val)) *
                        (J / dr.detach(J))
                    )

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
            L                    # State the for differential phase
        )

mi.register_integrator("prb_basic", lambda props: BasicPRBIntegrator(props))

del RBIntegrator
