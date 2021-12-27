from __future__ import annotations # Delayed parsing of type annotations

import enoki as ek
import mitsuba

from .common import ADIntegrator, mis_weight

class PRBIntegrator(ADIntegrator):
    """
    This class implements a basic Path Replay Backpropagation (PRB) integrator
    with the following properties:

    - Emitter sampling (a.k.a. next event estimation).

    - Russian Roulette stopping criterion.

    - No reparameterization. This means that the integrator cannot be used for
      shape optimization (it will return incorrect/biased gradients for
      geometric parameters like vertex positions.)

    - Detached sampling. This means that the properties of ideal specular
      objects (e.g., the IOR of a glass vase) cannot be optimized.

    See 'prb_basic.py' for an even more reduced implementation that removes
    the first two features.

    See the papers

      "Path Replay Backpropagation: Differentiating Light Paths using
       Constant Memory and Linear Time" (Proceedings of SIGGRAPH'21)
       by Delio Vicini, Sébastien Speierer, and Wenzel Jakob

    and

      "Monte Carlo Estimators for Differential Light Transport"
      (Proceedings of SIGGRAPH'21) by Tizan Zeltner, Sébastien Speierer,
      Iliyan Georgiev, and Wenzel Jakob

    for details on PRB, attached/detached sampling, and reparameterizations.
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
        from mitsuba.render import SurfaceInteraction3f, DirectionSample3f, \
            BSDFContext, BSDFFlags, RayFlags, has_flag

        primal = mode == ek.ADMode.Primal

        # --------------------- Configure loop state ----------------------

        # Copy input arguments to avoid mutating the caller's state

        ray = Ray3f(ray)
        depth, depth_initial = UInt32(depth), UInt32(depth)
        L = Spectrum(0 if primal else state_in)
        δL = Spectrum(δL if δL is not None else 0)
        active = Bool(active)
        β = Spectrum(1)
        η = Float(1)

        # Variables caching information from the previous bounce
        prev_si         = ek.zero(SurfaceInteraction3f)
        prev_bsdf_pdf   = Float(1.0)
        prev_bsdf_delta = Bool(True)

        bsdf_ctx = BSDFContext()

        # Record the following loop in its entirety
        loop = Loop(name="Path Replay Backpropagation (%s)" % mode.name,
                    state=lambda: (sampler, ray, depth, L, δL, β, η, active,
                                   prev_si, prev_bsdf_pdf, prev_bsdf_delta))

        while loop(active):
            with ek.resume_grad(condition=not primal):
                # Capture π-dependence of intersection for a detached input ray
                si = scene.ray_intersect(ray,
                                         ray_flags=RayFlags.All,
                                         coherent=ek.eq(depth, 0))
                bsdf = si.bsdf(ray)

            # ---------------------- Direct emission ----------------------

            # Compute MIS weight for emitter sample from previous bounce
            ds = DirectionSample3f(scene, si, prev_si)

            mis = mis_weight(
                prev_bsdf_pdf,
                scene.pdf_emitter_direction(prev_si, ds, ~prev_bsdf_delta)
            )

            with ek.resume_grad(condition=not primal):
                Le = β * mis * ds.emitter.eval(si, prev_bsdf_pdf>0)

            # ---------------------- Emitter sampling ----------------------

            # Perform emitter sampling?
            active_next = (depth + 1 < self.max_depth) & si.is_valid()
            active_em = active_next & has_flag(bsdf.flags(), BSDFFlags.Smooth)

            ds, emitter_val = scene.sample_emitter_direction(
                si, sampler.next_2d(), True, active_em)
            active_em &= ek.neq(ds.pdf, 0.0)

            with ek.resume_grad(condition=not primal):
                # Recompute local 'wo' to propagate derivatives to cosine term
                wo = si.to_local(ds.d)

                # Evalute BRDF * cos(theta) differentiably
                bsdf_val, bsdf_pdf = bsdf.eval_pdf(bsdf_ctx, si, wo, active_em)
                mis_em = mis_weight(ek.select(ds.delta, 0.0, ds.pdf), bsdf_pdf)
                Lr_dir = β * mis_em * bsdf_val * emitter_val

            # ---------------------- BSDF sampling ----------------------

            # Perform detached BSDF sampling?
            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                   sampler.next_1d(),
                                                   sampler.next_2d(),
                                                   active_next)

            # ---- Update loop variables based on current interaction -----

            L = (L + Le + Lr_dir) if primal else (L - Le - Lr_dir)
            ray = si.spawn_ray(si.to_world(bsdf_sample.wo))
            η *= bsdf_sample.eta
            β *= bsdf_weight

            # Information about the current vertex needed by the next iteration
            prev_si = si
            prev_bsdf_pdf = bsdf_sample.pdf
            prev_bsdf_delta = has_flag(bsdf_sample.sampled_type, BSDFFlags.Delta)

            # ------------------ Differential phase only ------------------

            if not primal:
                with ek.resume_grad():
                    # Recompute local 'wo' to propagate derivatives to cosine term
                    wo = si.to_local(ray.d)

                    # Re-evalute BRDF * cos(theta) differentiably
                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active)

                    # Recompute the reflected indirect radiance (terminology not 100%
                    # correct, there may be a direct component due to MIS)
                    recip = bsdf_weight * bsdf_sample.pdf
                    Lr_ind = L * bsdf_val * ek.select(ek.neq(recip, 0), ek.rcp(recip), 0)

                    # Estimated contribution of the current vertex
                    contrib = Le + Lr_dir + Lr_ind

                    # Propagate derivatives from/to 'contrib' based on 'mode'
                    if mode == ek.ADMode.Backward:
                        ek.backward_from(δL * contrib)
                    else:
                        ek.forward_to(contrib)
                        δL += ek.grad(contrib)

            # -------------------- Stopping criterion ---------------------

            β_max = ek.hmax(β)
            rr_active = depth >= self.rr_depth
            rr_prob = ek.min(β_max * η**2, .95)
            rr_continue = sampler.next_1d() < rr_prob
            β[rr_active] *= ek.rcp(rr_prob)

            active = active_next & ek.neq(β_max, 0) & \
                     (~rr_active | rr_continue)

            depth[si.is_valid()] += 1

        return (
            L if primal else δL,          # Radiance/differential radiance
            ek.neq(depth, depth_initial), # Ray valid flag
            L                             # State for differential phase
        )

mitsuba.render.register_integrator("prb", lambda props:
                                   PRBIntegrator(props))
