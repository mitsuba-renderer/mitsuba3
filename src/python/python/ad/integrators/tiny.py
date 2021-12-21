from __future__ import annotations # Delayed parsing of type annotations

import enoki as ek
import mitsuba

from .common import ADIntegrator

class TinyPRBIntegrator(ADIntegrator):
    """
    Tiny PRB-style integrator *without* next event estimation / multiple
    importance sampling / re-parameterization. The lack of all of these
    features means that gradients are relatively noisy and don't correctly
    account for visibility discontinuties.

    This class exists to illustrate how a very basic rendering algorithm can be
    implemented in Python along with efficient forward/reverse-mode derivatives
    """

    def sample(
        self,
        mode: enoki.ADMode,
        scene: mitsuba.render.Scene,
        sampler: mitsuba.render.Sampler,
        ray: mitsuba.core.Ray3f,
        depth: mitsuba.core.UInt32,
        δL: Optional[mitsuba.core.Spectrum],
        state_in: Any,
        active: mitsuba.core.Bool
    ) -> Tuple[mitsuba.core.Spectrum, mitsuba.core.Bool, Any]:
        """
        Parameter ``mode`` (``enoki.ADMode``)
            Specifies whether the rendering algorithm should run in primal or
            forward/backward derivative propagation mode

        Parameter ``scene`` (``mitsuba.render.Scene``):
            Reference to the scene being rendered in a differentiable manner.

        Parameter ``sampler`` (``mitsuba.render.Sampler``):
            A pre-seeded sample generator

        Parameter ``depth`` (``mitsuba.core.UInt32``):
            Path depth of `ray` (typically set to zero).

        Parameter ``δL`` (``mitsuba.core.Spectrum``):
            When back-propagating gradients (``mode == enoki.ADMode.Backward``)
            the ``δL`` parameter should specify the adjoint radiance associated
            with each ray. Otherwise, it must be set to ``None``.

        Parameter ``state_in`` (``Any``):
            The primal phase of ``sample()`` returns a state vector as part of
            its return value. The forward/backward differential phases expect
            that this state vector is provided to them via this argument. When
            invoked in primal mode, it should be set to ``None``.

        Parameter ``active`` (``mitsuba.core.Bool``):
            This mask array can optionally be used to indicate that some of
            the rays are disabled.

        The function returns a tuple ``(spec, valid, state_out)`` where

        Output ``spec`` (``mitsuba.core.Spectrum``):
            Specifies the estimated radiance and differential radiance in
            primal and forward mode, respectively.

        Output ``valid`` (``mitsuba.core.Bool``):
            Indicates whether the rays intersected a surface, which can be used
            to compute an alpha channel.

        Output ``state_out`` (``Any``):
            When invoked in primal mode, this return argument provides an
            unspecified state vector that is a required input of both
            forward/backward differential phases.
        """

        from mitsuba.core import Loop, Spectrum, Float, Bool, UInt32, Ray3f
        from mitsuba.render import BSDFContext

        primal = mode == ek.ADMode.Primal

        # Copy the input arguments to avoid mutating caller state
        bsdf_ctx = BSDFContext()
        ray = Ray3f(ray)
        depth, depth_initial = UInt32(depth), UInt32(depth)
        L = Spectrum(0 if primal else state_in)
        δL = Spectrum(δL if δL is not None else 0)
        active = Bool(active)
        β = Spectrum(1)

        # Label loop variables (e.g. for GraphViz plots of the computation graph)
        ek.set_label(ray=ray, depth=depth, L=L, δL=δL, β=β, active=active)

        # Recorded loop over bounces
        loop = Loop(name="Path replay backpropagation (%s mode)" % mode.name.lower(),
                    state=lambda: (sampler, ray, depth, L, δL, β, active))

        while loop(active):
            with ek.resume_grad(condition=not primal):
                # Capture π-dependence of intersection for a detached input ray
                si = scene.ray_intersect(ray)
                bsdf = si.bsdf(ray)

                # Differentiable evaluation of intersected emitter / envmap
                Le = si.emitter(scene).eval(si)

            # Detached BSDF sampling
            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                   sampler.next_1d(),
                                                   sampler.next_2d())

            # Generate an outgoing ray from the current surface interaction
            ray = si.spawn_ray(si.to_world(bsdf_sample.wo))

            # Update radiance, throughput, and ray based on current interaction
            L = ek.fmadd(β, Le, L) if primal else ek.fnmadd(β, Le, L)

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
                    contrib = ek.fmadd(Le, β, Lr)

                    if mode == ek.ADMode.Backward:
                        ek.backward(δL * contrib)
                    else:
                        ek.forward_to(contrib)
                        δL += ek.grad(contrib)

            β *= bsdf_weight

            # Stopping criterion
            depth[si.is_valid()] += 1
            active &= si.is_valid() & (depth < self.max_depth)

        # Return radiance, sample status, state vector for differential phase
        result_spec = L if primal else δL
        result_valid = eq.neq(depth, depth_initial)
        result_state = L

        return result_spec, result_valid, result_state

mitsuba.render.register_integrator("prb_tiny", lambda props:
                                   TinyPRBIntegrator(props))
