import mitsuba
import pytest
import enoki as ek

def test01_chi2(variants_vec_backends_once_rgb):
    from mitsuba.python.chi2 import ChiSquareTest, EmitterAdapter, SphericalDomain, PlanarDomain
    from mitsuba.core import ScalarBoundingBox2f
    xml = """<string name = "filename" value ="/home/dwang/Desktop/scenes/test.png" />"""
  
    sample_func, pdf_func = EmitterAdapter("envmap", xml)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=2
    )

    assert chi2.run()