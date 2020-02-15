import mitsuba
import pytest
import enoki as ek

from mitsuba.python.chi2 import ChiSquareTest, BSDFAdapter, SphericalDomain

def test01_chi2_smooth(variant_packet_rgb):
    sample_func, pdf_func = BSDFAdapter("roughconductor", """<float name="alpha" value="0.05"/>""")

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()


def test02_chi2_rough(variant_packet_rgb):
    sample_func, pdf_func = BSDFAdapter("roughconductor", """<float name="alpha" value="0.5"/>""")

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    assert chi2.run()


def test03_chi2_rough_alt_wi(variant_packet_rgb):
    sample_func, pdf_func = BSDFAdapter("roughconductor", """<float name="alpha" value="0.25"/>""", wi=[0.970942, 0, 0.239316])

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()


def test04_chi2_anisotropic(variant_packet_rgb):
    sample_func, pdf_func = BSDFAdapter("roughconductor",
                                """<boolean name="sample_visible" value="false"/>
                                    <string name="distribution" value="beckmann"/>
                                    <float name="alpha_u" value="0.2"/>
                                    <float name="alpha_v" value="0.02"/>
                                """)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()


def test05_chi2_anisotropic_sample_visible(variant_packet_rgb):
    sample_func, pdf_func = BSDFAdapter("roughconductor",
                                """<boolean name="sample_visible" value="true"/>
                                    <string name="distribution" value="beckmann"/>
                                    <float name="alpha_u" value="0.2"/>
                                    <float name="alpha_v" value="0.02"/>
                                """)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()

