from __future__ import annotations # Delayed parsing of type annotations

import enoki as ek
import mitsuba
from .common import prepare_sampler, sample_sensor_rays, mis_weight

class TinyIntegrator(mitsuba.render.SamplingIntegrator):
    """
    This integrator implements a path replay backpropagation surface path tracer.
    """
    def __init__(self, props = mitsuba.core.Properties()):
        super().__init__(props)
        self.max_depth = props.get('max_depth', 4)

    def render(self: mitsuba.render.SamplingIntegrator,
               scene: mitsuba.render.Scene,
               sensor: Union[int, mitsuba.render.Sensor] = 0,
               seed: int = 0,
               spp: int = 0,
               develop: bool = True,
               evaluate: bool = True) -> None:

        from mitsuba.core import Mask
        from mitsuba.render import ImageBlock

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        if not develop:
            raise Exception("develop=True must be specified when "
                            "invoking AD integrators")

        film = sensor.film()
        sampler = sensor.sampler()

        # Completely disable gradient tracking here just in case..
        with ek.suspend_grad():
            # Seed the sampler and compute the number of sample per pixels
            spp = prepare_sampler(sensor, seed, spp)

            ray, weight, pos, _ = sample_sensor_rays(sensor)

            spec, valid, state = self.sample_impl(
                mode=ek.ADMode.Primal,
                scene=scene,
                sampler=sampler,
                ray=ray,
                active=Mask(True)
            )

            block = ImageBlock(offset=film.crop_offset(),
                               size=film.crop_size(),
                               channel_count=5,
                               rfilter=film.rfilter(),
                               border=False)

            block.put(pos, ray.wavelengths, spec)

            return block.tensor()


    #def render_backward(self: mitsuba.render.SamplingIntegrator,
    #                    scene: mitsuba.render.Scene,
    #                    params: mitsuba.python.util.SceneParameters,
    #                    image_adj: mitsuba.core.TensorXf,
    #                    sensor: Union[int, mitsuba.render.Sensor] = 0,
    #                    seed: int = 0,
    #                    spp: int = 0) -> None:

    #    if isinstance(sensor, int):
    #        sensor = scene.sensors()[sensor]

    #    film = sensor.film()
    #    sampler = sensor.sampler()

    #    # Track no derivatives by default in any of the following. Evaluating
    #    # self.sample_impl(ADMode.Reverse, ..) will re-enable them as needed)

    #    with ek.suspend_grad():
    #        # Seed the sampler and compute the number of sample per pixels
    #        spp = prepare_sampler(sensor, seed, spp)

    #        ray, weight, pos, _ = sample_sensor_rays(sensor)

    #        spec, valid, state = self.sample_impl(
    #            mode=ek.ADMode.Primal,
    #            scene=scene,
    #            sampler=sampler.clone(),
    #            ray=ray,
    #            active=active
    #        )

    #        # Use the image reconstruction filter to
    #        block = ImageBlock(tensor=image_adj,
    #                           rfilter=film.rfilter(),
    #                           normalize=True)
    #        block.set_offset(film.crop_offset())

    #        δL = block.read(pos) * (weight * ek.rcp(spp))

    #        spec, valid = self.sample_impl(
    #            mode=ek.ADMode.Reverse,
    #            scene=scene,
    #            sampler=sampler.clone(),
    #            ray=ray,
    #            active=active,
    #            δL = δL
    #            
    #        )

    #    return spec, valid, [] # aov


    def sample_impl(self,
                    mode: ek.ADMode,
                    scene: mitsuba.render.Scene,
                    sampler: mitsuba.render.Sampler,
                    ray: mitsuba.core.RayDifferential3f,
                    active: mitsuba.core.Mask) -> Spectrum:

        from mitsuba.core import Loop, Spectrum, Float, Bool, UInt32
        from mitsuba.render import BSDFContext

        ctx = BSDFContext()
        active = Bool(True)
        depth = UInt32(0)

        loop = Loop("Path replay", lambda: (ray, active, depth))
        while loop(active):
            # Capture π-dependence of intersection for a detached input ray 
            si = scene.ray_intersect(ray)
            bsdf = si.bsdf(ray)
            active = Bool(False)

        return Spectrum(ek.abs(ray.d.x)), si.is_valid(), [] # no AOVs

    def to_string(self):
        return f'TinyIntegrator[max_depth = {self.max_depth}]'

mitsuba.render.register_integrator("tiny", lambda props: TinyIntegrator(props))
