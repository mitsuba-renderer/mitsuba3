import pytest
import enoki as ek
from mitsuba.python.chi2 import ChiSquareTest, BSDFAdapter, SphericalDomain

# TODO this is way too slow
# @pytest.mark.slow
# def test01_chi2_smooth(variant_llvm_rgb):
#     from mitsuba.core import ScalarVector3f
#     xml = """<float name="alpha" value="0.05"/>"""
#     wi = ek.normalize(ScalarVector3f(0.8, 0.3, 0.05))
#     sample_func, pdf_func = BSDFAdapter("roughdielectric", xml, wi=wi)

#     chi2 = ChiSquareTest(
#         domain=SphericalDomain(),
#         sample_func=sample_func,
#         pdf_func=pdf_func,
#         sample_dim=3,
#         res=201,
#         ires=32
#     )

#     assert chi2.run()


def test02_chi2_rough_grazing(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    xml = """<float name="alpha" value="0.5"/>"""
    wi = ek.normalize(ScalarVector3f(0.8, 0.3, 0.05))
    sample_func, pdf_func = BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()


def test03_chi2_rough_beckmann_all(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    xml = """<float name="alpha" value="0.5"/>
             <boolean name="sample_visible" value="false"/>
             <string name="distribution" value="beckmann"/>
          """
    wi = ek.normalize(ScalarVector3f(0.5, 0.0, 0.5))
    sample_func, pdf_func = BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201
    )

    assert chi2.run()


def test04_chi2_rough_beckmann_visible(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    xml = """<float name="alpha" value="0.5"/>
             <boolean name="sample_visible" value="true"/>
             <string name="distribution" value="beckmann"/>
          """
    wi = ek.normalize(ScalarVector3f(0.5, 0.0, 0.5))
    sample_func, pdf_func = BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()


def test05_chi2_rough_ggx_all(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    xml = """<float name="alpha" value="0.5"/>
             <boolean name="sample_visible" value="false"/>
             <string name="distribution" value="ggx"/>
          """
    wi = ek.normalize(ScalarVector3f(0.5, 0.0, 0.5))
    sample_func, pdf_func = BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201
    )

    assert chi2.run()


def test06_chi2_rough_ggx_visible(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    xml = """<float name="alpha" value="0.5"/>
             <boolean name="sample_visible" value="true"/>
             <string name="distribution" value="ggx"/>
          """
    wi = ek.normalize(ScalarVector3f(0.5, 0.5, 0.001))
    sample_func, pdf_func = BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()


def test07_chi2_rough_from_inside(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    xml = """<float name="alpha" value="0.5"/>"""
    wi = ek.normalize(ScalarVector3f(0.2, -0.6, -0.5))
    sample_func, pdf_func = BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201
    )

    assert chi2.run()


def test08_chi2_rough_from_inside_tir(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    xml = """<float name="alpha" value="0.5"/>"""
    wi = ek.normalize(ScalarVector3f(0.8, 0.3, -0.05))
    sample_func, pdf_func = BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()


def test09_chi2_rough_from_denser(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    xml = """<float name="alpha" value="0.5"/>
             <float name="ext_ior" value="1.5"/>
             <float name="int_ior" value="1"/>"""
    wi = ek.normalize(ScalarVector3f(0.2, -0.6, 0.5))
    sample_func, pdf_func = BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()


def test10_chi2_lobe_refl(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    from mitsuba.render import BSDFContext

    xml = """
    <float name="alpha_u" value="0.5"/>
    <float name="alpha_v" value="0.2"/>
    """
    wi = ek.normalize(ScalarVector3f(-0.5, -0.5, 0.1))
    ctx = BSDFContext()
    ctx.component = 0
    sample_func, pdf_func = BSDFAdapter("roughdielectric", xml, wi=wi, ctx=ctx)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201
    )

    assert chi2.run()


def test11_chi2_aniso_lobe_trans(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    from mitsuba.render import BSDFContext

    xml = """
    <float name="alpha_u" value="0.5"/>
    <float name="alpha_v" value="0.2"/>
    """
    wi = ek.normalize(ScalarVector3f(-0.5, -0.5, 0.1))
    ctx = BSDFContext()
    ctx.component = 1
    sample_func, pdf_func = BSDFAdapter("roughdielectric", xml, wi=wi, ctx=ctx)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=201
    )

    assert chi2.run()
