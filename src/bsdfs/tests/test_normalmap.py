import pytest
import drjit as dr
import mitsuba as mi
import numpy as np

def test01_empty(variants_vec_backends_once_rgb):
    raw_bsdf : mi.BSDF = mi.load_dict({
        'type': 'roughconductor',
        'distribution': 'ggx',
        'alpha_u': 0.2,
        'alpha_v': 0.05,
        'material': 'Al'
    })
    normalmap_bsdf : mi.BSDF = mi.load_dict(
    {
        'type': 'normalmap',
        'normalmap': {
            'type': 'srgb',
            'color': [ 0.5, 0.5, 1.0 ]
        },
        'nested_bsdf': raw_bsdf
    })

    rng : mi.Sampler = mi.load_dict({
        'type': 'ldsampler'
    })
    rng.seed(0, 1024)
    sample1 = rng.next_1d()
    sample2 = rng.next_2d()

    si = mi.SurfaceInteraction3f()
    si.sh_frame = mi.Frame3f(mi.Vector3f(1, 1, 1))
    si.wi = mi.Vector3f(0, 0, 1)

    bs_ref, weight_ref = raw_bsdf.sample(mi.BSDFContext(), si, sample1, sample2)
    bs_test, weight_test = normalmap_bsdf.sample(mi.BSDFContext(), si, sample1, sample2)

    # ignore quantities for zero-weight samples!
    bs_ref.wo[dr.all(weight_ref == 0)] = 0
    bs_test.wo[dr.all(weight_test == 0)] = 0
    bs_ref.pdf[dr.all(weight_ref == 0)] = 0
    bs_test.pdf[dr.all(weight_test == 0)] = 0

    assert dr.allclose(bs_ref.wo, bs_test.wo)
    assert dr.allclose(bs_ref.pdf, bs_test.pdf)
    assert dr.allclose(weight_ref, weight_test)


def test02_tilted(variants_vec_backends_once_rgb):
    # Construct a "local" frame representing a normal perturbation
    sin_theta = 0.5
    cos_theta = 0.5*dr.sqrt(3)
    s = mi.ScalarVector3f(cos_theta, 0, -sin_theta)
    t = mi.ScalarVector3f(0, 1, 0)
    n = mi.ScalarVector3f(sin_theta, 0, cos_theta)
    normal_frame = mi.Frame3f(s, t, n)
    raw_bsdf : mi.BSDF = mi.load_dict({
        'type': 'roughconductor',
        'distribution': 'ggx',
        'alpha_u': 0.2,
        'alpha_v': 0.05,
        'material': 'Al'
    })
    perturbed_bsdf : mi.BSDF = mi.load_dict(
    {
        'type': 'normalmap',
        'normalmap': {
            'type': 'srgb',
            'color': n * 0.5 + 0.5
        },
        'nested_bsdf': raw_bsdf
    })

    rng : mi.Sampler = mi.load_dict({
        'type': 'ldsampler'
    })
    rng.seed(0, 1024)
    sample1 = rng.next_1d()
    sample2 = rng.next_2d()

    si_original = mi.SurfaceInteraction3f()
    si_original.sh_frame = mi.Frame3f(mi.Vector3f(1, 1, 1))
    si_original.wi = mi.Vector3f(0, 0, 1)

    si_perturbed = mi.SurfaceInteraction3f()
    si_perturbed.sh_frame = mi.Frame3f(si_original.to_world(normal_frame.s), si_original.to_world(normal_frame.t), si_original.to_world(normal_frame.n))
    si_perturbed.wi = normal_frame.to_local(si_original.wi)

    bs_raw, weight_raw = raw_bsdf.sample(mi.BSDFContext(), si_perturbed, sample1, sample2)
    bs_test, weight_test = perturbed_bsdf.sample(mi.BSDFContext(), si_original, sample1, sample2)

    # move raw wo to into the perturbed normal frame
    wo_raw = normal_frame.to_world(bs_raw.wo)
    wo_test = bs_test.wo

    # mask invalid "raw" samples with inconsistent surface orientations
    bs_raw.pdf[wo_raw.z * bs_raw.wo.z <= 0] = 0
    weight_raw[wo_raw.z * bs_raw.wo.z <= 0] = 0

    # ignore quantities for zero-weight samples!
    wo_raw[dr.all(weight_raw == 0)] = 0
    wo_test[dr.all(weight_test == 0)] = 0
    bs_raw.pdf[dr.all(weight_raw == 0)] = 0
    bs_test.pdf[dr.all(weight_test == 0)] = 0

    assert dr.allclose(wo_raw, wo_test, atol=1e-4)
    assert dr.allclose(bs_raw.pdf, bs_test.pdf, atol=1e-4)
    assert dr.allclose(weight_raw, weight_test, atol=2e-4)
