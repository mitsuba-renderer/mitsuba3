import pytest
import drjit as dr
import mitsuba as mi


@pytest.mark.slow
def test01_chi2_smooth(variants_vec_backends_once_rgb):
    xml = """<float name="alpha" value="0.05"/>
             <rgb name="specular_reflectance" value="0.7"/>
             <rgb name="diffuse_reflectance" value="0.1"/>"""
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughplastic", xml)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
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
             <rgb name="specular_reflectance" value="0.7"/>
             <rgb name="diffuse_reflectance" value="0.1"/>"""
    sample_func, pdf_func = mi.chi2.BSDFAdapter("roughplastic", xml)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    assert chi2.run()


def test03_eval_pdf(variant_scalar_rgb):
    bsdf = mi.load_dict({'type': 'roughplastic'})

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


def test04_eval_attribute(variants_all_rgb):
    reflectance = [0.5, 0.6, 0.7]
    roughness = 0.4
    bsdf = mi.load_dict({
        'type': 'roughplastic',
        'diffuse_reflectance': { 'type': 'rgb', 'value': reflectance},
        'alpha': roughness
    })

    si = mi.SurfaceInteraction3f()

    assert dr.allclose(bsdf.eval_attribute('diffuse_reflectance', si), reflectance)
    assert dr.allclose(bsdf.eval_attribute_3('diffuse_reflectance', si), reflectance)
    assert dr.allclose(bsdf.eval_attribute_1('alpha', si), roughness)
