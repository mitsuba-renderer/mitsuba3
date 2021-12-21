from __future__ import annotations # Delayed parsing of type annotations

import enoki as ek
import mitsuba

from .common import ADIntegrator

class BasicPRBIntegrator(ADIntegrator):
    """
    Basic Path Replay Backpropagation-style integrator *without* next event
    estimation, multiple importance sampling, Russian Roulette, and
    re-parameterization. The lack of all of these features means that gradients
    are relatively noisy and don't correctly account for visibility
    discontinuties. The lack of a Russian Roulette stopping criterion means
    that generated light paths may be unnecessarily long and costly to
    generate.

    This class is not meant to be used in practice, but merely exists to
    illustrate how a very basic rendering algorithm can be implemented in
    Python along with efficient forward/reverse-mode derivatives. See 'prb.py'
    for a more feature-complete Path Replay Backpropagation integrator.
    """

    def sample(self,
               mode: enoki.ADMode,
               scene: mitsuba.render.Scene,
               sampler: mitsuba.render.Sampler,
               ray: mitsuba.core.Ray3f,
               depth: mitsuba.core.UInt32,
               δL: Optional[mitsuba.core.Spectrum],
               state_in: Any,
               active: mitsuba.core.Bool) -> Tuple[mitsuba.core.Spectrum,
                                                   mitsuba.core.Bool, Any]:
        """
        See ``ADIntegrator.sample()`` for a description of this interface and
        the role of the various parameters and return values.
        """

        from mitsuba.core import Loop, Spectrum, Float, Bool, UInt32, Ray3f
        from mitsuba.render import BSDFContext

        primal = mode == ek.ADMode.Primal

        # --------------------- Configure loop state ----------------------

        # Copy input arguments to avoid mutating the caller's state
        ray = Ray3f(ray)
        depth, depth_initial = UInt32(depth), UInt32(depth)
        L = Spectrum(0 if primal else state_in)
        δL = Spectrum(δL if δL is not None else 0)
        active = Bool(active)
        β = Spectrum(1)
        bsdf_ctx = BSDFContext()

        # Record the following loop in its entirety
        loop = Loop(name="Path Replay Backpropagation (%s)" % mode.name,
                    state=lambda: (sampler, ray, depth, L, δL, β, active))

        while loop(active):
            # ---------------------- Direct emission ----------------------

            with ek.resume_grad(condition=not primal):
                # Capture π-dependence of intersection for a detached input ray
                si = scene.ray_intersect(ray)
                bsdf = si.bsdf(ray)

                # Differentiable evaluation of intersected emitter / envmap
                Le = β * si.emitter(scene).eval(si)

            # Detached BSDF sampling
            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                   sampler.next_1d(),
                                                   sampler.next_2d())

            # ---- Update loop variables based on current interaction -----

            L = L + Le if primal else L - Le
            ray = si.spawn_ray(si.to_world(bsdf_sample.wo))
            η *= bsdf_sample.eta
            β *= bsdf_weight

            # ------------------ Differential phase only ------------------

            if not primal:
                with ek.resume_grad():
                    # Recompute local 'wo' to propagate derivatives to cosine term
                    wo = si.to_local(ray.d)

                    # Re-evalute BRDF*cos(theta) differentiably
                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active)

                    # Recompute the reflected radiance
                    recip = bsdf_weight * bsdf_sample.pdf
                    Lr = L * bsdf_val * ek.select(ek.neq(recip, 0), ek.rcp(recip), 0)

                    # Estimated contribution of the current vertex
                    contrib = Le + Lr

                    # Propagate derivatives from/to 'contrib' based on 'mode'
                    if mode == ek.ADMode.Backward:
                        ek.backward_from(δL * contrib)
                    else:
                        ek.forward_to(contrib)
                        δL += ek.grad(contrib)

            # -------------------- Stopping criterion ---------------------

            depth[si.is_valid()] += 1
            active &= si.is_valid() & (depth < self.max_depth) & ek.any(ek.neq(β, 0))

        return (
            L if primal else δL,          # Radiance/differential radiance
            ek.neq(depth, depth_initial), # Ray valid flag
            L                             # State for differential phase
        )

mitsuba.render.register_integrator("prb_basic", lambda props:
                                   BasicPRBIntegrator(props))
