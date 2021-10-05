import enoki as ek
import mitsuba
from .integrator import sample_sensor_rays, mis_weight


class RBIntegrator(mitsuba.render.SamplingIntegrator):
    """
    This integrator implements a Radiative Backpropagation path tracer.
    """
    def __init__(self, props=mitsuba.core.Properties()):
        super().__init__(props)
        self.max_depth = props.get('max_depth', 4)
        self.recursive_li = props.get('recursive_li', True)

    def render_forward(self: mitsuba.render.SamplingIntegrator,
                       scene: mitsuba.render.Scene,
                       params: mitsuba.python.util.SceneParameters,
                       image_adj: mitsuba.core.TensorXf,
                       seed: int,
                       sensor_index: int=0,
                       spp: int=0) -> None:
        from mitsuba.core import Spectrum
        from mitsuba.render import ImageBlock
        sensor = scene.sensors()[sensor_index]
        film = sensor.film()
        rfilter = film.reconstruction_filter()
        sampler = sensor.sampler()
        if spp > 0:
            sampler.set_sample_count(spp)
        spp = sampler.sample_count()
        sampler.seed(seed, ek.hprod(film.crop_size()) * spp)

        ray, weight, pos, _, _ = sample_sensor_rays(sensor)

        block = ImageBlock(ek.detach(image_adj), rfilter, normalize=True)
        grad = Spectrum(block.read(pos)) * weight

        grad_img = self.Li(ek.ADMode.Forward, scene, sampler,
                           ray, params=params, grad=grad)[0]

        block = ImageBlock(film.crop_size(), channel_count=5,
                           filter=rfilter, border=False)
        block.clear()
        block.put(pos, ray.wavelengths, grad_img, 1.0)
        film.prepare(['R', 'G', 'B', 'A', 'W'])
        film.put(block)
        return film.develop()

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
        from mitsuba.render import ImageBlock
        sensor = scene.sensors()[sensor_index]
        rfilter = sensor.film().reconstruction_filter()
        sampler = sensor.sampler()
        if spp > 0:
            sampler.set_sample_count(spp)
        spp = sampler.sample_count()

        film_size = sensor.film().crop_size()
        if sensor.film().has_high_quality_edges():
            film_size += 2 * rfilter.border_size()

        sampler.seed(seed, ek.hprod(film_size) * spp)

        ray, weight, pos, _, _ = sample_sensor_rays(sensor)

        block = ImageBlock(ek.detach(image_adj), rfilter, normalize=True)
        grad = Spectrum(block.read(pos)) * weight / spp

        self.Li(ek.ADMode.Reverse, scene, sampler, ray, params=params, grad=grad)

    def sample(self, scene, sampler, ray, medium, active):
        return *self.Li(None, scene, sampler, ray), []

    def Li(self: mitsuba.render.SamplingIntegrator,
           mode: ek.ADMode,
           scene: mitsuba.render.Scene,
           sampler: mitsuba.render.Sampler,
           ray: mitsuba.core.Ray3f,
           depth: mitsuba.core.UInt32=1,
           params=mitsuba.python.util.SceneParameters(),
           grad: mitsuba.core.Spectrum=None,
           emission_weight: mitsuba.core.Float=None,
           active_: mitsuba.core.Mask=True):
        from mitsuba.core import Spectrum, Float, Mask, UInt32, Loop, Ray3f
        from mitsuba.render import DirectionSample3f, BSDFContext, BSDFFlags, has_flag

        is_primal = mode is None

        ray = Ray3f(ray)
        si = scene.ray_intersect(ray, active_)
        valid_ray = active_ & si.is_valid()

        # Initialize loop variables
        result     = Spectrum(0.0)
        throughput = Spectrum(1.0)
        active     = Mask(active_)
        if emission_weight is None:
            emission_weight = 1.0
        emission_weight = Float(emission_weight)

        depth_i = UInt32(depth)
        loop = Loop("RBPLoop" + '_recursive_li' if is_primal else '')
        loop.put(lambda:(depth_i, active, ray, emission_weight, throughput, si, result))
        sampler.loop_register(loop)
        loop.init()
        while loop(active):
            # ---------------------- Direct emission ----------------------

            emitter = si.emitter(scene, active)
            emitter_val = ek.select(active, emitter.eval(si, active), 0.0)
            accum = emitter_val * throughput * emission_weight

            active &= si.is_valid()
            active &= depth_i < self.max_depth

            ctx = BSDFContext()
            bsdf = si.bsdf(ray)

            # ---------------------- Emitter sampling ----------------------

            active_e = active & has_flag(bsdf.flags(), BSDFFlags.Smooth)
            ds, emitter_val = scene.sample_emitter_direction(
                si, sampler.next_2d(active_e), True, active_e)
            ds = ek.detach(ds, True)
            active_e &= ek.neq(ds.pdf, 0.0)
            wo = si.to_local(ds.d)

            bsdf_val, bsdf_pdf = bsdf.eval_pdf(ctx, si, wo, active_e)
            mis = ek.select(ds.delta, 1.0, mis_weight(ds.pdf, bsdf_pdf))

            accum += ek.select(active_e, bsdf_val *
                               throughput * mis * emitter_val, 0.0)

            # ---------------------- BSDF sampling ----------------------

            with ek.suspend_grad():
                bs, bsdf_weight = bsdf.sample(ctx, si, sampler.next_1d(active),
                                              sampler.next_2d(active), active)
                active &= bs.pdf > 0.0
                ray = si.spawn_ray(si.to_world(bs.wo))
                si_bsdf = scene.ray_intersect(ray, active)

            # Compute MIS weight for the BSDF sampling
            ds = DirectionSample3f(scene, si_bsdf, si)
            ds.emitter = si_bsdf.emitter(scene, active)
            delta = has_flag(bs.sampled_type, BSDFFlags.Delta)
            active_b = active & ek.neq(ds.emitter, None) & ~delta
            emitter_pdf = scene.pdf_emitter_direction(si, ds, active_b)
            emission_weight = ek.select(active_b, mis_weight(bs.pdf, emitter_pdf), 1.0)

            if not is_primal:
                with ek.suspend_grad():
                    # Account for incoming radiance
                    if self.recursive_li:
                        li = self.Li(None, scene, sampler, ray, depth_i+1,
                                     emission_weight=emission_weight,
                                     active_=active)[0]
                    else:
                        li = ds.emitter.eval(si_bsdf, active_b) * emission_weight

                bsdf_eval = bsdf.eval(ctx, si, bs.wo, active)
                accum += ek.select(active, bsdf_eval * throughput * li / bs.pdf, 0.0)

            if mode is ek.ADMode.Reverse:
                ek.backward(accum * grad)
            elif mode is ek.ADMode.Forward:
                ek.enqueue(ek.ADMode.Forward, params)
                ek.traverse(mitsuba.core.Float, retain_graph=False, retain_grad=True)
                result += ek.grad(accum) * grad
            else:
                result += accum

            # ------------------- Recurse to the next bounce -------------------

            si = si_bsdf
            throughput *= bsdf_weight

            depth_i += UInt32(1)

        return result, valid_ray

    def to_string(self):
        return f'RBIntegrator[max_depth = {self.max_depth}]'


mitsuba.render.register_integrator("rb", lambda props: RBIntegrator(props))
