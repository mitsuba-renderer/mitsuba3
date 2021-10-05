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
        grad_values = Spectrum(block.read(ds.uv)) * weight / spp

        with params.suspend_gradients():
            Li, _ = self.Li(scene, sampler, ray)

        ek.set_grad(grad_values, Li)
        ek.set_grad(Spectrum(reparam_div), grad_values * Li)
        ek.enqueue(ek.ADMode.Reverse, grad_values, reparam_div)
        ek.traverse(Float, retain_graph=False)

        # print(ek.graphviz_str(Float(1)))

        self.sample_adjoint(scene, sampler, ray, params, ek.detach(grad_values))

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
        """
        Performs a recursive step of a random walk to construct a light path.

        When `params=None`, this method will compute and return the incoming
        radiance along a path in a detached way. This is the case for the primal
        rendering phase (e.g. call to the `sample` method) or for a recursive
        call in the adjoint phase to compute the detached incoming radiance.

        When `params` is defined, the method will backpropagate `grad` to the
        scene parameters in `params` and return nothing.
        """
        from mitsuba.core import Spectrum, Float, UInt32, Ray3f, Loop
        from mitsuba.render import DirectionSample3f, BSDFContext, BSDFFlags, has_flag
        from mitsuba.python.ad import reparameterize_ray

        is_primal = len(params) == 0
        assert is_primal or ek.detached_t(grad) or grad.index_ad() == 0

        def adjoint(variables, active):
            for v, w in variables.items():
                v = ek.select(active, v, 0.0)
            if is_primal:
                result = 0.0
                for v, w in variables.items():
                    result += v * w
                return result
            else:
                for v, w in variables.items():
                    if ek.grad_enabled(v):
                        weight = w * grad
                        if ek.is_dynamic_array_v(v) and ek.is_static_array_v(weight):
                            weight = ek.hsum(weight)
                        ek.set_grad(v, weight)
                        ek.enqueue(ek.ADMode.Reverse, v)
                ek.traverse(Float, retain_graph=False)
                return 0

        def reparam(ray, active):
            return reparameterize_ray(scene, sampler, ray, active, params,
                                      num_auxiliary_rays=self.num_aux_rays,
                                      kappa=self.kappa, power=self.power)

        ray = Ray3f(ray_)
        prev_ray = Ray3f(ray_)

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
            # Attach incoming direction (reparameterization from the previous bounce)
            with params.resume_gradients():
                reparam_d, _ = reparam(prev_ray, active)
                si.wi = -si.to_local(reparam_d)

            # ---------------------- Direct emission ----------------------

            with params.resume_gradients():
                result += adjoint({
                    si.emitter(scene, active).eval(si, active):
                    throughput * emission_weight,
                }, active)

            active &= si.is_valid()
            active &= depth_i < self.max_depth

            ctx = BSDFContext()
            bsdf = si.bsdf(ray)

            # ---------------------- Emitter sampling ----------------------

            active_e = active & has_flag(bsdf.flags(), BSDFFlags.Smooth)
            ds, emitter_val = scene.sample_emitter_direction(
                ek.detach(si), sampler.next_2d(active_e), True, active_e)

            with params.resume_gradients():
                ray_e = ek.detach(si.spawn_ray(ds.d))
                reparam_d, reparam_div = reparam(ray_e, active_e)
                wo = si.to_local(reparam_d)

                bsdf_val, bsdf_pdf = bsdf.eval_pdf(ctx, si, wo, active_e)
                mis = ek.select(ds.delta, 1.0, mis_weight(ds.pdf, bsdf_pdf))

                result += adjoint({
                    # Term 4: Change in BSDF
                    # [\partial_theta fs(\omega_T, \omega_T')] * Li
                    bsdf_val : throughput * mis * emitter_val,
                    # Term 5: Rendering eq. divergence term
                    # fs * Li * [\partial_theta \nabla_T']
                    reparam_div : throughput * ek.detach(bsdf_val) * mis * emitter_val
                }, active_e)

            # ---------------------- BSDF sampling ----------------------

            bs, bsdf_weight = bsdf.sample(ctx, ek.detach(si),
                                          sampler.next_1d(active),
                                          sampler.next_2d(active), active)

            active &= bs.pdf > 0.0
            prev_ray = ray
            ray = si.spawn_ray(si.to_world(bs.wo))
            si_bsdf = scene.ray_intersect(ray, active)

            # Compute MIS weight for the BSDF sampling
            ds = DirectionSample3f(scene, si_bsdf, ek.detach(si))
            ds.emitter = si_bsdf.emitter(scene, active)
            delta = has_flag(bs.sampled_type, BSDFFlags.Delta)
            active_b = active & ek.neq(ds.emitter, None) & ~delta
            emitter_pdf = scene.pdf_emitter_direction(ek.detach(si), ds, active_b)
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
                    reparam_d, reparam_div = reparam(ray, active)
                    bsdf_eval = bsdf.eval(ctx, si, si.to_local(reparam_d), active)

                    adjoint({
                        # Term 4: Change in BSDF
                        # [\partial_theta fs(\omega_T, \omega_T')] * Li
                        bsdf_eval : throughput * li / bs.pdf,
                        # Term 5: Rendering eq. divergence term
                        # fs * Li * [\partial_theta \nabla_T']
                        reparam_div : throughput * ek.detach(bsdf_eval) * li / bs.pdf
                    }, active)

            # ------------------- Recurse to the next bounce -------------------

            si = si_bsdf
            throughput *= bsdf_weight

            depth_i += UInt32(1)

        return result, valid_ray

    def to_string(self):
        return f'RBReparamIntegrator[max_depth = {self.max_depth}]'


mitsuba.render.register_integrator("rbreparam", lambda props: RBReparamIntegrator(props))
