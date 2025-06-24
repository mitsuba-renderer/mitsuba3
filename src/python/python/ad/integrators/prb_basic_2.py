from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import RBIntegrator

class BasicPRB2Integrator(RBIntegrator):
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
        ray = mi.Ray3f(ray)
        ray_prev = mi.Ray3f(ray)
        depth = mi.UInt32(0)                          # Depth of current vertex
        L = mi.Spectrum(0 if primal else state_in)    # Radiance accumulator
        δL = mi.Spectrum(δL if δL is not None else 0) # Differential/adjoint radiance
        β = mi.Spectrum(1)                            # Path throughput weight
        active = mi.Bool(active)                      # Active SIMD lanes
        pi = scene.ray_intersect_preliminary(ray,
                                             coherent=True,
                                             reorder=False,
                                             active=active)
        pi_prev = dr.zeros(mi.PreliminaryIntersection3f)

        while dr.hint(active,
                      max_iterations=self.max_depth,
                      label="Path Replay Backpropagation (%s)" % mode.name):
            active_next = mi.Bool(active)

            # Compute a surface interaction that tracks derivatives arising
            # from differentiable shape parameters (position, normals, etc.)
            # In primal mode, this is just an ordinary ray tracing operation.
            with dr.resume_grad(when=not primal):
                si = pi.compute_surface_interaction(ray, ray_flags=mi.RayFlags.All)

                if (not primal) & mi.Bool(depth >= 1):
                    # Recompute an attached si.wi
                    si_prev = pi_prev.compute_surface_interaction(
                        ray_prev, ray_flags=mi.RayFlags.All)
                    si_detached = dr.detach(si)
                    si.wi = dr.replace_grad(si.wi,
                        si_detached.to_local(dr.normalize(si_prev.p - dr.detach(si.p)))
                    )

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
            ray_next = si.spawn_ray(si.to_world(bsdf_sample.wo))
            β *= bsdf_weight

            # Don't run another iteration if the throughput has reached zero
            active_next &= dr.any(β != 0)

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
                    # 'L' stores the reflected radiance at the current vertex
                    # but does not track parameter derivatives. The following
                    # addresses this by canceling the detached BSDF value and
                    # replacing it with an equivalent term that has derivative
                    # tracking enabled.

                    d = si_next.p - si.p
                    d_squared = dr.squared_norm(d)
                    d_normalized = dr.normalize(d)

                    cos_theta = dr.abs_dot(si_next.n, d_normalized)
                    G2 = cos_theta / d_squared
                    G2 = dr.select(active_next & si_next.is_valid(), G2, 1) #FIXME Think about envmaps later

                    # Recompute 'wo' to propagate derivatives to cosine term
                    wo = si.to_local(d_normalized)

                    # Re-evaluate BSDF * cos(theta) differentiably
                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active_next)

                    # Detached version of the above term and inverse
                    bsdf_val_detach = dr.detach(bsdf_val)
                    inv_bsdf_val_detach = dr.select(bsdf_val_detach != 0,
                                                    dr.rcp(bsdf_val_detach), 0)

                    # Differentiable version of the reflected radiance. Minor
                    # optional tweak: indicate that the primal value of the
                    # second term is 1.
                    Lr = L * dr.replace_grad(
                        1,
                        inv_bsdf_val_detach * bsdf_val *
                        G2 / dr.detach(G2)
                    )

                    # Differentiable Monte Carlo estimate of all contributions
                    Lo = Le + Lr

                    # Propagate derivatives from/to 'Lo' based on 'mode'
                    if dr.hint(mode == dr.ADMode.Backward, mode='scalar'):
                        dr.backward_from(δL * Lo)
                    else:
                        δL += dr.forward_to(Lo)

            reorder_hint = dr.reinterpret_array(mi.UInt32, pi_next.shape)
            reorder_hint = (reorder_hint << 1) | dr.select(active_next, 1, 0)
            depth = dr.reorder_threads(reorder_hint, 16, depth)

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

mi.register_integrator("prb_basic_2", lambda props: BasicPRB2Integrator(props))

del RBIntegrator
