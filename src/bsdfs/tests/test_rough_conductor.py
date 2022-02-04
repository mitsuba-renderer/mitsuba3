import drjit as dr

from mitsuba.python.chi2 import ChiSquareTest, BSDFAdapter, SphericalDomain


def test01_chi2_smooth(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    xml = """<float name="alpha" value="0.05"/>"""
    wi = dr.normalize(ScalarVector3f(1.0, 1.0, 1.0))
    sample_func, pdf_func = BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=140,
        ires=32
    )

    assert chi2.run()


def test02_chi2_aniso_beckmann_all(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    xml = """<float name="alpha_u" value="0.2"/>
             <float name="alpha_v" value="0.05"/>
             <string name="distribution" value="beckmann"/>
             <boolean name="sample_visible" value="false"/>"""
    wi = dr.normalize(ScalarVector3f(1.0, 1.0, 1.0))
    sample_func, pdf_func = BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        ires=16
    )

    assert chi2.run()


def test03_chi2_aniso_beckmann_visible(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    xml = """<float name="alpha_u" value="0.2"/>
             <float name="alpha_v" value="0.05"/>
             <string name="distribution" value="beckmann"/>
             <boolean name="sample_visible" value="true"/>"""
    wi = dr.normalize(ScalarVector3f(1.0, 1.0, 1.0))
    sample_func, pdf_func = BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        ires=16,
        seed=2
    )

    assert chi2.run()


def test04_chi2_aniso_ggx_all(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    xml = """<float name="alpha_u" value="0.2"/>
             <float name="alpha_v" value="0.05"/>
             <string name="distribution" value="ggx"/>
             <boolean name="sample_visible" value="false"/>"""
    wi = dr.normalize(ScalarVector3f(1.0, 1.0, 1.0))
    sample_func, pdf_func = BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        ires=8
    )

    assert chi2.run()


def test05_chi2_aniso_ggx_visible(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    xml = """<float name="alpha_u" value="0.2"/>
             <float name="alpha_v" value="0.05"/>
             <string name="distribution" value="ggx"/>
             <boolean name="sample_visible" value="true"/>"""
    wi = dr.normalize(ScalarVector3f(1.0, 1.0, 1.0))
    sample_func, pdf_func = BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        ires=8
    )

    assert chi2.run()


def test06_eval_pdf(variant_scalar_rgb):
    from mitsuba.core import load_string, Frame3f
    from mitsuba.render import BSDFContext, SurfaceInteraction3f

    bsdf = load_string("<bsdf version='2.0.0' type='roughconductor'></bsdf>")

    si    = SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.wi = [0, 0, 1]
    si.sh_frame = Frame3f(si.n)

    ctx = BSDFContext()

    for i in range(20):
        theta = i / 19.0 * (dr.Pi / 2)
        wo = [dr.sin(theta), 0, dr.cos(theta)]

        v_pdf  = bsdf.pdf(ctx, si, wo=wo)
        v_eval = bsdf.eval(ctx, si, wo=wo)[0]
        v_eval_pdf = bsdf.eval_pdf(ctx, si, wo=wo)
        assert dr.allclose(v_eval, v_eval_pdf[0])
        assert dr.allclose(v_pdf, v_eval_pdf[1])
