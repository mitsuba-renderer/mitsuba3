import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


def test01_empty(variants_vec_backends_once_rgb):
    raw_bsdf: mi.BSDF = mi.load_dict({
        'type': 'roughconductor',
        'distribution': 'ggx',
        'alpha_u': 0.4,
        'alpha_v': 0.1,
        'material': 'Al'
    })
    normalmap_bsdf: mi.BSDF = mi.load_dict(
        {
            'type': 'normalmap',
            'normalmap': {
                'type': 'srgb',
                'color': [0.5, 0.5, 1.0]
            },
            'nested_bsdf': raw_bsdf
        })

    rng: mi.Sampler = mi.load_dict({
        'type': 'ldsampler'
    })
    rng.seed(0, 1024)
    sample1 = rng.next_1d()
    sample2 = rng.next_2d()

    si = mi.SurfaceInteraction3f()
    si.sh_frame = mi.Frame3f(mi.Vector3f(1, 1, 1))
    si.wi = mi.Vector3f(0, 0, 1)

    bs_ref, weight_ref = raw_bsdf.sample(
        mi.BSDFContext(), si, sample1, sample2)
    bs_test, weight_test = normalmap_bsdf.sample(
        mi.BSDFContext(), si, sample1, sample2)

    # ignore quantities for zero-weight samples!
    mask = dr.all(weight_ref == 0)
    bs_ref.wo[mask] = 0
    bs_test.wo[mask] = 0
    bs_ref.pdf[mask] = 0
    bs_test.pdf[mask] = 0

    # This bounds of this test are somewhat lax. Tiny errors of ~1 ULP in the
    # incident direction (thanks to rsqrt()) are greatly magnified by the GGX sampling
    assert dr.allclose(bs_ref.wo, bs_test.wo, atol=1e-3)
    assert dr.allclose(bs_ref.pdf, bs_test.pdf, atol=1e-4)
    assert dr.allclose(weight_ref, weight_test, atol=1e-2)


def test02_tilted(variants_vec_backends_once_rgb):
    # Construct a "local" frame representing a normal perturbation
    sin_theta = 0.5
    cos_theta = 0.5*dr.sqrt(3)
    s = mi.ScalarVector3f(cos_theta, 0, -sin_theta)
    t = mi.ScalarVector3f(0, 1, 0)
    n = mi.ScalarVector3f(sin_theta, 0, cos_theta)
    normal_frame = mi.Frame3f(s, t, n)
    raw_bsdf: mi.BSDF = mi.load_dict({
        'type': 'roughconductor',
        'distribution': 'ggx',
        'alpha_u': 0.2,
        'alpha_v': 0.05,
        'material': 'Al'
    })
    perturbed_bsdf: mi.BSDF = mi.load_dict(
        {
            'type': 'normalmap',
            'normalmap': {
                'type': 'srgb',
                'color': n * 0.5 + 0.5
            },
            'nested_bsdf': raw_bsdf,
            'use_shadowing_function': False
        })

    rng: mi.Sampler = mi.load_dict({
        'type': 'ldsampler'
    })
    rng.seed(0, 1024)
    sample1 = rng.next_1d()
    sample2 = rng.next_2d()

    si_original = mi.SurfaceInteraction3f()
    si_original.sh_frame = mi.Frame3f(mi.Vector3f(1, 1, 1))
    si_original.wi = mi.Vector3f(0, 0, 1)

    si_perturbed = mi.SurfaceInteraction3f()
    si_perturbed.sh_frame = mi.Frame3f(si_original.to_world(
        normal_frame.s), si_original.to_world(normal_frame.t), si_original.to_world(normal_frame.n))
    si_perturbed.wi = normal_frame.to_local(si_original.wi)

    bs_raw, weight_raw = raw_bsdf.sample(
        mi.BSDFContext(), si_perturbed, sample1, sample2)
    bs_test, weight_test = perturbed_bsdf.sample(
        mi.BSDFContext(), si_original, sample1, sample2)

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


def test03_flip_invalid_normals(variants_vec_backends_once_rgb):
    # Ensure that the "flip_invalid_normals" option works as intended.
    wo = dr.normalize(mi.Vector3f(0, 0, 1))
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.sh_frame = mi.Frame3f(mi.Vector3f(0, 0, 1))
    si.wi = dr.normalize(mi.Vector3f(-1, -1, 0.5))

    n = dr.normalize(mi.ScalarVector3f(1, 1, 1))
    bsdf_dict = {
        'type': 'normalmap',
        'normalmap': {
                'type': 'srgb',
                'color': n * 0.5 + 0.5
        },
        'nested_bsdf': {'type': 'diffuse'},
        'flip_invalid_normals': False
    }
    context = mi.BSDFContext()
    bsdf1 = mi.load_dict(bsdf_dict)
    assert dr.allclose(bsdf1.sample(context, si, 0.5, (0.5, 0.5))[1], 0)
    assert dr.allclose(bsdf1.eval(context, si, wo), 0)
    assert dr.allclose(bsdf1.eval_pdf(context, si, wo)[0], 0)

    bsdf_dict['flip_invalid_normals'] = True
    bsdf2 = mi.load_dict(bsdf_dict)
    assert dr.all(bsdf2.sample(context, si, 0.5, (0.5, 0.5))[1] > 0)
    assert dr.all(bsdf2.eval(context, si, wo) > 0)
    assert dr.all(bsdf2.eval_pdf(context, si, wo)[0] > 0)


def test04_use_shadowing_function(variants_vec_backends_once_rgb):
    # Without the shadowing term, we get a full response if
    # the shading normal aligns with wo, even at grazing angles.
    wo = dr.normalize(mi.Vector3f(1, 0.01, 0.01))
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.sh_frame = mi.Frame3f(mi.Vector3f(0, 0, 1))
    si.wi = dr.normalize(mi.Vector3f(0, 0, 1))

    n = dr.normalize(mi.ScalarVector3f(1, 0.01, 0.01))
    bsdf_dict = {
        'type': 'normalmap',
        'normalmap': {
                'type': 'srgb',
                'color': n * 0.5 + 0.5
        },
        'nested_bsdf': {'type': 'diffuse', 'reflectance': 1.0},
        'use_shadowing_function': False
    }
    bsdf1 = mi.load_dict(bsdf_dict)
    context = mi.BSDFContext()
    assert dr.allclose(bsdf1.sample(context, si, 0.5, (0.5, 0.5))[1], 1.0)
    assert dr.allclose(bsdf1.eval(context, si, wo), dr.inv_pi, atol=1e-2)
    assert dr.allclose(bsdf1.eval_pdf(context, si, wo)
                       [0], dr.inv_pi, atol=1e-2)

    # With the shadowing term, the response decreases to almost zero.
    bsdf_dict['use_shadowing_function'] = True
    bsdf2 = mi.load_dict(bsdf_dict)

    assert dr.all(bsdf2.sample(context, si, 0.5, (0.5, 0.5))[1] < 0.05)
    assert dr.all(bsdf2.eval(context, si, wo) < 0.01)
    assert dr.all(bsdf2.eval_pdf(context, si, wo)[0] < 0.01)
