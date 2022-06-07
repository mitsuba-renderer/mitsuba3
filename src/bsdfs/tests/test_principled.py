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
        sample_dim=3
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
        sample_dim=3
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
        sample_dim=3
    )
    assert chi2.run()


def test05_eval_pdf(variant_scalar_rgb):
    # The true values are defined by the first implementation in order to
    # prevent unwanted changes.
    pdf_true = [
        0.17007458209991455,
        0.15398569405078888,
        0.14061447978019714,
        0.12924496829509735,
        0.1193309798836708,
        0.1104581207036972,
        0.10231398791074753,
        0.09466464817523956,
        0.08733665198087692,
        0.08020336925983429,
        0.07317451387643814,
        0.06618815660476685,
        0.0592045858502388,
        0.052201706916093826,
        0.045171335339546204,
        0.03811638057231903,
        0.031048627570271492,
        0.02398696169257164,
        0.016955917701125145,
        0.009984553791582584]
    evaluate_true = [
        0.06804995983839035,
        0.06425636261701584,
        0.060901228338479996,
        0.057847555726766586,
        0.05498623475432396,
        0.05223095044493675,
        0.049513764679431915,
        0.04678140580654144,
        0.04399249702692032,
        0.04111534729599953,
        0.03812596574425697,
        0.03500543162226677,
        0.031735487282276154,
        0.02829158864915371,
        0.024633212015032768,
        0.02069348469376564,
        0.01637275144457817,
        0.011544802226126194,
        0.006091561634093523,
        4.703659982886835e-18]

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
    si.wi = [1, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)

    ctx = mi.BSDFContext()
    pdf = []
    evaluate = []
    for i in range(20):
        theta = i / 19.0 * (dr.pi / 2)
        wo = [dr.sin(theta), 0, dr.cos(theta)]
        assert dr.allclose(bsdf.pdf(ctx, si, wo=wo), pdf_true[i])
        assert dr.allclose(bsdf.eval(ctx, si, wo=wo)[0], evaluate_true[i])
