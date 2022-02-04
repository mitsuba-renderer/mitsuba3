import pytest
import mitsuba
import drjit as dr

from mitsuba.python.chi2 import ChiSquareTest, BSDFAdapter, SphericalDomain

@pytest.mark.slow
def test01_chi2_smooth(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.05"/>
             <spectrum name="specular_reflectance" value="0.7"/>
             <spectrum name="diffuse_reflectance" value="0.1"/>"""
    sample_func, pdf_func = BSDFAdapter("roughplastic", xml)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        ires=16,
        res=201
    )

    assert chi2.run()


@pytest.mark.slow
def test02_chi2_rough(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.25"/>
             <spectrum name="specular_reflectance" value="0.7"/>
             <spectrum name="diffuse_reflectance" value="0.1"/>"""
    sample_func, pdf_func = BSDFAdapter("roughplastic", xml)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    assert chi2.run()


def test03_eval_pdf(variant_scalar_rgb):
    from mitsuba.core import load_string, Frame3f
    from mitsuba.render import BSDFContext, SurfaceInteraction3f

    bsdf = load_string("<bsdf version='2.0.0' type='roughplastic'></bsdf>")

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
