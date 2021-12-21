from __future__ import annotations # Delayed parsing of type annotations
import mitsuba

def Li(self: mitsuba.render.SamplingIntegrator,
       mode: ek.ADMode,
       scene: mitsuba.render.Scene,
       sampler: mitsuba.render.Sampler,
       ray: mitsuba.core.RayDifferential3f,
       depth: mitsuba.core.UInt32=1,
       params=mitsuba.python.util.SceneParameters(),
       grad: mitsuba.core.Spectrum=None,
       active_: mitsuba.core.Mask=True,
       primal_result: mitsuba.core.Spectrum=None):

    from mitsuba.core import Spectrum, Float, Mask, UInt32, Loop, Ray3f
    from mitsuba.render import DirectionSample3f, BSDFContext, BSDFFlags, has_flag, RayFlags

    is_primal = mode is None

    ray = Ray3f(ray)
    pi = scene.ray_intersect_preliminary(ray, active_)
    valid_ray = active_ & pi.is_valid()

    result = Spectrum(0.0)
    if is_primal:
        primal_result = Spectrum(0.0)

    throughput = Spectrum(1.0)
    active = Mask(active_)
    emission_weight = Float(1.0)

    depth_i = UInt32(depth)
    loop = Loop("Path Replay Backpropagation main loop" + '' if is_primal else ' - adjoint')
    loop.put(lambda: (depth_i, active, ray, emission_weight, throughput, pi, result, primal_result, sampler))

    while loop(active):
        si = pi.compute_surface_interaction(ray, RayFlags.All, active)

        # ---------------------- Direct emission ----------------------

        emitter_val = si.emitter(scene, active).eval(si, active)
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

        accum += ek.select(active_e, bsdf_val * throughput * mis * emitter_val, 0.0)

        # Update accumulated radiance. When propagating gradients, we subtract the
        # emitter contributions instead of adding them
        if not is_primal:
            primal_result -= ek.detach(accum)

        # ---------------------- BSDF sampling ----------------------

        with ek.suspend_grad():
            bs, bsdf_weight = bsdf.sample(ctx, si, sampler.next_1d(active),
                                          sampler.next_2d(active), active)
            active &= bs.pdf > 0.0
            ray = si.spawn_ray(si.to_world(bs.wo))

        pi_bsdf = scene.ray_intersect_preliminary(ray, active)
        si_bsdf = pi_bsdf.compute_surface_interaction(ray, RayFlags.All, active)

        # Compute MIS weight for the BSDF sampling
        ds = DirectionSample3f(scene, si_bsdf, si)
        ds.emitter = si_bsdf.emitter(scene, active)
        delta = has_flag(bs.sampled_type, BSDFFlags.Delta)
        emitter_pdf = scene.pdf_emitter_direction(si, ds, ~delta)
        emission_weight = ek.select(delta, 1.0, mis_weight(bs.pdf, emitter_pdf))

        # Backpropagate gradients related to the current bounce
        if not is_primal:
            bsdf_eval = bsdf.eval(ctx, si, bs.wo, active)
            accum += ek.select(active, bsdf_eval * primal_result / ek.max(1e-8, ek.detach(bsdf_eval)), 0.0)

        if mode is ek.ADMode.Backward:
            ek.backward(accum * grad, ek.ADFlag.ClearVertices)
        elif mode is ek.ADMode.Forward:
            ek.enqueue(ek.ADMode.Forward, params)
            ek.traverse(Float, ek.ADFlag.ClearEdges | ek.ADFlag.ClearInterior)
            result += ek.grad(accum) * grad
        else:
            result += accum

        pi = pi_bsdf
        throughput *= bsdf_weight

        depth_i += UInt32(1)

    return result, valid_ray
