import enoki as ek
import mitsuba
from .integrator import sample_sensor_rays, mis_weight


class PRBReparamIntegrator(mitsuba.render.SamplingIntegrator):
    """
    This integrator implements a path replay backpropagation surface path tracer
    with reparameterization to handle discontinuities.
    """

    def __init__(self, props=mitsuba.core.Properties()):
        super().__init__(props)
        self.max_depth = props.long_('max_depth', 4)
        self.num_aux_rays = props.long_('num_aux_rays', 4)
        self.kappa = props.float_('kappa', 1e5)
        self.power = props.float_('power', 3.0)

    def render_backward(self: mitsuba.render.SamplingIntegrator,
                        scene: mitsuba.render.Scene,
                        params: mitsuba.python.util.SceneParameters,
                        image_adj: mitsuba.core.TensorXf,
                        seed: int,
                        sensor_index: int = 0,
                        spp: int = 0) -> None:
        from mitsuba.core import Float, Spectrum
        from mitsuba.python.ad import reparameterize_ray

        sensor = scene.sensors()[sensor_index]
        rfilter = sensor.film().reconstruction_filter()
        assert rfilter.radius() > 0.5 + mitsuba.core.math.RayEpsilon

        sampler = sensor.sampler()
        if spp > 0:
            sampler.set_sample_count(spp)
        spp = sampler.sample_count()
        sampler.seed(seed, ek.hprod(sensor.film().crop_size()) * spp)

        ray, _, _, _, aperture_samples = sample_sensor_rays(sensor)

        # Reparameterize primary rays
        reparam_d, reparam_div = reparameterize_ray(scene, sampler, ray, True,
                                                    params,
                                                    num_auxiliary_rays=self.num_aux_rays,
                                                    kappa=self.kappa,
                                                    power=self.power)

        it = ek.zero(mitsuba.render.Interaction3f)
        it.p = ray.o + reparam_d
        ds, weight = sensor.sample_direction(it, aperture_samples)

        # Sample forward paths (not differentiable)
        with ek.suspend_grad():
            Li, _ = self.Li(scene, sampler.clone(), ray)

        block = mitsuba.render.ImageBlock(ek.detach(image_adj), rfilter, normalize=True)
        grad_values = Spectrum(block.read(ds.uv)) * weight / spp

        ek.set_grad(grad_values, Li)
        ek.set_grad(Spectrum(reparam_div), grad_values * Li)
        ek.enqueue(ek.ADMode.Reverse, grad_values, reparam_div)
        ek.traverse(Float, retain_graph=False)

        # Replay light paths by using the same seed and accumulate gradients
        # This uses the result from the first pass to compute gradients
        self.sample_adjoint(scene, sampler, ray, params, ek.detach(grad_values), Li)

    def sample(self, scene, sampler, ray, medium, active):
        return *self.Li(scene, sampler, ray), []

    def sample_adjoint(self, scene, sampler, ray, params, grad, result):
        with ek.suspend_grad():
            self.Li(scene, sampler, ray, params=params, grad=grad, result=result)

    def Li(self: mitsuba.render.SamplingIntegrator,
           scene: mitsuba.render.Scene,
           sampler: mitsuba.render.Sampler,
           ray_: mitsuba.core.RayDifferential3f,
           depth: mitsuba.core.UInt32=1,
           params=mitsuba.python.util.SceneParameters(),
           grad: mitsuba.core.Spectrum=None,
           active_: mitsuba.core.Mask=True,
           result: mitsuba.core.Spectrum=None):

        from mitsuba.core import Spectrum, Float, UInt32, Loop, Ray3f
        from mitsuba.render import DirectionSample3f, BSDFContext, BSDFFlags, has_flag, HitComputeFlags
        from mitsuba.python.ad import reparameterize_ray

        def reparam(ray, active):
            return reparameterize_ray(scene, sampler, ray, active, params,
                                      num_auxiliary_rays=self.num_aux_rays,
                                      kappa=self.kappa, power=self.power)

        is_primal = len(params) == 0

        ray = Ray3f(ray_)
        prev_ray = Ray3f(ray_)

        pi = scene.ray_intersect_preliminary(ray, active_)
        valid_ray = active_ & pi.is_valid()

        if is_primal:
            result = Spectrum(0.0)

        throughput = Spectrum(1.0)
        active = active_ & pi.is_valid()
        emission_weight = Float(1.0)

        depth_i = UInt32(depth)
        loop = Loop("Path Replay Backpropagation main loop" + '' if is_primal else ' - adjoint')
        loop.put(lambda: (depth_i, active, ray, prev_ray, emission_weight, throughput, pi, result))
        sampler.loop_register(loop)
        loop.init()
        while loop(active):
            # Attach incoming direction (reparameterization from the previous bounce)
            with ek.resume_grad():
                si = pi.compute_surface_interaction(ray, HitComputeFlags.All, active)
                reparam_d, _ = reparam(prev_ray, active)
                si.wi = -si.to_local(reparam_d)

            # ---------------------- Direct emission ----------------------

            with ek.resume_grad():
                emitter_val = ek.select(active, si.emitter(scene, active).eval(si, active), 0.0)
                emitter_contrib = throughput * emission_weight * emitter_val

            active &= si.is_valid()
            active &= depth_i < self.max_depth

            ctx = BSDFContext()
            bsdf = si.bsdf(ray)

            # ---------------------- Emitter sampling ----------------------

            active_e = active & has_flag(bsdf.flags(), BSDFFlags.Smooth)
            ds, emitter_val = scene.sample_emitter_direction(
                ek.detach(si), sampler.next_2d(active_e), True, active_e)
            active_e &= ek.neq(ds.pdf, 0.0)

            with ek.resume_grad():
                # Reparameterize sampled NEE direction
                ray_e = ek.detach(si.spawn_ray(ds.d))
                reparam_d, reparam_div = reparam(ray_e, active_e)
                wo = si.to_local(reparam_d)

                bsdf_val, bsdf_pdf = bsdf.eval_pdf(ctx, si, wo, active_e)
                mis = ek.select(ds.delta, 1.0, mis_weight(ds.pdf, bsdf_pdf))

                emitter_contrib += ek.select(active_e, mis * throughput * bsdf_val * emitter_val, 0.0)

                # Update accumulated radiance. When propagating gradients, we subtract the
                # emitter contributions instead of adding them
                result += ek.detach(emitter_contrib if is_primal else -emitter_contrib)

                emitter_contrib += ek.select(active_e, mis * throughput * ek.detach(bsdf_val) * emitter_val * reparam_div, 0.0)

            # ---------------------- BSDF sampling ----------------------

            bs, bsdf_weight = bsdf.sample(ctx, ek.detach(si),
                                          sampler.next_1d(active),
                                          sampler.next_2d(active), active)
            active &= bs.pdf > 0.0
            prev_ray = ray
            ray = ek.detach(si.spawn_ray(si.to_world(bs.wo)))
            pi_bsdf = scene.ray_intersect_preliminary(ray, active)
            si_bsdf = pi_bsdf.compute_surface_interaction(ray, HitComputeFlags.All, active)

            # Compute MIS weight for the BSDF sampling
            ds = DirectionSample3f(scene, si_bsdf, ek.detach(si))
            ds.emitter = si_bsdf.emitter(scene, active)
            delta = has_flag(bs.sampled_type, BSDFFlags.Delta)
            active_b = active & ek.neq(ds.emitter, None) & ~delta
            emitter_pdf = scene.pdf_emitter_direction(ek.detach(si), ds, active_b)
            emission_weight = ek.select(active_b, mis_weight(bs.pdf, emitter_pdf), 1.0)

            # Backpropagate gradients related to the current bounce
            if not is_primal:
                with ek.resume_grad():
                    # Reparameterize BSDF sampled direction
                    reparam_d, reparam_div = reparam(ray, active)
                    bsdf_eval = bsdf.eval(ctx, si, si.to_local(reparam_d), active)

                    accum_contrib = emitter_contrib
                    accum_contrib += bsdf_eval * ek.detach(ek.select(active, result / ek.max(1e-8, bsdf_eval), 0.0))
                    accum_contrib += reparam_div * ek.select(active, result, 0.0)
                    ek.backward(grad * accum_contrib)

            pi = pi_bsdf
            throughput *= bsdf_weight

            depth_i += UInt32(1)

        return result, valid_ray

    def to_string(self):
        return f'PRBReparamIntegrator[max_depth = {self.max_depth}]'


mitsuba.render.register_integrator("prbreparam", lambda props: PRBReparamIntegrator(props))
