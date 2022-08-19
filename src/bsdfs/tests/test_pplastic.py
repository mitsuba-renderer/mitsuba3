import pytest
import mitsuba as mi


@pytest.mark.slow
def test01_chi2_smooth(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.05"/>
             <rgb name="specular_reflectance" value="0.7"/>
             <rgb name="diffuse_reflectance" value="0.1"/>"""
    sample_func, pdf_func = mi.chi2.BSDFAdapter("pplastic", xml)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201,
        ires=16,
        seed=2
    )

    assert chi2.run()


@pytest.mark.slow
def test02_chi2_rough(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.25"/>
             <rgb name="specular_reflectance" value="0.7"/>
             <rgb name="diffuse_reflectance" value="0.1"/>"""
    sample_func, pdf_func = mi.chi2.BSDFAdapter("pplastic", xml)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    assert chi2.run()
