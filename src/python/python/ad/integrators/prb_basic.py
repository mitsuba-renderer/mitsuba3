from __future__ import annotations # Delayed parsing of type annotations

import enoki as ek
import mitsuba

from .common import ADIntegrator

class BasicPRBIntegrator(ADIntegrator):
    """
    Basic Path Replay Backpropagation-style integrator *without* next event
    estimation, multiple importance sampling, Russian Roulette, and
    re-parameterization. The lack of all of these features means that gradients
    are noisy and don't correctly account for visibility discontinuties. The
    lack of a Russian Roulette stopping criterion means that generated light
    paths may be unnecessarily long and costly to generate.

    This class is not meant to be used in practice, but merely exists to
    illustrate how a very basic rendering algorithm can be implemented in
    Python along with efficient forward/reverse-mode derivatives. See the file
    'prb.py' for a more feature-complete Path Replay Backpropagation
    integrator, and 'prb_reparam.py' for one that also handles visibility.
    """

    def sample(self,
               mode: enoki.ADMode,
               scene: mitsuba.render.Scene,
               sampler: mitsuba.render.Sampler,
               ray: mitsuba.core.Ray3f,
               δL: Optional[mitsuba.core.Spectrum],
               state_in: Optional[mitsuba.core.Spectrum],
               active: mitsuba.core.Bool,
               **kwargs # Absorbs unused arguments
    ) -> Tuple[mitsuba.core.Spectrum,
               mitsuba.core.Bool, mitsuba.core.Spectrum]:
        """
        See ``ADIntegrator.sample()`` for a description of this interface and
        the role of the various parameters and return values.
        """

        from mitsuba.core import Loop, Spectrum, Float, Bool, UInt32, Ray3f
        from mitsuba.render import BSDFContext

        # Rendering a primal image? (vs performing forward/reverse-mode AD)
        primal = mode == ek.ADMode.Primal

        # Standard BSDF evaluation context for path tracing
        bsdf_ctx = BSDFContext()

        # --------------------- Configure loop state ----------------------

        # Copy input arguments to avoid mutating the caller's state
        ray = Ray3f(ray)
        depth = UInt32(0)                          # Depth of current vertex
        L = Spectrum(0 if primal else state_in)    # Radiance accumulator
        δL = Spectrum(δL if δL is not None else 0) # Differential/adjoint radiance
        β = Spectrum(1)                            # Path throughput weight
        active = Bool(active)                      # Active SIMD lanes

        # Record the following loop in its entirety
        loop = Loop(name="Path Replay Backpropagation (%s)" % mode.name,
                    state=lambda: (sampler, ray, depth, L, δL, β, active))

        while loop(active):
            # ---------------------- Direct emission ----------------------

            # Compute a surface interaction that tracks derivatives arising
            # from differentiable shape parameters (position, normals, etc.)
            # In primal mode, this is just an ordinary ray tracing operation.

            with ek.resume_grad(when=not primal):
                si = scene.ray_intersect(ray)

                # Differentiable evaluation of intersected emitter / envmap
                Le = β * si.emitter(scene).eval(si)

            # Should we continue tracing to reach one more vertex?
            active_next = (depth + 1 < self.max_depth) & si.is_valid()

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
            active_next &= ek.any(ek.neq(β, 0))

            # ------------------ Differential phase only ------------------

            if not primal:
                with ek.resume_grad():
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
                    bsdf_val_det = bsdf_weight * bsdf_sample.pdf
                    inv_bsdf_val_det = ek.select(ek.neq(bsdf_val_det, 0),
                                                 ek.rcp(bsdf_val_det), 0)

                    # Differentiable version of the reflected radiance. Minor
                    # optional tweak: indicate that the primal value of the
                    # second term is 1.
                    Lr = L * ek.replace_grad(1, inv_bsdf_val_det * bsdf_val)

                    # Differentiable Monte Carlo estimate of all contributions
                    Lo = Le + Lr

                    # Propagate derivatives from/to 'Lo' based on 'mode'
                    if mode == ek.ADMode.Backward:
                        ek.backward_from(δL * Lo)
                    else:
                        δL += ek.forward_to(Lo)

            depth[si.is_valid()] += 1
            active = active_next

        return (
            L if primal else δL, # Radiance/differential radiance
            ek.neq(depth, 0),    # Ray validity flag for alpha blending
            L                    # State the for differential phase
        )

mitsuba.render.register_integrator("prb_basic", lambda props:
                                   BasicPRBIntegrator(props))
