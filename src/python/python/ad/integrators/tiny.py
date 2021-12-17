from __future__ import annotations # Delayed parsing of type annotations

import enoki as ek
import mitsuba
from .common import prepare_sampler, sample_sensor_rays, mis_weight

class TinyIntegrator(mitsuba.render.SamplingIntegrator):
    def __init__(self, props=mitsuba.core.Properties()):
        super().__init__(props)
        self.max_depth = props.get('max_depth', 4)

    def sample(self,
               scene: mitsuba.render.Scene,
               sampler: mitsuba.render.Sampler,
               ray: mitsuba.core.RayDifferential3f,
               medium: mitsuba.render.Medium,
               active: mitsuba.core.Mask):

        spec, valid = self.sample_impl(
            mode=ek.ADMode.Primal,
            scene=scene,
            sampler=sampler,
            ray=ray,
            active=active
        )

        return spec, valid, [] # aov


    def sample_impl(self,
                    mode: ek.ADMode,
                    scene: mitsuba.render.Scene,
                    sampler: mitsuba.render.Sampler,
                    ray: mitsuba.core.RayDifferential3f,
                    active: mitsuba.core.Mask):
        from mitsuba.core import Spectrum, Bool
        return Spectrum(ek.abs(ray.d.x)), Bool(True)

    def to_string(self):
        return f'TinyIntegrator[max_depth = {self.max_depth}]'


mitsuba.render.register_integrator("tiny", lambda props: TinyIntegrator(props))
