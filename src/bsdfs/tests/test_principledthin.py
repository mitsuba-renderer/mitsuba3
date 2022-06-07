import pytest
import drjit as dr
import mitsuba as mi


def test00_construction_and_lobes(variant_scalar_rgb):
    # By default the BSDF should only have 2 lobes, no anisotropic / transmission
    b = mi.load_dict({
        'type': 'principledthin',
    })
    assert b.component_count() == 3
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
        'type': 'principledthin',
        'spec_trans': 0.5,
    })
    assert b.component_count() == 4
    assert mi.has_flag(b.flags(), mi.BSDFFlags.DiffuseReflection)
    assert mi.has_flag(b.flags(), mi.BSDFFlags.GlossyReflection)
    assert mi.has_flag(b.flags(), mi.BSDFFlags.GlossyTransmission)
    assert not mi.has_flag(b.flags(), mi.BSDFFlags.Anisotropic)


def test01_chi2_thin_front_side(variants_vec_backends_once_rgb):
    # front_side thin
    xml = """<float name="roughness" value="0.6"/>
             <float name="anisotropic" value="0.5"/>
             <float name="spec_trans" value="0.4"/>
             <float name="eta" value="1.3296"/>
             <float name="diff_trans" value="0.6"/>
          """
    wi = dr.normalize(mi.ScalarVector3f([1, 0, 1]))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("principledthin", xml, wi=wi)
    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )
    assert chi2.run()


def test02_chi2_thin_back_side(variants_vec_backends_once_rgb):
    # back side thin
    xml = """<float name="roughness" value="0.6"/>
             <float name="anisotropic" value="0.5"/>
             <float name="spec_trans" value="0.6"/>
             <float name="eta" value="1.3296"/>
             <float name="diff_trans" value="0.9"/>
        """
    wi = dr.normalize(mi.ScalarVector3f([1, 0, -1]))
    sample_func, pdf_func = mi.chi2.BSDFAdapter("principledthin", xml, wi=wi)
    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )
    assert chi2.run()


def test03_eval_pdf_thin(variant_scalar_rgb):
    # The true values are defined by the first implementation in order to
    # prevent unwanted changes.
    pdf_true = [
        0.18230389058589935,
        0.17071931064128876,
        0.1604636013507843,
        0.15113641321659088,
        0.1424213945865631,
        0.13407252728939056,
        0.1259022206068039,
        0.1177704930305481,
        0.10957615822553635,
        0.1012495756149292,
        0.09274674206972122,
        0.0840444564819336,
        0.07513649761676788,
        0.06603056192398071,
        0.05674567073583603,
        0.04731012135744095,
        0.03775978833436966,
        0.028136683627963066,
        0.01848774217069149,
        0.008863822557032108]
    evaluate_true = [
        0.04944333806633949,
        0.04872140288352966,
        0.047846242785453796,
        0.0468028225004673,
        0.04557877406477928,
        0.04416417330503464,
        0.042551249265670776,
        0.04073421657085419,
        0.03870923072099686,
        0.036474503576755524,
        0.03403010591864586,
        0.0313769206404686,
        0.028513599187135696,
        0.025431113317608833,
        0.02210502326488495,
        0.01848825439810753,
        0.014510626904666424,
        0.01009628176689148,
        0.005216196645051241,
        3.899415020768372e-18]

    bsdf = mi.load_string("""<bsdf version='2.0.0' type='principledthin'>
                      <float name="eta" value="1.5"/>
                      <float name="anisotropic" value="0.5"/>
                      <float name="sheen" value="0.5"/>
                      <float name="sheen_tint" value="0.2"/>
                      <float name="spec_trans" value="0.5"/>
                      <float name="flatness" value="0.5"/>
                      <float name="diff_trans" value="0.6"/>
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
