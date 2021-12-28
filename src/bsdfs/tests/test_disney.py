import pytest
import enoki as ek
from mitsuba.python.chi2 import ChiSquareTest, BSDFAdapter, SphericalDomain


def test01_chi2_disney_normal(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    # without spectrans
    xml = """<float name="roughness" value="0.6"/>
             <float name="metallic" value="0.4"/>
             <float name="anisotropic" value="0.4"/>
             <float name="clearcoat" value="0.8"/>
             <float name="eta" value="1.33"/>
          """
    wi = ek.normalize(ScalarVector3f([1, 0, 1]))
    sample_func, pdf_func = BSDFAdapter("disney", xml, wi=wi)
    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )
    assert chi2.run()


def test02_chi2_spec_trans_outside(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    # spec_trans outside (wi.z()>0)

    xml = """<float name="roughness" value="0.6"/>
             <float name="metallic" value="0.2"/>
             <float name="anisotropic" value="0.4"/>
             <float name="clearcoat" value="0.8"/>
             <float name="spec_trans" value="0.7"/>
             <float name="eta" value="1.3296"/>
          """
    wi = ek.normalize(ScalarVector3f([1, 0, 1]))
    sample_func, pdf_func = BSDFAdapter("disney", xml, wi=wi)
    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )
    assert chi2.run()


def test03_chi2_spec_trans_inside(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    # spec_trans inside (wi.z() < 0)
    xml = """<float name="roughness" value="0.5"/>
             <float name="anisotropic" value="0.4"/>
             <float name="clearcoat" value="0.8"/>
             <float name="spec_trans" value="0.7"/>
             <float name="eta" value="1.5"/>
          """
    wi = ek.normalize(ScalarVector3f([1, 0, -1]))
    sample_func, pdf_func = BSDFAdapter("disney", xml, wi=wi)
    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )
    assert chi2.run()


def test04_chi2_spec_trans_less_dense(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    # eta<1
    xml = """<float name="roughness" value="0.8"/>
             <float name="metallic" value="0.2"/>
             <float name="anisotropic" value="0.0"/>
             <float name="clearcoat" value="1.0"/>
             <float name="sheen" value="0.0"/>
             <float name="spec_trans" value="0.5"/>
             <float name="eta" value="0.5"/>
          """
    wi = ek.normalize(ScalarVector3f([1, 0, 1]))
    sample_func, pdf_func = BSDFAdapter("disney", xml, wi=wi)
    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )
    assert chi2.run()


def test05_chi2_thin_front_side(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    # front_side thin
    xml = """<float name="roughness" value="0.6"/>
             <float name="anisotropic" value="0.5"/>
             <float name="spec_trans" value="0.4"/>
             <float name="eta" value="1.3296"/>
             <float name="diff_trans" value="0.6"/>
             <boolean name="thin" value="true"/>

          """
    wi = ek.normalize(ScalarVector3f([1, 0, 1]))
    sample_func, pdf_func = BSDFAdapter("disney", xml, wi=wi)
    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )
    assert chi2.run()


def test06_chi2_thin_back_side(variants_vec_backends_once_rgb):
    from mitsuba.core import ScalarVector3f
    # back side thin
    xml = """<float name="roughness" value="0.6"/>
             <float name="anisotropic" value="0.5"/>
             <float name="spec_trans" value="0.6"/>
             <float name="eta" value="1.3296"/>
             <float name="diff_trans" value="0.9"/>
             <boolean name="thin" value="true"/>
        """
    wi = ek.normalize(ScalarVector3f([1, 0, -1]))
    sample_func, pdf_func = BSDFAdapter("disney", xml, wi=wi)
    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )
    assert chi2.run()


def test07_eval_pdf_regular(variant_scalar_rgb):
    from mitsuba.core import Frame3f, load_string
    from mitsuba.render import BSDFContext, BSDFFlags, SurfaceInteraction3f

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

    bsdf = load_string("""<bsdf version='2.0.0' type='disney'>
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
    si = SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.wi = [1, 0, 1]
    si.sh_frame = Frame3f(si.n)

    ctx = BSDFContext()
    pdf = []
    evaluate = []
    for i in range(20):
        theta = i / 19.0 * (ek.Pi / 2)
        wo = [ek.sin(theta), 0, ek.cos(theta)]
        assert ek.allclose(bsdf.pdf(ctx, si, wo=wo), pdf_true[i])
        assert ek.allclose(bsdf.eval(ctx, si, wo=wo)[0], evaluate_true[i])


def test08_eval_pdf_thin(variant_scalar_rgb):
    from mitsuba.core import Frame3f, load_string
    from mitsuba.render import BSDFContext, BSDFFlags, SurfaceInteraction3f

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

    bsdf = load_string("""<bsdf version='2.0.0' type='disney'>
                      <float name="eta" value="1.5"/>
                      <float name="anisotropic" value="0.5"/>
                      <float name="sheen" value="0.5"/>
                      <float name="sheen_tint" value="0.2"/>
                      <float name="spec_trans" value="0.5"/>
                      <float name="flatness" value="0.5"/>
                      <float name="diff_trans" value="0.6"/>
                      <boolean name="thin" value="true"/>
                      </bsdf>
                      """)

    si = SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.wi = [1, 0, 1]
    si.sh_frame = Frame3f(si.n)

    ctx = BSDFContext()
    pdf = []
    evaluate = []
    for i in range(20):
        theta = i / 19.0 * (ek.Pi / 2)
        wo = [ek.sin(theta), 0, ek.cos(theta)]
        assert ek.allclose(bsdf.pdf(ctx, si, wo=wo), pdf_true[i])
        assert ek.allclose(bsdf.eval(ctx, si, wo=wo)[0], evaluate_true[i])
