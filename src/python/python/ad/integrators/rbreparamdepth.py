import enoki as ek
import mitsuba
from .integrator import sample_sensor_rays, mis_weight


class RBReparamIntegrator(mitsuba.render.SamplingIntegrator):
    """
    This integrator implements a Radiative Backpropagation path tracer with
    reparameterization.
    """
    def __init__(self, props=mitsuba.core.Properties()):
        super().__init__(props)
        self.max_depth = props.long_('max_depth', 4)
        self.recursive_li = props.bool_('recursive_li', True)
        self.num_aux_rays = props.long_('num_aux_rays', 4)
        self.kappa = props.float_('kappa', 1e5)
        self.power = props.float_('power', 3.0)

    def render_backward(self: mitsuba.render.SamplingIntegrator,
                        scene: mitsuba.render.Scene,
                        params: mitsuba.python.util.SceneParameters,
                        image_adj: mitsuba.core.TensorXf,
                        seed: int,
                        sensor_index: int=0,
                        spp: int=0) -> None:
        """
        Performed the adjoint rendering integration, backpropagating the
        image gradients to the scene parameters.
        """
        from mitsuba.core import Spectrum
        from mitsuba.python.ad import reparameterize_ray

        sensor = scene.sensors()[sensor_index]
        rfilter = sensor.film().reconstruction_filter()
        sampler = sensor.sampler()
        if spp > 0:
            sampler.set_sample_count(spp)
        spp = sampler.sample_count()
        sampler.seed(seed, ek.hprod(sensor.film().crop_size()) * spp)

        ray, _, pos, _, aperture_samples = sample_sensor_rays(sensor)

        # Reparameterize primary rays
        reparam_d, reparam_div = reparameterize_ray(scene, sampler, ray, True,
                                                    params,
                                                    num_auxiliary_rays=self.num_aux_rays,
                                                    kappa=self.kappa,
                                                    power=self.power)

        it = ek.zero(mitsuba.render.Interaction3f)
        it.p = ray.o + reparam_d
        ds, weight = sensor.sample_direction(it, aperture_samples)

        block = mitsuba.render.ImageBlock(ek.detach(image_adj), rfilter)
        # grad_values = block.read(ds.uv) * weight / spp
        grad_values = block.read(ds.uv) * ek.detach(weight, True) / spp
        # grad_values = block.read(ds.uv) * ek.detach(weight, True) / spp

        # with params.suspend_gradients():
            # Li, _ = self.Li(scene, sampler, ray)

        # ek.set_grad(ds.uv.x, 1.0)
        # ek.enqueue(ek.ADMode.Reverse, ds.uv.x)

        # ek.set_grad(reparam_d.x, 1.0 / spp)
        # ek.enqueue(ek.ADMode.Reverse, reparam_d.x)

        ek.set_grad(grad_values, 1.0)
        # ek.enqueue(ek.ADMode.Reverse, grad_values)
        # ek.set_grad(Spectrum(reparam_div), grad_values * Li)
        ek.enqueue(ek.ADMode.Reverse, grad_values, reparam_div)
        ek.traverse(mitsuba.core.Float, retain_graph=False)

        # self.sample_adjoint(scene, sampler, ray, params, ek.detach(grad_values))

    def sample(self, scene, sampler, ray, medium, active):
        return *self.Li(scene, sampler, ray), []

    def sample_adjoint(self, scene, sampler, ray, params, grad):
        with params.suspend_gradients():
            self.Li(scene, sampler, ray, params=params, grad=grad)

    def Li(self: mitsuba.render.SamplingIntegrator,
           scene: mitsuba.render.Scene,
           sampler: mitsuba.render.Sampler,
           ray_: mitsuba.core.RayDifferential3f,
           depth: mitsuba.core.UInt32=1,
           params=mitsuba.python.util.SceneParameters(),
           grad: mitsuba.core.Spectrum=None,
           emission_weight: mitsuba.core.Float=None,
           active_: mitsuba.core.Mask=True):
        si = scene.ray_intersect(ray_, active_)
        active_ &= si.is_valid()
        return ek.select(active_, si.t, 0.0), active_

    def to_string(self):
        return f'RBReparamDepthIntegrator[max_depth = {self.max_depth}]'


mitsuba.render.register_integrator("rbreparamdepth", lambda props: RBReparamIntegrator(props))
