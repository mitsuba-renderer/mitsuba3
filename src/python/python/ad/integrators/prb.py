import enoki as ek
import mitsuba
from .integrator import sample_sensor_rays, mis_weight


class PRBIntegrator(mitsuba.render.SamplingIntegrator):
    """
    This integrator implements a path replay backpropagation surface path tracer.
    """

    def __init__(self, props=mitsuba.core.Properties()):
        super().__init__(props)
        self.max_depth = props.long_('max_depth', 4)

    def render_backward(self: mitsuba.render.SamplingIntegrator,
                       scene: mitsuba.render.Scene,
                       params: mitsuba.python.util.SceneParameters,
                       image_adj: mitsuba.core.TensorXf,
                       seed: int,
                       sensor_index: int = 0,
                       spp: int = 0) -> None:

        sensor = scene.sensors()[sensor_index]
        rfilter = sensor.film().reconstruction_filter()
        sampler = sensor.sampler()
        if spp > 0:
            sampler.set_sample_count(spp)
        spp = sampler.sample_count()
        sampler.seed(seed, ek.hprod(sensor.film().crop_size()) * spp)

        ray, weight, pos, _ = sample_sensor_rays(sensor)

        # Sample forward paths (not differentiable)
        with ek.suspend_grad():
            result, _ = self.Li(scene, sampler.clone(), ray)

        block = mitsuba.render.ImageBlock(ek.detach(image_adj), rfilter)
        grad_values = block.read(pos) * weight / spp

        for k, v in params.items():
            ek.enable_grad(v)

        # Replay light paths by using the same seed and accumulate gradients
        # This uses the result from the first pass to compute gradients
        self.sample_adjoint(scene, sampler, ray, params, grad_values, result)

    def sample(self, scene, sampler, ray, medium, active):
        return *self.Li(scene, sampler, ray), []

    def sample_adjoint(self, scene, sampler, ray, params, grad, result):
        self.Li(scene, sampler, ray, params=params, grad=grad, result=result)

    def Li(self, scene, sampler, ray, depth=1, params=mitsuba.python.util.SceneParameters(),
           grad=None, active_=True, result=None):

        from mitsuba.core import Spectrum, Float, UInt32, Loop, RayDifferential3f
        from mitsuba.render import DirectionSample3f, BSDFContext, BSDFFlags, has_flag

        is_primal = len(params) == 0

        ray = RayDifferential3f(ray)

        si = scene.ray_intersect(ray, active_)
        valid_ray = active_ & si.is_valid()

        if is_primal:
            result = Spectrum(0.0)

        throughput = Spectrum(1.0)
        active = active_ & si.is_valid()
        emission_weight = Float(1.0)

        depth_i = UInt32(depth)
        loop = Loop("Path Replay Backpropagation main loop" + '' if is_primal else ' - adjoint')
        loop.put(lambda: (depth_i, active, ray, emission_weight, throughput, si, result))
        sampler.loop_register(loop)
        loop.init()
        while loop(active):
            # ---------------------- Direct emission ----------------------
            emitter_contrib = throughput * emission_weight * ek.select(active, si.emitter(scene, active).eval(si, active), 0.0)
            active &= si.is_valid()
            active &= depth_i < self.max_depth
            ctx = BSDFContext()
            bsdf = si.bsdf(ray)

            # ---------------------- Emitter sampling ----------------------
            active_e = active & has_flag(bsdf.flags(), BSDFFlags.Smooth)
            ds, emitter_val = scene.sample_emitter_direction(
                si, sampler.next_2d(active_e), True, active_e)
            active_e &= ek.neq(ds.pdf, 0.0)
            wo = si.to_local(ds.d)

            bsdf_val, bsdf_pdf = bsdf.eval_pdf(ctx, si, wo, active_e)
            mis = ek.select(ds.delta, 1.0, mis_weight(ds.pdf, bsdf_pdf))

            emitter_contrib += ek.select(active_e, mis * throughput * bsdf_val * emitter_val, 0.0)
            # Update accumulated radiance. When propagating gradients, we subtract the
            # emitter contributions instead of adding them
            result += ek.detach(emitter_contrib if is_primal else -emitter_contrib)

            # ---------------------- BSDF sampling ----------------------
            with ek.suspend_grad():
                bs, bsdf_weight = bsdf.sample(ctx, si, sampler.next_1d(active),
                                              sampler.next_2d(active), active)
                active &= bs.pdf > 0.0
                ray = RayDifferential3f(si.spawn_ray(si.to_world(bs.wo)))
                si_bsdf = scene.ray_intersect(ray, active)

            # Compute MIS weight for the BSDF sampling
            ds = DirectionSample3f(scene, si_bsdf, si)
            ds.emitter = si_bsdf.emitter(scene, active)
            delta = has_flag(bs.sampled_type, BSDFFlags.Delta)
            active_b = active & ek.neq(ds.emitter, None) & ~delta
            emitter_pdf = scene.pdf_emitter_direction(si, ds, active_b)
            emission_weight = ek.select(active_b, mis_weight(bs.pdf, emitter_pdf), 1.0)

            # Backpropagate gradients related to the current bounce
            if not is_primal:
                bsdf_eval = bsdf.eval(ctx, si, bs.wo, active)
                ek.backward(grad * (emitter_contrib + bsdf_eval * ek.detach(ek.select(active, result / ek.max(1e-8, bsdf_eval), 0.0))))

            si = si_bsdf
            throughput *= bsdf_weight

            depth_i += UInt32(1)

        return result, valid_ray

    def to_string(self):
        return f'PRBIntegrator[max_depth = {self.max_depth}]'


mitsuba.render.register_integrator("prb", lambda props: PRBIntegrator(props))
