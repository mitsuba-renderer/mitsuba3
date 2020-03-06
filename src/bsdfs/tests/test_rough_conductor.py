import enoki as ek

from mitsuba.python.chi2 import ChiSquareTest, BSDFAdapter, SphericalDomain


def test01_chi2_smooth(variant_packet_rgb):
    xml = """<float name="alpha" value="0.05"/>"""
    wi = ek.normalize([1.0, 1.0, 1.0])
    sample_func, pdf_func = BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201,
        ires=8
    )

    assert chi2.run()


def test02_chi2_aniso_beckmann_all(variant_packet_rgb):
    xml = """<float name="alpha_u" value="0.2"/>
             <float name="alpha_v" value="0.05"/>
             <string name="distribution" value="beckmann"/>
             <boolean name="sample_visible" value="false"/>"""
    wi = ek.normalize([1.0, 1.0, 1.0])
    sample_func, pdf_func = BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201,
        ires=8
    )

    assert chi2.run()


def test03_chi2_aniso_beckmann_visible(variant_packet_rgb):
    xml = """<float name="alpha_u" value="0.2"/>
             <float name="alpha_v" value="0.05"/>
             <string name="distribution" value="beckmann"/>
             <boolean name="sample_visible" value="true"/>"""
    wi = ek.normalize([1.0, 1.0, 1.0])
    sample_func, pdf_func = BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        ires=8
    )

    assert chi2.run()


def test04_chi2_aniso_ggx_all(variant_packet_rgb):
    xml = """<float name="alpha_u" value="0.2"/>
             <float name="alpha_v" value="0.05"/>
             <string name="distribution" value="ggx"/>
             <boolean name="sample_visible" value="false"/>"""
    wi = ek.normalize([1.0, 1.0, 1.0])
    sample_func, pdf_func = BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        ires=8
    )

    assert chi2.run()


def test05_chi2_aniso_ggx_visible(variant_packet_rgb):
    xml = """<float name="alpha_u" value="0.2"/>
             <float name="alpha_v" value="0.05"/>
             <string name="distribution" value="ggx"/>
             <boolean name="sample_visible" value="true"/>"""
    wi = ek.normalize([1.0, 1.0, 1.0])
    sample_func, pdf_func = BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        ires=8
    )

    assert chi2.run()
