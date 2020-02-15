import mitsuba
import pytest
import enoki as ek

from mitsuba.python.chi2 import ChiSquareTest, BSDFAdapter, SphericalDomain


def test01_chi2_smooth(variant_packet_rgb):
    sample_func, pdf_func = BSDFAdapter("roughdielectric", """<float name="alpha" value="0.05"/>""")

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()


def test02_chi2_rough(variant_packet_rgb):
    sample_func, pdf_func = BSDFAdapter("roughdielectric", """<float name="alpha" value="0.25"/>""")

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    assert chi2.run()


def test03_chi2_rough_alt_wi(variant_packet_rgb):
    sample_func, pdf_func = BSDFAdapter("roughdielectric", """<float name="alpha" value="0.25"/>""", wi=[0.48666426,  0.32444284,  0.81110711])

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()
