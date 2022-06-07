import drjit as dr
import mitsuba as mi

def test01_chi2_smooth(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.05"/>"""
    wi = dr.normalize(mi.ScalarVector3f(1.0, 1.0, 1.0))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        res=140,
        ires=32
    )

    assert chi2.run()


def test02_chi2_aniso_beckmann_all(variants_vec_backends_once_rgb):
    xml = """<float name="alpha_u" value="0.2"/>
             <float name="alpha_v" value="0.05"/>
             <string name="distribution" value="beckmann"/>
             <boolean name="sample_visible" value="false"/>"""
    wi = dr.normalize(mi.ScalarVector3f(1.0, 1.0, 1.0))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        ires=16
    )

    assert chi2.run()


def test03_chi2_aniso_beckmann_visible(variants_vec_backends_once_rgb):
    xml = """<float name="alpha_u" value="0.2"/>
             <float name="alpha_v" value="0.05"/>
             <string name="distribution" value="beckmann"/>
             <boolean name="sample_visible" value="true"/>"""
    wi = dr.normalize(mi.ScalarVector3f(1.0, 1.0, 1.0))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        ires=16,
        seed=2
    )

    assert chi2.run()


def test04_chi2_aniso_ggx_all(variants_vec_backends_once_rgb):
    xml = """<float name="alpha_u" value="0.2"/>
             <float name="alpha_v" value="0.05"/>
             <string name="distribution" value="ggx"/>
             <boolean name="sample_visible" value="false"/>"""
    wi = dr.normalize(mi.ScalarVector3f(1.0, 1.0, 1.0))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        ires=8
    )

    assert chi2.run()


def test05_chi2_aniso_ggx_visible(variants_vec_backends_once_rgb):
    xml = """<float name="alpha_u" value="0.2"/>
             <float name="alpha_v" value="0.05"/>
             <string name="distribution" value="ggx"/>
             <boolean name="sample_visible" value="true"/>"""
    wi = dr.normalize(mi.ScalarVector3f(1.0, 1.0, 1.0))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughconductor", xml, wi=wi)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        ires=8
    )

    assert chi2.run()


def test06_eval_pdf(variant_scalar_rgb):
    bsdf = mi.load_dict({'type': 'roughconductor'})

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
