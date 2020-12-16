import pytest
import mitsuba

from mitsuba.python.chi2 import ChiSquareTest, BSDFAdapter, SphericalDomain

@pytest.mark.slow
def test01_chi2_smooth(variants_vec_backends_once):
    xml = """<float name="alpha" value="0.05"/>
             <spectrum name="specular_reflectance" value="0.7"/>
             <spectrum name="diffuse_reflectance" value="0.1"/>"""
    sample_func, pdf_func = BSDFAdapter("roughplastic", xml)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201
    )

    assert chi2.run()


@pytest.mark.slow
def test02_chi2_rough(variants_vec_backends_once):
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
