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
        self.num_aux_rays = props.long_('num_aux_rays', 16)
        self.kappa = props.float_('kappa', 1e5)
        self.power = props.float_('power', 3.0)

    def render_forward(self: mitsuba.render.SamplingIntegrator,
                       scene: mitsuba.render.Scene,
                       params: mitsuba.python.util.SceneParameters,
                       image_adj: mitsuba.core.TensorXf,
                       seed: int,
                       sensor_index: int=0,
                       spp: int=0) -> None:
        from mitsuba.core import Float, Spectrum
        from mitsuba.render import ImageBlock, Interaction3f
        from mitsuba.python.ad import reparameterize_ray

        sensor = scene.sensors()[sensor_index]
        film = sensor.film()
        rfilter = film.reconstruction_filter()
        assert not rfilter.class_().name() == 'BoxFilter'
        assert film.has_high_quality_edges()

        sampler = sensor.sampler()
        if spp > 0:
            sampler.set_sample_count(spp)
        spp = sampler.sample_count()
        sampler.seed(seed, ek.hprod(film.crop_size()) * spp)

        # Sample primary rays
        ray, weight, pos, _, aperture_samples = sample_sensor_rays(sensor)

        # Read image gradient values per sample through the pixel filter
        block = ImageBlock(ek.detach(image_adj), rfilter, normalize=True)
        grad = Spectrum(block.read(pos)) * weight

        # Compute and splat (no pixel filter) directly visible light
        with ek.suspend_grad():
            Li = self.Li(None, scene, sampler, ray)[0]
        sampler.schedule_state()
        ek.eval(Li, grad)

        # Reparameterize primary rays
        reparam_d, reparam_div = reparameterize_ray(scene, sampler, ray, True,
                                                    params, self.num_aux_rays,
                                                    self.kappa, self.power)
        it = ek.zero(Interaction3f)
        it.p = ray.o + reparam_d
        ds, weight = sensor.sample_direction(it, aperture_samples)
        weight = ek.select(weight > 0.0, weight / ek.detach(weight), 1.0)

        block = ImageBlock(film.crop_size(), channel_count=5,
                           filter=rfilter, border=True)
        block.clear()
        block.put(ds.uv, ray.wavelengths, Li * weight)
        film.prepare(['R', 'G', 'B', 'A', 'W'])
        film.put(block)
        Li_attached = film.develop()

        ek.enqueue(ek.ADMode.Forward, params)
        ek.traverse(Float, ek.ADFlag.ClearEdges | ek.ADFlag.ClearInterior)

        Wk_grad = ek.grad(Li_attached) * image_adj

        div_grad = grad * Li * ek.grad(reparam_div)
        ek.eval(Wk_grad, div_grad)

        result = self.Li(ek.ADMode.Forward, scene, sampler,
                         ray, params=params, grad=grad)[0]

        block.clear()
        block.put(pos, ray.wavelengths, result + div_grad)
        film.prepare(['R', 'G', 'B', 'A', 'W'])
        film.put(block)

        return Wk_grad + film.develop()

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
        from mitsuba.render import ImageBlock, Interaction3f
        from mitsuba.python.ad import reparameterize_ray

        sensor = scene.sensors()[sensor_index]
        film = sensor.film()
        rfilter = film.reconstruction_filter()
        assert not rfilter.class_().name() == 'BoxFilter'
        assert film.has_high_quality_edges()

        sampler = sensor.sampler()
        if spp > 0:
            sampler.set_sample_count(spp)
        spp = sampler.sample_count()
        sampler.seed(seed, ek.hprod(film.crop_size()) * spp)

        ray, weight, pos, _, aperture_samples = sample_sensor_rays(sensor)

        # Read image gradient values per sample through the pixel filter
        block = ImageBlock(ek.detach(image_adj), rfilter, normalize=True)
        grad = Spectrum(block.read(pos)) * weight / spp

        with ek.suspend_grad():
            Li, _ = self.Li(None, scene, sampler, ray)
        sampler.schedule_state()
        ek.eval(Li, grad)

        # Reparameterize primary rays
        reparam_d, reparam_div = reparameterize_ray(scene, sampler, ray, True,
                                                    params, self.num_aux_rays,
                                                    self.kappa, self.power)
        it = ek.zero(Interaction3f)
        it.p = ray.o + reparam_d
        ds, weight = sensor.sample_direction(it, aperture_samples)
        weight = ek.select(weight > 0.0, weight / ek.detach(weight), 1.0)

        block = ImageBlock(film.crop_size(), channel_count=5,
                           filter=rfilter, border=True)
        block.clear()
        block.put(ds.uv, ray.wavelengths, Li * weight)
        film.prepare(['R', 'G', 'B', 'A', 'W'])
        film.put(block)
        Li_attached = film.develop()

        ek.set_grad(Li_attached, image_adj)
        ek.set_grad(reparam_div, ek.hsum(grad * Li))
        ek.enqueue(ek.ADMode.Reverse, Li_attached, reparam_div)
        ek.traverse(Float, ek.ADFlag.ClearVertices)

        self.Li(ek.ADMode.Reverse, scene, sampler, ray,
                params=params, grad=grad)

    def sample(self, scene, sampler, ray, medium, active):
        return *self.Li(None, scene, sampler, ray), []

    def Li(self: mitsuba.render.SamplingIntegrator,
           mode: ek.ADMode,
           scene: mitsuba.render.Scene,
           sampler: mitsuba.render.Sampler,
           ray_: mitsuba.core.Ray3f,
           depth: mitsuba.core.UInt32=1,
           params=mitsuba.python.util.SceneParameters(),
           grad: mitsuba.core.Spectrum=None,
           emission_weight: mitsuba.core.Float=None,
           active_: mitsuba.core.Mask=True):
        from mitsuba.core import Spectrum, Float, Mask, UInt32, Ray3f, Loop
        from mitsuba.render import DirectionSample3f, BSDFContext, BSDFFlags, has_flag, HitComputeFlags
        from mitsuba.python.ad import reparameterize_ray

        is_primal = mode is None

        def reparam(ray, active):
            return reparameterize_ray(scene, sampler, ray, active, params,
                                      num_auxiliary_rays=self.num_aux_rays,
                                      kappa=self.kappa, power=self.power)

        ray = Ray3f(ray_)
        prev_ray = Ray3f(ray_)

        pi = scene.ray_intersect_preliminary(ray, active_)
        valid_ray = active_ & pi.is_valid()

        # Initialize loop variables
        result     = Spectrum(0.0)
        throughput = Spectrum(1.0)
        active     = Mask(active_)
        if emission_weight is None:
            emission_weight = 1.0
        emission_weight = Float(emission_weight)

        depth_i = UInt32(depth)
        loop = Loop("RBPLoop" + '_recursive_li' if is_primal else '')
        loop.put(lambda:(depth_i, active, ray, prev_ray, emission_weight,
                         throughput, pi, result))
        sampler.loop_register(loop)
        loop.init()
        while loop(active):
            # Attach incoming direction (reparameterization from the previous bounce)
            si = pi.compute_surface_interaction(ray, HitComputeFlags.All, active)
            reparam_d, _ = reparam(prev_ray, active)
            si.wi = -si.to_local(reparam_d)

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
                ek.detach(si), sampler.next_2d(active_e), True, active_e)
            ds = ek.detach(ds, True)
            active_e &= ek.neq(ds.pdf, 0.0)

            ray_e = ek.detach(si.spawn_ray(ds.d))
            reparam_d, reparam_div = reparam(ray_e, active_e)
            wo = si.to_local(reparam_d)

            bsdf_val, bsdf_pdf = bsdf.eval_pdf(ctx, si, wo, active_e)
            mis = ek.select(ds.delta, 1.0, mis_weight(ds.pdf, bsdf_pdf))

            contrib = bsdf_val * emitter_val * throughput * mis
            accum += ek.select(active_e, contrib + reparam_div * ek.detach(contrib), 0.0)

            # ---------------------- BSDF sampling ----------------------

            with ek.suspend_grad():
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

            if not is_primal:
                with ek.suspend_grad():
                    # Account for incoming radiance
                    if self.recursive_li:
                        li = self.Li(None, scene, sampler, ray, depth_i+1,
                                     emission_weight=emission_weight,
                                     active_=active)[0]
                    else:
                        li = ds.emitter.eval(si_bsdf, active_b) * emission_weight

                reparam_d, reparam_div = reparam(ray, active)
                bsdf_eval = bsdf.eval(ctx, si, si.to_local(reparam_d), active)

                contrib = bsdf_eval * throughput * li / bs.pdf
                accum += ek.select(active, contrib + reparam_div * ek.detach(contrib), 0.0)

            if mode is ek.ADMode.Reverse:
                ek.backward(accum * grad, ek.ADFlag.ClearVertices)
            elif mode is ek.ADMode.Forward:
                ek.enqueue(ek.ADMode.Forward, params)
                ek.traverse(Float, ek.ADFlag.ClearEdges | ek.ADFlag.ClearInterior)
                result += ek.grad(accum) * grad
            else:
                result += accum

            # ------------------- Recurse to the next bounce -------------------

            pi = pi_bsdf
            throughput *= bsdf_weight

            depth_i += UInt32(1)

        return result, valid_ray

    def to_string(self):
        return f'RBReparamIntegrator[max_depth = {self.max_depth}]'


mitsuba.render.register_integrator("rbreparam", lambda props: RBReparamIntegrator(props))
