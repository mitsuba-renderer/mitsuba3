from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import RBIntegrator

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
        depth = mi.UInt32(0)                          # Depth of current vertex
        L = mi.Spectrum(0 if primal else state_in)    # Radiance accumulator
        δL = mi.Spectrum(δL if δL is not None else 0) # Differential/adjoint radiance
        β = mi.Spectrum(1)                            # Path throughput weight
        active = mi.Bool(active)                      # Active SIMD lanes

        while dr.hint(active,
                      max_iterations=self.max_depth,
                      label="Path Replay Backpropagation (%s)" % mode.name):
            active_next = mi.Bool(active)

            # ---------------------- Direct emission ----------------------

            # Compute a surface interaction that tracks derivatives arising
            # from differentiable shape parameters (position, normals, etc.)
            # In primal mode, this is just an ordinary ray tracing operation.
            with dr.resume_grad(when=not primal):
                si = scene.ray_intersect(ray)

            # Hide the environment emitter if necessary
            if self.hide_emitters:
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
            ray = si.spawn_ray(si.to_world(bsdf_sample.wo))
            β *= bsdf_weight

            # Don't run another iteration if the throughput has reached zero
            active_next &= dr.any(β != 0)

            # ------------------ Differential phase only ------------------

            if not primal:
                with dr.resume_grad():
                    # 'L' stores the reflected radiance at the current vertex
                    # but does not track parameter derivatives. The following
                    # addresses this by canceling the detached BSDF value and
                    # replacing it with an equivalent term that has derivative
                    # tracking enabled.

                    # Recompute 'wo' to propagate derivatives to cosine term
                    wo = si.to_local(ray.d)

                    # Re-evaluate BSDF * cos(theta) differentiably
                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active_next)

                    # Detached version of the above term and inverse
                    bsdf_val_detach = bsdf_weight * bsdf_sample.pdf
                    inv_bsdf_val_detach = dr.select(bsdf_val_detach != 0,
                                                    dr.rcp(bsdf_val_detach), 0)

                    # Differentiable version of the reflected radiance. Minor
                    # optional tweak: indicate that the primal value of the
                    # second term is 1.
                    Lr = L * dr.replace_grad(1, inv_bsdf_val_detach * bsdf_val)

                    # Differentiable Monte Carlo estimate of all contributions
                    Lo = Le + Lr

                    # Propagate derivatives from/to 'Lo' based on 'mode'
                    if mode == dr.ADMode.Backward:
                        dr.backward_from(δL * Lo)
                    else:
                        δL += dr.forward_to(Lo)

            depth[si.is_valid()] += 1
            active = active_next

        return (
            L if primal else δL, # Radiance/differential radiance
            depth != 0,          # Ray validity flag for alpha blending
            [],                  # Empty typle of AOVs
            L                    # State the for differential phase
        )

mi.register_integrator("prb_basic", lambda props: BasicPRBIntegrator(props))

del RBIntegrator
