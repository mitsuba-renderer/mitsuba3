from __future__ import annotations # Delayed parsing of type annotations

import enoki as ek
import mitsuba

from .common import ADIntegrator

class TinyIntegrator(ADIntegrator):
    def sample_impl(self, mode: ek.ADMode,
                    scene: mitsuba.render.Scene,
                    sampler: mitsuba.render.Sampler,
                    ray: mitsuba.core.Ray3f,
                    active: mitsuba.core.Bool,
                    depth: mitsuba.core.UInt32 = 1,
                    δL: mitsuba.core.Spectrum = None,
                    state_in: object = None) -> Spectrum:

        from mitsuba.core import Loop, Spectrum, Float, Bool, UInt32, Ray3f
        from mitsuba.render import BSDFContext

        primal = mode == ek.ADMode.Primal

        bsdf_ctx = BSDFContext()
        active = Bool(active)
        depth = UInt32(depth)
        ray = Ray3f(ray)
        β = Spectrum(1)
        δL = Spectrum(0 if primal else δL)
        L = Spectrum(0 if primal else state_in)
        self.max_depth = 0

        loop = Loop(name="Path replay backpropagation (%s mode)" % mode.name.lower(),
                    state=lambda: (active, depth, ray, β, L, δL, sampler))

        while loop(active):
            with ek.resume_grad():
                # Capture π-dependence of intersection for a detached input ray 
                si = scene.ray_intersect(ray)
                bsdf = si.bsdf(ray)

                # Differentiable evaluation of intersected emitter / envmap
                Le = si.emitter(scene).eval(si)

            # Detached BSDF sampling
            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si, 
                                                   sampler.next_1d(),
                                                   sampler.next_2d())

            # Update radiance, throughput, and ray based on current interaction
            L = ek.fmadd(β, Le, L) if primal else ek.fmsub(β, Le, L) 

            ray = si.spawn_ray(si.to_world(bsdf_sample.wo))

            if mode == ek.ADMode.Backward:
                with ek.resume_grad():
                    # Recompute local outgoing direction to propagate derivatives to cosine term
                    wo = si.to_local(ray.d)

                    # Propagate derivative to emission, and BSDF * cosine term
                    Li = L / bsdf_weight
                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active) 
                    assert ek.grad_enabled(bsdf_val)
                    print(ek.grad_enabled(bsdf_val))
                    ek.backward(δL * (Le * β + Li * bsdf_val / bsdf_sample.pdf))

            β *= bsdf_weight

            # Stopping criterion
            active &= si.is_valid() & (depth < self.max_depth)
            depth += 1

        return L, True, L

    def to_string(self):
        return f'TinyIntegrator[max_depth = {self.max_depth}]'

mitsuba.render.register_integrator("tiny", lambda props: TinyIntegrator(props))
