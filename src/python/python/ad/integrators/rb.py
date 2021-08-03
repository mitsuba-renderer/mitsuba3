import enoki as ek
import mitsuba
from .integrator import sample_sensor_rays, mis_weight


class RBIntegrator(mitsuba.render.SamplingIntegrator):
    """
    This integrator implements a Radiative Backpropagation path tracer.
    """
    def __init__(self, props=mitsuba.core.Properties()):
        super().__init__(props)
        self.max_depth = props.long_('max_depth', 4)
        self.recursive_li = props.bool_('recursive_li', True)

    def render_backward(self: mitsuba.render.SamplingIntegrator,
                        scene: mitsuba.render.Scene,
                        params: mitsuba.python.util.SceneParameters,
                        image_adj: mitsuba.core.Spectrum,
                        seed: int,
                        sensor_index: int=0,
                        spp: int=0) -> None:
        """
        Performed the adjoint rendering integration, backpropagating the
        image gradients to the scene parameters.
        """
        sensor = scene.sensors()[sensor_index]
        sampler = sensor.sampler()
        if spp > 0:
            sampler.set_sample_count(spp)
        spp = sampler.sample_count()
        sampler.seed(seed, ek.hprod(sensor.film().crop_size()) * spp)

        ray, weight, _, pos_idx = sample_sensor_rays(sensor)

        grad_values = ek.gather(mitsuba.core.Spectrum, ek.detach(image_adj), pos_idx)
        grad_values *= weight / spp

        for k, v in params.items():
            ek.enable_grad(v)

        self.sample_adjoint(scene, sampler, ray, params, grad_values)

    def sample(self, scene, sampler, ray, medium, active):
        return *self.Li(scene, sampler, ray), []

    def sample_adjoint(self, scene, sampler, ray, params, grad):
        with params.suspend_gradients():
            self.Li(scene, sampler, ray, params=params, grad=grad)

    def Li(self: mitsuba.render.SamplingIntegrator,
           scene: mitsuba.render.Scene,
           sampler: mitsuba.render.Sampler,
           ray: mitsuba.core.RayDifferential3f,
           depth: mitsuba.core.UInt32=1,
           params=mitsuba.python.util.SceneParameters(),
           grad: mitsuba.core.Spectrum=None,
           emission_weight: mitsuba.core.Float=None,
           active_: mitsuba.core.Mask=True):
        """
        Performs a recursive step of a random walk to construct a light path.

        When `params=None`, this method will compute and return the incoming
        radiance along a path in a detached way. This is the case for the primal
        rendering phase (e.g. call to the `sample` method) or for a recursive
        call in the adjoint phase to compute the detached incoming radiance.

        When `params` is defined, the method will backpropagate `grad` to the
        scene parameters in `params` and return nothing.
        """
        from mitsuba.core import Spectrum, Float, UInt32, RayDifferential3f, Loop
        from mitsuba.render import DirectionSample3f, BSDFContext, BSDFFlags, has_flag

        is_primal = len(params) == 0
        assert is_primal or ek.detached_t(grad) or grad.index_ad() == 0

        def adjoint(var, weight, active):
            var = ek.select(active, var, 0.0)
            if is_primal:
                return var * weight
            else:
                if ek.grad_enabled(var):
                    ek.backward(var * ek.detach(weight) * grad)
                return 0

        ray = RayDifferential3f(ray)

        si = scene.ray_intersect(ray, active_)
        valid_ray = active_ & si.is_valid()

        # Initialize loop variables
        result     = Spectrum(0.0)
        throughput = Spectrum(1.0)
        active     = active_ & si.is_valid()
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

            with params.resume_gradients():
                result += adjoint(
                    si.emitter(scene, active).eval(si, active),
                    throughput * emission_weight,
                    active
                )

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

            with params.resume_gradients():
                bsdf_val, bsdf_pdf = bsdf.eval_pdf(ctx, si, wo, active_e)
                mis = ek.select(ds.delta, 1.0, mis_weight(ds.pdf, bsdf_pdf))

                result += adjoint(
                    bsdf_val,
                    throughput * mis * emitter_val,
                    active_e
                )

            # ---------------------- BSDF sampling ----------------------

            bs, bsdf_weight = bsdf.sample(ctx, si, sampler.next_1d(active),
                                          sampler.next_2d(active), active)

            active &= bs.pdf > 0.0
            ray = si.spawn_ray(si.to_world(bs.wo))
            ray = RayDifferential3f(ray)
            si_bsdf = scene.ray_intersect(ray, active)

            # Compute MIS weight for the BSDF sampling
            ds = DirectionSample3f(scene, si_bsdf, si)
            ds.emitter = si_bsdf.emitter(scene, active)
            delta = has_flag(bs.sampled_type, BSDFFlags.Delta)
            active_b = active & ek.neq(ds.emitter, None) & ~delta
            emitter_pdf = scene.pdf_emitter_direction(si, ds, active_b)
            emission_weight = ek.select(active_b, mis_weight(bs.pdf, emitter_pdf), 1.0)

            if not is_primal:
                # Account for incoming radiance
                if self.recursive_li:
                    li = self.Li(scene, sampler, ray, depth_i+1,
                                 emission_weight=emission_weight,
                                 active_=active)[0]
                else:
                    li = ds.emitter.eval(si_bsdf, active_b) * emission_weight

                with params.resume_gradients():
                    bsdf_eval = bsdf.eval(ctx, si, bs.wo, active)

                    adjoint(
                        bsdf_eval,
                        throughput * li / bs.pdf,
                        active
                    )

            # ------------------- Recurse to the next bounce -------------------

            si = si_bsdf
            throughput *= bsdf_weight

            depth_i += UInt32(1)

        return result, valid_ray

    def to_string(self):
        return f'RBIntegrator[max_depth = {self.max_depth}]'


mitsuba.render.register_integrator("rb", lambda props: RBIntegrator(props))
