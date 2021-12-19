from __future__ import annotations # Delayed parsing of type annotations

import enoki as ek
import mitsuba

from .common import ADIntegrator

class TinyIntegrator(ADIntegrator):
    def __init__(self, props):
        super().__init__(props)

    def sample_impl(
        self,
        mode: ek.ADMode,
        scene: mitsuba.render.Scene,
        sampler: mitsuba.render.Sampler,
        ray: mitsuba.core.Ray3f,
        depth: mitsuba.core.UInt32,
        δL: Optional[mitsuba.core.Spectrum],
        state_in: Any,
        active: mitsuba.core.Bool
    ) -> Tuple[mitsuba.core.Spectrum, mitsuba.core.Bool, Any]:
        """
        asdf
        """

        from mitsuba.core import Loop, Spectrum, Float, Bool, UInt32, Ray3f
        from mitsuba.render import BSDFContext

        primal = mode == ek.ADMode.Primal

        # Copy the input arguments to avoid mutating caller state
        bsdf_ctx = BSDFContext()
        ray = Ray3f(ray)
        depth = UInt32(depth)
        L = Spectrum(0 if primal else state_in)
        δL = None if δL is None else Spectrum(δL)
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

            # Update radiance, throughput, and ray based on current interaction
            L = ek.fmadd(β, Le, L) if primal else ek.fmsub(β, Le, L) 

            ray = si.spawn_ray(si.to_world(bsdf_sample.wo))

            if mode == ek.ADMode.Backward:
                with ek.resume_grad():
                    # Recompute local outgoing direction to propagate derivatives to cosine term
                    wo = si.to_local(ray.d)

                    # Propagate derivative to emission, and BSDF * cosine term
                    Li = ek.select(ek.eq(bsdf_weight * bsdf_sample.pdf, 0), 0, L / bsdf_weight)
                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active) 
                    ek.backward(δL * (Le * β + Li * bsdf_val / bsdf_sample.pdf))

            β *= bsdf_weight

            # Stopping criterion
            depth += 1
            active &= si.is_valid() & (depth < self.max_depth)

        #  print(L)
        return L, True, L

    def to_string(self):
        return f'TinyIntegrator[max_depth = {self.max_depth}]'

mitsuba.render.register_integrator("tiny", lambda props: TinyIntegrator(props))
