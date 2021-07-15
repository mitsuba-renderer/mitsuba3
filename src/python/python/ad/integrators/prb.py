import enoki as ek
import mitsuba

from mitsuba.core import Spectrum, Float, UInt32, Ray3f, Loop
from mitsuba.render import DirectionSample3f, BSDFContext, BSDFFlags, has_flag

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
                        image_adj: mitsuba.core.Spectrum,
                        sensor_index: int = 0,
                        spp: int = 0) -> None:

        sensor = scene.sensors()[sensor_index]
        sampler = sensor.sampler()
        if spp > 0:
            sampler.set_sample_count(spp)

        rays, weights, _, pos_idx = sample_sensor_rays(sensor)

        # sample forward path (not differentiable)
        result, _ = self.Li(scene, sampler.clone(), rays)

        grad_values = ek.gather(mitsuba.core.Spectrum, ek.detach(image_adj), pos_idx)
        grad_values *= weights / sampler.sample_count()

        for k, v in params.items():
            ek.enable_grad(v)

        # Replay light paths and accumulate gradients
        self.sample_adjoint(scene, sampler, rays, params, grad_values, result)

    def sample(self, scene, sampler, rays, medium, active):
        return *self.Li(scene, sampler, rays, params=None), []

    def sample_adjoint(self, scene, sampler, rays, params, grad, result):
        self.Li(scene, sampler, rays, params=params, grad=grad, result=result)

    def Li(self, scene, sampler, rays, depth=1, params=None, grad=None, active_=True, result=None):
        is_primal = params is None
        rays = Ray3f(rays)

        si = scene.ray_intersect(rays, active_)
        valid_rays = active_ & si.is_valid()

        if is_primal:
            result = Spectrum(0.0)

        throughput = Spectrum(1.0)
        active = active_ & si.is_valid()
        emission_weight = Float(1.0)

        if params:
            params.set_grad_suspended(is_primal)

        depth_i = UInt32(depth)
        loop = Loop("PRBLoop" + '' if is_primal else '_adjoint')
        loop.put(lambda: (depth_i, active, rays, emission_weight, throughput, si, result))
        sampler.loop_register(loop)
        loop.init()
        while loop(active):
            # ---------------------- Direct emission ----------------------
            emitter_contrib = throughput * emission_weight * si.emitter(scene, active).eval(si, active)
            active &= si.is_valid()
            active &= depth_i < self.max_depth
            ctx = BSDFContext()
            bsdf = si.bsdf()

            # ---------------------- Emitter sampling ----------------------
            active_e = active & has_flag(bsdf.flags(), BSDFFlags.Smooth)
            ds, emitter_val = scene.sample_emitter_direction(
                si, sampler.next_2d(active_e), True, active_e)
            active_e &= ek.neq(ds.pdf, 0.0)
            wo = si.to_local(ds.d)

            bsdf_val, bsdf_pdf = bsdf.eval_pdf(ctx, si, wo, active_e)
            mis = ek.select(ds.delta, 1.0, mis_weight(ds.pdf, bsdf_pdf))

            emitter_contrib += ek.select(active_e, mis * throughput * bsdf_val * emitter_val, 0.0)
            result += ek.detach(emitter_contrib if is_primal else -emitter_contrib)

            # ---------------------- BSDF sampling ----------------------
            bs, bsdf_val = bsdf.sample(ctx, si, sampler.next_1d(active),
                                       sampler.next_2d(active), active)
            active &= bs.pdf > 0.0
            rays = ek.detach(si.spawn_ray(si.to_world(bs.wo)))
            si_bsdf = scene.ray_intersect(rays, active)

            # Compute MIS weights for the BSDF sampling
            ds = DirectionSample3f(scene, si_bsdf, si)
            ds.emitter = si_bsdf.emitter(scene, active)
            delta = has_flag(bs.sampled_type, BSDFFlags.Delta)
            active_b = active & ek.neq(ds.emitter, None) & ~delta
            emitter_pdf = scene.pdf_emitter_direction(si, ds, active_b)
            emission_weight = ek.select(active_b, mis_weight(bs.pdf, emitter_pdf), 1.0)

            if not is_primal:
                bsdf_eval = bsdf.eval(ctx, si, ek.detach(bs.wo), active)
                ek.backward(grad * (emitter_contrib + bsdf_eval * ek.detach(ek.select(active, result / ek.max(1e-8, bsdf_eval), 0.0))))

            si = ek.detach(si_bsdf)
            throughput *= ek.detach(bsdf_val)

            depth_i += UInt32(1)

        return result, valid_rays

    def to_string(self):
        return f'PRBIntegrator[max_depth = {self.max_depth}]'


mitsuba.render.register_integrator("prb", lambda props: PRBIntegrator(props))
