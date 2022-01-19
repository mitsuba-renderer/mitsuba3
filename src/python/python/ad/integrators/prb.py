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
        from mitsuba.render import SurfaceInteraction3f, DirectionSample3f, \
            BSDFContext, BSDFFlags, RayFlags, has_flag

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
        η = Float(1)                               # Index of refraction
        active = Bool(active)                      # Active SIMD lanes

        # Variables caching information from the previous bounce
        prev_si         = ek.zero(SurfaceInteraction3f)
        prev_bsdf_pdf   = Float(1.0)
        prev_bsdf_delta = Bool(True)

        # Record the following loop in its entirety
        loop = Loop(name="Path Replay Backpropagation (%s)" % mode.name,
                    state=lambda: (sampler, ray, depth, L, δL, β, η, active,
                                   prev_si, prev_bsdf_pdf, prev_bsdf_delta))

        # Specify the max. number of loop iterations (this can help avoid
        # costly synchronization when when wavefront-style loops are generated)
        loop.set_max_iterations(self.max_depth)

        while loop(active):
            # Compute a surface interaction that tracks derivatives arising
            # from differentiable shape parameters (position, normals, etc.)
            # In primal mode, this is just an ordinary ray tracing operation.

            with ek.resume_grad(when=not primal):
                si = scene.ray_intersect(ray,
                                         ray_flags=RayFlags.All,
                                         coherent=ek.eq(depth, 0))

            # Get the BSDF, potentially computes texture-space differentials
            bsdf = si.bsdf(ray)

            # ---------------------- Direct emission ----------------------

            # Compute MIS weight for emitter sample from previous bounce
            ds = DirectionSample3f(scene, si=si, ref=prev_si)

            mis = mis_weight(
                prev_bsdf_pdf,
                scene.pdf_emitter_direction(prev_si, ds, ~prev_bsdf_delta)
            )

            with ek.resume_grad(when=not primal):
                Le = β * mis * ds.emitter.eval(si)

            # ---------------------- Emitter sampling ----------------------

            # Should we continue tracing to reach one more vertex?
            active_next = (depth + 1 < self.max_depth) & si.is_valid()

            # Is emitter sampling even possible on the current vertex?
            active_em = active_next & has_flag(bsdf.flags(), BSDFFlags.Smooth)

            # If so, randomly sample an emitter without derivative tracking.
            ds, em_weight = scene.sample_emitter_direction(
                si, sampler.next_2d(), True, active_em)
            active_em &= ek.neq(ds.pdf, 0.0)

            with ek.resume_grad(when=not primal):
                if not primal:
                    # Given the detached emitter sample, *recompute* its
                    # contribution with AD to enable light source optimization
                    ds.d = ek.normalize(ds.p - si.p)
                    em_val = scene.eval_emitter_direction(si, ds, active_em)
                    em_weight = ek.select(ek.neq(ds.pdf, 0), em_val / ds.pdf, 0)
                    ek.disable_grad(ds.d)

                # Evaluate BSDF * cos(theta) differentiably
                wo = si.to_local(ds.d)
                bsdf_value_em, bsdf_pdf_em = bsdf.eval_pdf(bsdf_ctx, si, wo, active_em)
                mis_em = ek.select(ds.delta, 1, mis_weight(ds.pdf, bsdf_pdf_em))
                Lr_dir = β * mis_em * bsdf_value_em * em_weight

            # ------------------ Detached BSDF sampling -------------------

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

            prev_si = ek.detach(si, True)
            prev_bsdf_pdf = bsdf_sample.pdf
            prev_bsdf_delta = has_flag(bsdf_sample.sampled_type, BSDFFlags.Delta)

            # -------------------- Stopping criterion ---------------------

            # Don't run another iteration if the throughput has reached zero
            β_max = ek.hmax(β)
            active_next &= ek.neq(β_max, 0)

            # Russian roulette stopping probability (must cancel out ior^2
            # to obtain unitless throughput, enforces a minimum probability)
            rr_prob = ek.min(β_max * η**2, .95)

            # Apply only further along the path since, this introduces variance
            rr_active = depth >= self.rr_depth
            β[rr_active] *= ek.rcp(rr_prob)
            rr_continue = sampler.next_1d() < rr_prob
            active_next &= ~rr_active | rr_continue

            # ------------------ Differential phase only ------------------

            if not primal:
                with ek.resume_grad():
                    # 'L' stores the indirectly reflected radiance at the
                    # current vertex but does not track parameter derivatives.
                    # The following addresses this by canceling the detached
                    # BSDF value and replacing it with an equivalent term that
                    # has derivative tracking enabled. (nit picking: the
                    # direct/indirect terminology isn't 100% accurate here,
                    # since there may be a direct component that is weighted
                    # via multiple importance sampling)

                    # Recompute 'wo' to propagate derivatives to cosine term
                    wo = si.to_local(ray.d)

                    # Re-evaluate BSDF * cos(theta) differentiably
                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active_next)

                    # Detached version of the above term and inverse
                    bsdf_val_det = bsdf_weight * bsdf_sample.pdf
                    inv_bsdf_val_det = ek.select(ek.neq(bsdf_val_det, 0),
                                                 ek.rcp(bsdf_val_det), 0)

                    # Differentiable version of the reflected indirect
                    # radiance. Minor optional tweak: indicate that the primal
                    # value of the second term is always 1.
                    Lr_ind = L * ek.replace_grad(1, inv_bsdf_val_det * bsdf_val)

                    # Differentiable Monte Carlo estimate of all contributions
                    Lo = Le + Lr_dir + Lr_ind

                    if ek.flag(ek.JitFlag.VCallRecord) and not ek.grad_enabled(Lo):
                        raise Exception(
                            "The contribution computed by the differential "
                            "rendering phase is not attached to the AD graph! "
                            "Raising an exception since this is usually "
                            "indicative of a bug (for example, you may have "
                            "forgotten to call ek.enable_grad(..) on one of "
                            "the scene parameters, or you may be trying to "
                            "optimize a parameter that does not generate "
                            "derivatives in detached PRB.)")

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
            L                    # State for the differential phase
        )

mitsuba.render.register_integrator("prb", lambda props:
                                   PRBIntegrator(props))
