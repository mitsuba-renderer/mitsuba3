import enoki as ek
import mitsuba

class RBPIntegrator(mitsuba.render.SamplingIntegrator):
    """
    This integrator implements a basic Radiative Backpropagation path tracer.
    """
    def __init__(self, props=mitsuba.core.Properties()):
        super().__init__(props)
        self.max_depth = props.long_('max_depth', 4)
        self.recursive_li = props.bool_('recursive_li', False)

    def sample(self, scene, sampler, rays, medium, active):
        return *self.Li(scene, sampler, rays, params=None), []

    def adjoint_sample(self, scene, sampler, rays, params, grad):
        self.Li(scene, sampler, rays, params, grad)

    def Li(self: mitsuba.render.SamplingIntegrator,
           scene: mitsuba.render.Scene,
           sampler: mitsuba.render.Sampler,
           rays: mitsuba.core.Ray3f,
           params: mitsuba.python.util.SceneParameters=None,
           grad: mitsuba.core.Spectrum=None,
           emission_weight: mitsuba.core.Spectrum=None,
           active: mitsuba.core.Mask=True):
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

        is_primal = params is None

        def mis_weight(a, b):
            return ek.select(a > 0.0, (a**2) / (a**2 + b**2), 0.0)

        def adjoint(variables, active):
            # Clean NaN is variable's weights
            for v, w in variables.items():
                w[ek.isnan(w)] = 0.0

            if is_primal:
                result = 0
                for v, w in variables.items():
                    result += ek.select(active, v * w, 0.0)
                return result
            else:
                # Detach all variable's weights
                for v, w in variables.items():
                    w = ek.detach(w)

                # Backward mode, we need to accumulate parameter gradients
                for v, w in variables.items():
                    tmp_w = w * grad
                    if ek.is_dynamic_array_v(v) and ek.is_static_array_v(tmp_w):
                        tmp_w = ek.hsum(tmp_w)
                    ek.set_grad(v, ek.detach(tmp_w))
                    ek.enqueue(v)
                ek.traverse(Float, reverse=True, retain_graph=False)
                return 0

        rays = Ray3f(rays)

        # Primary ray intersection
        si = scene.ray_intersect(rays, active)
        valid_rays = active & si.is_valid()

        # Initialize loop variables
        result     = Spectrum(0.0)
        throughput = Spectrum(1.0)
        active     = active & si.is_valid()

        if emission_weight is None:
            emission_weight = Spectrum(1.0)

        if not is_primal:
            params.set_grad_suspended(False)

        i = UInt32(0)
        loop = Loop("RBPLoop" + '_recursive_li' if is_primal else '')
        loop.put(lambda:(i, active, rays, emission_weight, throughput, si, result))
        sampler.loop_register(loop)
        loop.init()
        while loop(active):

            # ---------------------- Direct emission ----------------------

            emitter = si.emitter(scene, active)
            result += adjoint({
                emitter.eval(si, active) : throughput * emission_weight
            }, active)

            if not is_primal:
                params.set_grad_suspended(True)

            active &= si.is_valid()
            active &= i < self.max_depth

            ctx = BSDFContext()
            bsdf = si.bsdf()

            # ---------------------- Emitter sampling ----------------------

            is_smooth_bsdf = has_flag(bsdf.flags(), BSDFFlags.Smooth)
            active_e = active & is_smooth_bsdf

            ds, emitter_val = scene.sample_emitter_direction(si, sampler.next_2d(active_e), True, active_e)
            active_e &= ek.neq(ds.pdf, 0.0)
            wo = si.to_local(ds.d)

            if not is_primal:
                params.set_grad_suspended(False)

            bsdf_val, bsdf_pdf = bsdf.eval_pdf(ctx, si, wo, active_e)
            mis = ek.select(ek.detach(ds.delta), 1.0, mis_weight(ds.pdf, bsdf_pdf))

            bsdf_val[~active_e] = 0.0
            emitter_val[~active_e] = 0.0

            result += adjoint({
                bsdf_val : throughput * mis * emitter_val
            }, active_e)

            # ---------------------- BSDF sampling ----------------------

            bs, bsdf_val = bsdf.sample(ctx, si, sampler.next_1d(active),
                                    sampler.next_2d(active), active)
            active &= bs.pdf > 0.0
            rays = si.spawn_ray(si.to_world(ek.detach(bs.wo)))
            si_bsdf = scene.ray_intersect(rays, active)

            # Compute MIS weights for the BSDF sampling
            emitter = si_bsdf.emitter(scene, active)
            ds = DirectionSample3f(scene, si_bsdf, si)
            ds.emitter = emitter
            delta = has_flag(bs.sampled_type, BSDFFlags.Delta)
            active_b = active & ek.neq(emitter, None) & ~delta
            emitter_pdf = ek.select(active_b, scene.pdf_emitter_direction(si, ds), 0.0)
            emission_weight = Spectrum(ek.detach(mis_weight(bs.pdf, emitter_pdf)))

            # Account for incoming radiance
            li = emitter.eval(si_bsdf, active_b)
            if self.recursive_li:
                li += self.Li(scene, sampler, ek.detach(rays),
                            emission_weight=emission_weight,
                            active=active_b)[0]
            else:
                li += Float(1.0)

            adjoint({
                bsdf_val : throughput * li / bs.pdf
            }, active_b)

            # ------------------- Recurse to the next bounce -------------------

            si = si_bsdf
            throughput *= ek.detach(bsdf_val)

            i += UInt32(1)

        return result, valid_rays

    def to_string(self):
        return f'RBPIntegrator[max_depth = {self.max_depth}]'


mitsuba.render.register_integrator("rbp", lambda props: RBPIntegrator(props))
