import pytest
import drjit as dr
import mitsuba as mi


def test00_construction_and_lobes(variant_scalar_rgb):
    # By default the BSDF should only have 2 lobes, no anisotropic / transmission
    b = mi.load_dict({
        'type': 'principled',
    })
    assert b.component_count() == 2
    assert mi.has_flag(b.flags(), mi.BSDFFlags.DiffuseReflection)
    assert mi.has_flag(b.flags(), mi.BSDFFlags.GlossyReflection)
    assert not mi.has_flag(b.flags(), mi.BSDFFlags.GlossyTransmission)
    assert not mi.has_flag(b.flags(), mi.BSDFFlags.Anisotropic)

    # Adding anisotropy via the traverse mechanism
    p = mi.traverse(b)
    p['anisotropic.value'] = 0.5
    p.update()
    assert mi.has_flag(b.flags(), mi.BSDFFlags.Anisotropic)

    b = mi.load_dict({
        'type': 'principled',
        'spec_trans': 0.5,
    })
    assert b.component_count() == 3
    assert mi.has_flag(b.flags(), mi.BSDFFlags.DiffuseReflection)
    assert mi.has_flag(b.flags(), mi.BSDFFlags.GlossyReflection)
    assert mi.has_flag(b.flags(), mi.BSDFFlags.GlossyTransmission)
    assert not mi.has_flag(b.flags(), mi.BSDFFlags.Anisotropic)

    b = mi.load_dict({
        'type': 'principled',
        'clearcoat': 0.5,
        'anisotropic': 0.5,
    })
    assert b.component_count() == 3
    assert mi.has_flag(b.flags(), mi.BSDFFlags.DiffuseReflection)
    assert mi.has_flag(b.flags(), mi.BSDFFlags.GlossyReflection)
    assert not mi.has_flag(b.flags(), mi.BSDFFlags.GlossyTransmission)
    assert mi.has_flag(b.flags(), mi.BSDFFlags.Anisotropic)


def test01_chi2_principled_normal(variants_vec_backends_once_rgb):
    # without spectrans
    xml = """<float name="roughness" value="0.6"/>
             <float name="metallic" value="0.4"/>
             <float name="anisotropic" value="0.4"/>
             <float name="clearcoat" value="0.8"/>
             <float name="eta" value="1.33"/>
          """
    wi = dr.normalize(mi.ScalarVector3f([1, 0, 1]))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("principled", xml, wi=wi)
    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )
    assert chi2.run()


def test02_chi2_spec_trans_outside(variants_vec_backends_once_rgb):
    # spec_trans outside (wi.z()>0)
    xml = """<float name="roughness" value="0.6"/>
             <float name="metallic" value="0.2"/>
             <float name="anisotropic" value="0.4"/>
             <float name="clearcoat" value="0.8"/>
             <float name="spec_trans" value="0.7"/>
             <float name="eta" value="1.3296"/>
          """
    wi = dr.normalize(mi.ScalarVector3f([1, 0, 1]))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("principled", xml, wi=wi)
    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        # The pdf has a sharp edge where the transmission lobe ends.
        # The default integration resolution underestimates its mass.
        ires=16
    )
    assert chi2.run()


def test03_chi2_spec_trans_inside(variants_vec_backends_once_rgb):
    # spec_trans inside (wi.z() < 0)
    xml = """<float name="roughness" value="0.5"/>
             <float name="anisotropic" value="0.4"/>
             <float name="clearcoat" value="0.8"/>
             <float name="spec_trans" value="0.7"/>
             <float name="eta" value="1.5"/>
          """
    wi = dr.normalize(mi.ScalarVector3f([1, 0, -1]))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("principled", xml, wi=wi)
    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        # The pdf has a sharp edge where the transmission lobe ends.
        # The default integration resolution underestimates its mass.
        ires=16
    )
    assert chi2.run()


def test04_chi2_spec_trans_less_dense(variants_vec_backends_once_rgb):
    # eta<1
    xml = """<float name="roughness" value="0.8"/>
             <float name="metallic" value="0.2"/>
             <float name="anisotropic" value="0.0"/>
             <float name="clearcoat" value="1.0"/>
             <float name="sheen" value="0.0"/>
             <float name="spec_trans" value="0.5"/>
             <float name="eta" value="0.5"/>
          """
    wi = dr.normalize(mi.ScalarVector3f([1, 0, 1]))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("principled", xml, wi=wi)
    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
        # The pdf has a sharp edge where the transmission lobe ends.
        # The default integration resolution underestimates its mass.
        ires=16
    )
    assert chi2.run()


def test05_eval_pdf(variant_scalar_rgb):
    # The true values are defined by the first implementation in order to
    # prevent unwanted changes.
    pdf_true = [
        0.17426574230194092,
        0.1573033183813095,
        0.14375387132167816,
        0.13257360458374023,
        0.12299317121505737,
        0.11445550620555878,
        0.10656186193227768,
        0.09903018176555634,
        0.09166355431079865,
        0.08432754129171371,
        0.07693372666835785,
        0.06942786276340485,
        0.06178133562207222,
        0.05398491770029068,
        0.04604418948292732,
        0.03797612711787224,
        0.02980651706457138,
        0.02156788296997547,
        0.013297977857291698,
        0.005038450006395578]
    evaluate_true = [
        0.0949176475405693,
        0.08489738404750824,
        0.07704004645347595,
        0.07072483748197556,
        0.06548994779586792,
        0.06099625676870346,
        0.05699571594595909,
        0.053306687623262405,
        0.049795448780059814,
        0.046362731605768204,
        0.042933426797389984,
        0.0394478514790535,
        0.035852666944265366,
        0.0320902056992054,
        0.028085866943001747,
        0.02373545989394188,
        0.01889786310493946,
        0.013403636403381824,
        0.0071020969189703465,
        5.483147842861893e-18]

    bsdf = mi.load_string("""<bsdf version='2.0.0' type='principled'>
                      <float name="metallic" value="0.3"/>
                      <float name="spec_tint" value="0.6"/>
                      <float name="specular" value="0.4"/>
                      <float name="anisotropic" value="0.3"/>
                      <float name="clearcoat" value="0.3"/>
                      <float name="clearcoat_gloss" value="0.5"/>
                      <float name="sheen" value="0.9"/>
                      <float name="sheen_tint" value="0.9"/>
                      <float name="spec_trans" value="0.5"/>
                      <float name="flatness" value="0.5"/>
                      </bsdf>
                      """)
    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.wi = dr.normalize(mi.Vector3f(1, 0, 1))
    si.sh_frame = mi.Frame3f(si.n)

    ctx = mi.BSDFContext()
    pdf = []
    evaluate = []
    for i in range(20):
        theta = i / 19.0 * (dr.pi / 2)
        wo = [dr.sin(theta), 0, dr.cos(theta)]
        assert dr.allclose(bsdf.pdf(ctx, si, wo=wo), pdf_true[i])
        assert dr.allclose(bsdf.eval(ctx, si, wo=wo)[0], evaluate_true[i])
