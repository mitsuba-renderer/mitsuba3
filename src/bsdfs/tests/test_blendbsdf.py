import pytest
import drjit as dr
import mitsuba as mi


def test01_create(variant_scalar_rgb):
    bsdf = mi.load_dict({
        'type': 'blendbsdf',
        'nested1': { 'type' : 'diffuse' },
        'nested2': { 'type' : 'diffuse' },
        'weight': 0.2,
    })
    assert bsdf is not None
    assert bsdf.component_count() == 2
    assert bsdf.flags(0) == mi.BSDFFlags.DiffuseReflection | mi.BSDFFlags.FrontSide
    assert bsdf.flags(1) == mi.BSDFFlags.DiffuseReflection | mi.BSDFFlags.FrontSide
    assert bsdf.flags() == bsdf.flags(0) | bsdf.flags(1)

    bsdf = mi.load_dict({
        'type': 'blendbsdf',
        'nested1': { 'type' : 'roughconductor' },
        'nested2': { 'type' : 'diffuse' },
        'weight': 0.2,
    })
    assert bsdf is not None
    assert bsdf.component_count() == 2
    assert bsdf.flags(0) == mi.BSDFFlags.GlossyReflection | mi.BSDFFlags.FrontSide
    assert bsdf.flags(1) == mi.BSDFFlags.DiffuseReflection | mi.BSDFFlags.FrontSide
    assert bsdf.flags() == bsdf.flags(0) | bsdf.flags(1)


def test02_eval_all(variant_scalar_rgb):
    weight = 0.2

    bsdf = mi.load_dict({
        'type': 'blendbsdf',
        'nested1': {
            'type': 'diffuse',
            'reflectance': {'type': 'rgb', 'value': 0.0}
        },
        'nested2': {
            'type': 'diffuse',
            'reflectance': {'type': 'rgb', 'value': 1.0}
        },
        'weight': weight,
    })

    si = mi.SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wi = [0, 0, 1]

    wo = [0, 0, 1]
    ctx = mi.BSDFContext()

    # Evaluate the blend of both components
    expected = (1 - weight) * 0.0 * dr.inv_pi + weight * 1.0 * dr.inv_pi
    value    = bsdf.eval(ctx, si, wo)
    assert dr.allclose(value, expected)


def test03_eval_components(variant_scalar_rgb):
    weight = 0.2

    bsdf = mi.load_dict({
        'type': 'blendbsdf',
        'nested1': {
            'type': 'diffuse',
            'reflectance': {'type': 'rgb', 'value': 0.0}
        },
        'nested2': {
            'type': 'diffuse',
            'reflectance': {'type': 'rgb', 'value': 1.0}
        },
        'weight': weight,
    })

    si = mi.SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wi = [0, 0, 1]

    wo = [0, 0, 1]
    ctx = mi.BSDFContext()

    # Evaluate the two components separately

    ctx.component = 0
    value0 = bsdf.eval(ctx, si, wo)
    expected0 = (1-weight) * 0.0*dr.inv_pi
    assert dr.allclose(value0, expected0)

    ctx.component = 1
    value1 = bsdf.eval(ctx, si, wo)
    expected1 = weight * 1.0*dr.inv_pi
    assert dr.allclose(value1, expected1)


def test04_sample_all(variant_scalar_rgb):
    weight = 0.2

    bsdf = mi.load_dict({
        'type': 'blendbsdf',
        'nested1': {
            'type': 'diffuse',
            'reflectance': {'type': 'rgb', 'value': 0.0}
        },
        'nested2': {
            'type': 'diffuse',
            'reflectance': {'type': 'rgb', 'value': 1.0}
        },
        'weight': weight,
    })

    si = mi.SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wi = [0, 0, 1]

    ctx = mi.BSDFContext()

    # Sample using two different values of 'sample1' and make sure correct
    # components are chosen.

    expected_a = 1.0    # InvPi & weight will cancel out with sampling pdf
    bs_a, weight_a = bsdf.sample(ctx, si, 0.1, [0.5, 0.5])
    assert dr.allclose(weight_a, expected_a)

    expected_b = 0.0    # InvPi & weight will cancel out with sampling pdf
    bs_b, weight_b = bsdf.sample(ctx, si, 0.3, [0.5, 0.5])
    assert dr.allclose(weight_b, expected_b)


def test05_sample_components(variant_scalar_rgb):
    weight = 0.2

    bsdf = mi.load_dict({
        'type': 'blendbsdf',
        'nested1': {
            'type': 'diffuse',
            'reflectance': {'type': 'rgb', 'value': 0.0}
        },
        'nested2': {
            'type': 'diffuse',
            'reflectance': {'type': 'rgb', 'value': 1.0}
        },
        'weight': weight,
    })

    si = mi.SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wi = [0, 0, 1]

    ctx = mi.BSDFContext()

    # Sample specific components separately using two different values of 'sample1'
    # and make sure the desired component is chosen always.

    ctx.component = 0

    expected_a = (1-weight)*0.0    # InvPi will cancel out with sampling pdf, but still need to apply weight
    bs_a, weight_a = bsdf.sample(ctx, si, 0.1, [0.5, 0.5])
    assert dr.allclose(weight_a, expected_a)

    expected_b = (1-weight)*0.0    # InvPi will cancel out with sampling pdf, but still need to apply weight
    bs_b, weight_b = bsdf.sample(ctx, si, 0.3, [0.5, 0.5])
    assert dr.allclose(weight_b, expected_b)

    ctx.component = 1

    expected_a = weight*1.0    # InvPi will cancel out with sampling pdf, but still need to apply weight
    bs_a, weight_a = bsdf.sample(ctx, si, 0.1, [0.5, 0.5])
    assert dr.allclose(weight_a, expected_a)

    expected_b = weight*1.0    # InvPi will cancel out with sampling pdf, but still need to apply weight
    bs_b, weight_b = bsdf.sample(ctx, si, 0.3, [0.5, 0.5])
    assert dr.allclose(weight_b, expected_b)
