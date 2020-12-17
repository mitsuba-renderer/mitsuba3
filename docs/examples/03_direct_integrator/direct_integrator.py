import os
import enoki as ek
import mitsuba

# Set the desired mitsuba variant
mitsuba.set_variant('cuda_rgb')

from mitsuba.core import Float, Vector3f, Thread, xml
from mitsuba.core.xml import load_file
from mitsuba.render import (BSDF, BSDFContext, BSDFFlags,
                            DirectionSample3f, Emitter, ImageBlock,
                            SamplingIntegrator, has_flag,
                            register_integrator)


def mis_weight(pdf_a, pdf_b):
    pdf_a *= pdf_a
    pdf_b *= pdf_b
    return ek.select(pdf_a > 0.0, pdf_a / (pdf_a + pdf_b), Float(0.0))


def integrator_sample(scene, sampler, rays, medium, active=True):
    si = scene.ray_intersect(rays)
    active = si.is_valid() & active

    # Visible emitters
    result = si.emitter(scene, active).eval(si, active)

    ctx = BSDFContext()
    bsdf = si.bsdf(rays)

    # Emitter sampling
    active_e = active & has_flag(bsdf.flags(), BSDFFlags.Smooth)
    ds, emitter_val = scene.sample_emitter_direction(si, sampler.next_2d(active_e), True, active_e)
    active_e &= ek.neq(ds.pdf, 0.0)
    wo = si.to_local(ds.d)
    bsdf_val = bsdf.eval(ctx, si, wo, active_e)
    bsdf_pdf = bsdf.pdf(ctx, si, wo, active_e)
    mis = ek.select(ds.delta, Float(1), mis_weight(ds.pdf, bsdf_pdf))
    result += ek.select(active_e, emitter_val * bsdf_val * mis, Vector3f(0))

    # BSDF sampling
    active_b = active
    bs, bsdf_val = bsdf.sample(ctx, si, sampler.next_1d(active), sampler.next_2d(active), active_b)
    si_bsdf = scene.ray_intersect(si.spawn_ray(si.to_world(bs.wo)), active_b)
    emitter = si_bsdf.emitter(scene, active_b)
    active_b &= ek.neq(emitter, None)
    emitter_val = emitter.eval(si_bsdf, active_b)
    delta = has_flag(bs.sampled_type, BSDFFlags.Delta)
    ds = DirectionSample3f(scene, si_bsdf, si)
    emitter_pdf = ek.select(delta, Float(0), scene.pdf_emitter_direction(si, ds, active_b))
    result += ek.select(active_b, bsdf_val * emitter_val * mis_weight(bs.pdf, emitter_pdf), Vector3f(0))
    return result, si.is_valid(), ek.select(si.is_valid(), si.t, Float(0.0))


class MyDirectIntegrator(SamplingIntegrator):
    def __init__(self, props):
        SamplingIntegrator.__init__(self, props)

    def sample(self, scene, sampler, ray, medium, active):
        result, is_valid, depth = integrator_sample(scene, sampler, ray, medium, active)
        return result, is_valid, [depth]

    def aov_names(self):
        return ["depth.Y"]

    def to_string(self):
        return "MyDirectIntegrator[]"


# Register our integrator such that the XML file loader can instantiate it when loading a scene
register_integrator("mydirectintegrator", lambda props: MyDirectIntegrator(props))

# Load an XML file which specifies "mydirectintegrator" as the scene's integrator


scene = load_file('path/to/my/scene.xml')

scene.integrator().render(scene, scene.sensors()[0])

film = scene.sensors()[0].film()
film.set_destination_file('my-direct-integrator.exr')
film.develop()
