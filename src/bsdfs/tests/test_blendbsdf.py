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

    bsdf_0 = mi.load_dict({
        'type': 'conductor'
    })

    bsdf_1 = mi.load_dict({
        'type': 'diffuse',
        'reflectance': {'type': 'rgb', 'value': 1.0}
    })

    bsdf = mi.load_dict({
        'type': 'blendbsdf',
        'nested1': bsdf_0,
        'nested2': bsdf_1,
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
    expected = (1 - weight) * bsdf_0.eval(ctx, si, wo) + \
                weight * bsdf_1.eval(ctx, si, wo)
    value    = bsdf.eval(ctx, si, wo)
    assert dr.allclose(value, expected)


def test03_eval_components(variant_scalar_rgb):
    weight = 0.2

    bsdf_0 = mi.load_dict({
        'type': 'conductor'
    })

    bsdf_1 = mi.load_dict({
        'type': 'diffuse',
        'reflectance': {'type': 'rgb', 'value': 1.0}
    })

    bsdf = mi.load_dict({
        'type': 'blendbsdf',
        'nested1': bsdf_0,
        'nested2': bsdf_1,
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
    blend_ctx = mi.BSDFContext()

    # Evaluate the two components separately

    blend_ctx.component = 0
    value0 = bsdf.eval(blend_ctx, si, wo)
    expected0 = (1.0-weight) * bsdf_0.eval(ctx, si, wo)
    assert dr.allclose(value0, expected0)

    blend_ctx.component = 1
    value1 = bsdf.eval(blend_ctx, si, wo)
    expected1 = weight * bsdf_1.eval(ctx, si, wo)
    assert dr.allclose(value1, expected1)


def test04_sample_all(variant_scalar_rgb):
    weight = 0.2

    bsdf = mi.load_dict({
        'type': 'blendbsdf',
        'nested1': {
            'type': 'plastic'
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

    bs_a, weight_a = bsdf.sample(ctx, si, 0.1, [0.2, 0.9])
    eval_a, pdf_a = bsdf.eval_pdf(ctx, si, wo=bs_a.wo)
    expected_a = eval_a / pdf_a
    assert dr.allclose(weight_a, expected_a)

    bs_b, weight_b = bsdf.sample(ctx, si, 0.3, [0.4, 0.6])
    eval_b, pdf_b = bsdf.eval_pdf(ctx, si, wo=bs_b.wo)
    expected_b = eval_b / pdf_b
    assert dr.allclose(weight_b, expected_b)


def test05_sample_components(variant_scalar_rgb):
    weight = 0.2

    bsdf_0 = mi.load_dict({
        'type': 'conductor'
    })

    bsdf_1 = mi.load_dict({
        'type': 'diffuse',
        'reflectance': {'type': 'rgb', 'value': 1.0}
    })

    bsdf = mi.load_dict({
        'type': 'blendbsdf',
        'nested1': bsdf_0,
        'nested2': bsdf_1,
        'weight': weight,
    })

    si = mi.SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wi = [0, 0, 1]

    ctx = mi.BSDFContext()
    blend_ctx = mi.BSDFContext()

    # Sample specific components separately using two different values of 'sample1'
    # and make sure the desired component is chosen always.

    blend_ctx.component = 0

    expected_bs_a, expected_a = bsdf_0.sample(ctx, si, 0.1, [0.5, 0.5])
    bs_a, weight_a = bsdf.sample(blend_ctx, si, 0.1, [0.5, 0.5])
    assert dr.allclose(weight_a, (1.0-weight) * expected_a)
    assert dr.allclose(bs_a.wo, expected_bs_a.wo)
    assert dr.allclose(bs_a.pdf, expected_bs_a.pdf)

    expected_bs_b, expected_b = bsdf_0.sample(ctx, si, 0.3, [0.2, 0.7])
    bs_b, weight_b = bsdf.sample(blend_ctx, si, 0.3, [0.2, 0.7])
    assert dr.allclose(weight_b, (1.0-weight) * expected_b)
    assert dr.allclose(bs_b.wo, expected_bs_b.wo)
    assert dr.allclose(bs_b.pdf, expected_bs_b.pdf)

    blend_ctx.component = 1

    expected_bs_a, expected_a = bsdf_1.sample(ctx, si, 0.1, [0.5, 0.5])
    bs_a, weight_a = bsdf.sample(blend_ctx, si, 0.1, [0.5, 0.5])
    assert dr.allclose(weight_a, weight * expected_a)
    assert dr.allclose(bs_a.wo, expected_bs_a.wo)
    assert dr.allclose(bs_a.pdf, expected_bs_a.pdf)

    expected_bs_b, expected_b = bsdf_1.sample(ctx, si, 0.3, [0.2, 0.7])
    bs_b, weight_b = bsdf.sample(blend_ctx, si, 0.3, [0.2, 0.7])
    assert dr.allclose(weight_b, weight * expected_b)
    assert dr.allclose(bs_b.wo, expected_bs_b.wo)
    assert dr.allclose(bs_b.pdf, expected_bs_b.pdf)

def test06_chi2_diffuse(variants_vec_backends_once_rgb):
    from mitsuba.chi2 import BSDFAdapter, ChiSquareTest, SphericalDomain

    sample_func, pdf_func = BSDFAdapter("blendbsdf", {
        'type': 'blendbsdf',
        'nested1': { 'type' : 'diffuse' },
        'nested2': { 'type' : 'diffuse' },
        'weight': 0.2,
    })

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()

def test07_chi2_diffuse_roughdielectric(variants_vec_backends_once_rgb):
    from mitsuba.chi2 import BSDFAdapter, ChiSquareTest, SphericalDomain

    sample_func, pdf_func = BSDFAdapter("blendbsdf", {
        'type': 'blendbsdf',
        'nested1': { 'type' : 'diffuse' },
        'nested2': { 'type' : 'roughdielectric', 'alpha': 0.5 },
        'weight': 0.2,
    })

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()

def test08_chi2_roughconductor_roughdielectric(variants_vec_backends_once_rgb):
    from mitsuba.chi2 import BSDFAdapter, ChiSquareTest, SphericalDomain

    sample_func, pdf_func = BSDFAdapter("blendbsdf", {
        'type': 'blendbsdf',
        'nested1': { 'type' : 'roughconductor', 'alpha': 0.5 },
        'nested2': { 'type' : 'roughdielectric', 'alpha': 0.5 },
        'weight': 0.2,
    })

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()
