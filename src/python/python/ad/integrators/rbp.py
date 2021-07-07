import enoki as ek
import mitsuba

# TODO move this to a another place (could implement and bind in C++)
def sample_sensor_rays(sensor):
    from mitsuba.core import Float, UInt32, Vector2f

    film = sensor.film()
    sampler = sensor.sampler()
    film_size = film.crop_size()
    spp = sampler.sample_count()

    total_sample_count = ek.hprod(film_size) * spp

    if sampler.wavefront_size() != total_sample_count:
        sampler.seed(0, total_sample_count)

    pos_idx = ek.arange(UInt32, total_sample_count)
    pos_idx //= ek.opaque(UInt32, spp)
    scale = ek.rcp(Vector2f(film_size))
    pos = Vector2f(Float(pos_idx %  int(film_size[0])),
                   Float(pos_idx // int(film_size[0])))

    pos += sampler.next_2d()

    rays, weights = sensor.sample_ray_differential(
        time=0.0,
        sample1=sampler.next_1d(),
        sample2=pos * scale,
        sample3=0.0
    )

    return rays, weights, pos, pos_idx


class RBPIntegrator(mitsuba.render.SamplingIntegrator):
    """
    This integrator implements a basic Radiative Backpropagation path tracer.
    """
    def __init__(self, props=mitsuba.core.Properties()):
        super().__init__(props)
        self.max_depth = props.long_('max_depth', 4)
        self.recursive_li = props.bool_('recursive_li', True)

    def render_adjoint(self: mitsuba.render.SamplingIntegrator,
                       scene: mitsuba.render.Scene,
                       params: mitsuba.python.util.SceneParameters,
                       image_adj: mitsuba.core.Spectrum,
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
        rays, weights, _, pos_idx = sample_sensor_rays(sensor)

        grad_values = ek.gather(mitsuba.core.Spectrum, ek.detach(image_adj), pos_idx)
        grad_values *= weights / sampler.sample_count()

        for k, v in params.items():
            ek.enable_grad(v)

        self.sample_adjoint(scene, sensor.sampler(), rays, params, grad_values)

    def sample(self, scene, sampler, rays, medium, active):
        return *self.Li(scene, sampler, rays, params=None), []

    def sample_adjoint(self, scene, sampler, rays, params, grad):
        self.Li(scene, sampler, rays, params=params, grad=grad)

    def Li(self: mitsuba.render.SamplingIntegrator,
           scene: mitsuba.render.Scene,
           sampler: mitsuba.render.Sampler,
           rays: mitsuba.core.Ray3f,
           depth: mitsuba.core.UInt32=1,
           params: mitsuba.python.util.SceneParameters=None,
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
        from mitsuba.core import Spectrum, Float, UInt32, Mask, Ray3f, Loop
        from mitsuba.render import DirectionSample3f, BSDFContext, BSDFFlags, has_flag

        assert params is None or ek.detached_t(grad) or grad.index_ad() == 0
        is_primal = params is None

        def mis_weight(a, b):
            return ek.detach(ek.select(a > 0.0, (a**2) / (a**2 + b**2), 0.0))

        def adjoint(var, weight, active):
            if is_primal:
                return ek.select(active, var * weight, 0.0)
            else:
                ek.set_grad(var, ek.select(active, weight * grad, 0.0))
                ek.enqueue(var)
                ek.traverse(Float, reverse=True, retain_graph=False)
                return 0

        rays = Ray3f(rays)

        # Primary ray intersection
        si = scene.ray_intersect(rays, active_)
        valid_rays = active_ & si.is_valid()

        # Initialize loop variables
        result     = Spectrum(0.0)
        throughput = Spectrum(1.0)
        active     = active_ & si.is_valid()

        if emission_weight is None:
            emission_weight = Float(1.0)
        else:
            emission_weight = Float(emission_weight) # TODO is this needed to copy in recursive call?

        if not is_primal:
            params.set_grad_suspended(False)

        depth_i = UInt32(depth)
        loop = Loop("RBPLoop" + '_recursive_li' if is_primal else '')
        loop.put(lambda:(depth_i, active, rays, emission_weight, throughput, si, result))
        sampler.loop_register(loop)
        loop.init()
        while loop(active):

            # ---------------------- Direct emission ----------------------

            emitter = si.emitter(scene, active)
            result += adjoint(
                emitter.eval(si, active),
                throughput * emission_weight,
                active
            )

            if not is_primal:
                params.set_grad_suspended(True)

            active &= si.is_valid()
            active &= depth_i < self.max_depth

            ctx = BSDFContext()
            bsdf = si.bsdf()

            # ---------------------- Emitter sampling ----------------------

            active_e = active & has_flag(bsdf.flags(), BSDFFlags.Smooth)
            ds, emitter_val = scene.sample_emitter_direction(si, sampler.next_2d(active_e), True, active_e)
            active_e &= ek.neq(ds.pdf, 0.0)
            wo = si.to_local(ds.d)

            if not is_primal:
                params.set_grad_suspended(False)

            bsdf_val, bsdf_pdf = bsdf.eval_pdf(ctx, si, wo, active_e)
            mis = ek.select(ds.delta, 1.0, mis_weight(ds.pdf, bsdf_pdf))

            result += adjoint(
                bsdf_val,
                throughput * mis * emitter_val,
                active_e
            )

            # ---------------------- BSDF sampling ----------------------

            if not is_primal:
                params.set_grad_suspended(True)

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
                # Account for incoming radiance
                if self.recursive_li:
                    li = self.Li(scene, sampler, ek.detach(rays), depth_i+1,
                                 emission_weight=emission_weight,
                                 active_=active_b)[0]
                else:
                    li = ds.emitter.eval(si_bsdf, active_b) * emission_weight

                params.set_grad_suspended(False)

                bsdf_eval = bsdf.eval(ctx, si, ek.detach(bs.wo), active_b)

                adjoint(
                    bsdf_eval,
                    throughput * li / bs.pdf,
                    active_b
                )

            # ------------------- Recurse to the next bounce -------------------

            si = si_bsdf
            throughput *= ek.detach(bsdf_val)

            depth_i += UInt32(1)

        return result, valid_rays

    def to_string(self):
        return f'RBPIntegrator[max_depth = {self.max_depth}]'


mitsuba.render.register_integrator("rbp", lambda props: RBPIntegrator(props))
