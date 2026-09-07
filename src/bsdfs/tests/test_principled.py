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
        0.06745785474777222,
        0.06373678892850876,
        0.06044566631317139,
        0.0574488490819931,
        0.0546383410692215,
        0.05192873254418373,
        0.04925282299518585,
        0.046557944267988205,
        0.04380323365330696,
        0.04095740243792534,
        0.037996821105480194,
        0.034902848303318024,
        0.031657446175813675,
        0.028236225247383118,
        0.02459871955215931,
        0.02067791298031807,
        0.016373587772250175,
        0.011557860299944878,
        0.006108088884502649,
        4.728061397366843e-18]

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
