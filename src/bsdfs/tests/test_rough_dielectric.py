import pytest
import drjit as dr
import mitsuba as mi

# TODO this is way too slow
# @pytest.mark.slow
# def test01_chi2_smooth(variants_vec_backends_once_rgb):
#     from mitsuba.core import mi.ScalarVector3f
#     xml = """<float name="alpha" value="0.05"/>"""
#     wi = dr.normalize(mi.ScalarVector3f(0.8, 0.3, 0.05))
#     sample_func, pdf_func = mi.chi2.BSDFAdapter("roughdielectric", xml, wi=wi)

#     chi2 = mi.chi2.ChiSquareTest(
#         domain=mi.chi2.SphericalDomain(),
#         sample_func=sample_func,
#         pdf_func=pdf_func,
#         sample_dim=3,
#         res=201,
#         ires=32
#     )

#     assert chi2.run()


def test02_chi2_rough_grazing(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.5"/>"""
    wi = dr.normalize(mi.ScalarVector3f(0.8, 0.3, 0.05))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        seed=4
    )

    assert chi2.run()


def test03_chi2_rough_beckmann_all(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.5"/>
             <boolean name="sample_visible" value="false"/>
             <string name="distribution" value="beckmann"/>
          """
    wi = dr.normalize(mi.ScalarVector3f(0.5, 0.0, 0.5))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201
    )

    assert chi2.run()


def test04_chi2_rough_beckmann_visible(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.5"/>
             <boolean name="sample_visible" value="true"/>
             <string name="distribution" value="beckmann"/>
          """
    wi = dr.normalize(mi.ScalarVector3f(0.5, 0.0, 0.5))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        seed=2
    )

    assert chi2.run()


def test05_chi2_rough_ggx_all(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.5"/>
             <boolean name="sample_visible" value="false"/>
             <string name="distribution" value="ggx"/>
          """
    wi = dr.normalize(mi.ScalarVector3f(0.5, 0.0, 0.5))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201
    )

    assert chi2.run()


def test06_chi2_rough_ggx_visible(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.5"/>
             <boolean name="sample_visible" value="true"/>
             <string name="distribution" value="ggx"/>
          """
    wi = dr.normalize(mi.ScalarVector3f(0.5, 0.5, 0.001))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        seed=2
    )

    assert chi2.run()


def test07_chi2_rough_from_inside(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.5"/>"""
    wi = dr.normalize(mi.ScalarVector3f(0.2, -0.6, -0.5))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201
    )

    assert chi2.run()


def test08_chi2_rough_from_inside_tir(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.5"/>"""
    wi = dr.normalize(mi.ScalarVector3f(0.8, 0.3, -0.05))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        seed=8
    )

    assert chi2.run()


def test09_chi2_rough_from_denser(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.5"/>
             <float name="ext_ior" value="1.5"/>
             <float name="int_ior" value="1"/>"""
    wi = dr.normalize(mi.ScalarVector3f(0.2, -0.6, 0.5))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()


def test10_chi2_lobe_refl(variants_vec_backends_once_rgb):
    xml = """
    <float name="alpha_u" value="0.5"/>
    <float name="alpha_v" value="0.2"/>
    """
    wi = dr.normalize(mi.ScalarVector3f(-0.5, -0.5, 0.1))
    ctx = mi.BSDFContext()
    ctx.component = 0
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughdielectric", xml, wi=wi, ctx=ctx)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201,
        seed=2
    )

    assert chi2.run()


def test11_chi2_aniso_lobe_trans(variants_vec_backends_once_rgb):
    xml = """
    <float name="alpha_u" value="0.5"/>
    <float name="alpha_v" value="0.2"/>
    """
    wi = dr.normalize(mi.ScalarVector3f(-0.5, -0.5, 0.1))
    ctx = mi.BSDFContext()
    ctx.component = 1
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughdielectric", xml, wi=wi, ctx=ctx)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201
    )

    assert chi2.run()


def test12_eval_pdf(variant_scalar_rgb):
    bsdf = mi.load_dict({'type': 'roughdielectric'})

    si    = mi.SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.wi = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)

    ctx = mi.BSDFContext()

    for i in range(20):
        theta = i / 19.0 * (dr.pi / 2)
        wo = [dr.sin(theta), 0, dr.cos(theta)]

        v_pdf  = bsdf.pdf(ctx, si, wo=wo)
        v_eval = bsdf.eval(ctx, si, wo=wo)[0]
        v_eval_pdf = bsdf.eval_pdf(ctx, si, wo=wo)
        assert dr.allclose(v_eval, v_eval_pdf[0])
        assert dr.allclose(v_pdf, v_eval_pdf[1])


def test13_attached_sampling(variants_all_ad_rgb):
    bsdf = mi.load_dict({'type': 'roughdielectric', 'alpha' : 0.5})

    angle = mi.Float(0)
    dr.enable_grad(angle)

    si    = mi.SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [dr.sin(angle), 0, dr.cos(angle)]
    si.sh_frame = mi.Frame3f(si.n)
    si.wi = si.sh_frame.to_local([0, 0, 1])

    bs, weight = bsdf.sample(mi.BSDFContext(), si, 0, [0.3, 0.3])
    assert dr.grad_enabled(weight)

    dr.forward(angle)
    assert dr.allclose(dr.grad(weight), 0.02079637348651886)
    