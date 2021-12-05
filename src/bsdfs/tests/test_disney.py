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

    xml = """<float name="roughness" value="0.6"/>
            <float name="metallic" value="0.2"/>
            <float name="anisotropic" value="0.4"/>
            <float name="clearcoat" value="0.8"/>
            <float name="spec_trans" value="0.7"/>
             <float name="eta" value="1.3296"/>
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
        #back side thin
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




def test07_eval_pdf_3d(variant_scalar_rgb):
    from mitsuba.core import Frame3f,load_string
    from mitsuba.render import BSDFContext, BSDFFlags, SurfaceInteraction3f

    pdf_true = [0.19930407404899597, 0.18617326021194458, 0.1747969686985016, 0.1646047681570053, 0.15516546368598938, 0.1461564600467682, 0.13733986020088196, 0.1285434067249298, 0.11964600533246994, 0.11056657880544662, 0.10125553607940674, 0.09168823808431625, 0.08185981959104538, 0.07178140431642532, 0.06147685647010803, 0.050980344414711, 0.04033433645963669, 0.029587971046566963, 0.018795616924762726, 0.008015768602490425]
    evaluate_true = [0.06745785474777222, 0.06373678892850876, 0.06044566631317139, 0.057448845356702805, 0.0546383410692215, 0.05192873254418373, 0.04925282299518585, 0.0465579479932785, 0.04380323365330696, 0.04095740616321564, 0.037996821105480194, 0.034902848303318024, 0.031657446175813675, 0.028236225247383118, 0.02459871955215931, 0.02067791484296322, 0.016373589634895325, 0.011557860299944878, 0.006108088884502649, 4.728061397366843e-18]
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

    si    = SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.wi = [1, 0, 1]
    si.sh_frame = Frame3f(si.n)

    ctx = BSDFContext()
    pdf = []
    evaluate=[]
    for i in range(20):
        theta = i / 19.0 * (ek.Pi / 2)
        wo = [ek.sin(theta), 0, ek.cos(theta)]
        assert ek.allclose(bsdf.pdf(ctx, si, wo=wo), pdf_true[i])
        assert ek.allclose(bsdf.eval(ctx, si, wo=wo)[0],evaluate_true[i])

def test08_eval_pdf_thin(variant_scalar_rgb):
    from mitsuba.core import Frame3f,load_string
    from mitsuba.render import BSDFContext, BSDFFlags, SurfaceInteraction3f

    pdf_true =[0.16879956424236298, 0.15360711514949799, 0.14069214463233948, 0.1295156329870224, 0.11964717507362366, 0.11074677854776382, 0.10254909843206406, 0.09484904259443283, 0.0874902606010437, 0.08035553246736526, 0.07335905730724335, 0.06644028425216675, 0.05955890193581581, 0.05269104614853859, 0.04582605138421059, 0.03896398842334747, 0.032113611698150635, 0.025290759280323982, 0.018516965210437775, 0.01181843038648367]
    evaluate_true = [0.04944333806633949, 0.04872140288352966, 0.047846242785453796, 0.0468028225004673, 0.04557877406477928, 0.04416417330503464, 0.042551249265670776, 0.04073421657085419, 0.03870923072099686, 0.036474503576755524, 0.03403010591864586, 0.0313769206404686, 0.028513599187135696, 0.025431113317608833, 0.02210502326488495, 0.01848825439810753, 0.014510626904666424, 0.01009628176689148, 0.005216196645051241, 3.899415020768372e-18]
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

    si    = SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.wi = [1, 0, 1]
    si.sh_frame = Frame3f(si.n)

    ctx = BSDFContext()
    pdf = []
    evaluate=[]
    for i in range(20):
        theta = i / 19.0 * (ek.Pi / 2)
        wo = [ek.sin(theta), 0, ek.cos(theta)]
        assert ek.allclose(bsdf.pdf(ctx, si, wo=wo), pdf_true[i])
        assert ek.allclose(bsdf.eval(ctx, si, wo=wo)[0],evaluate_true[i])
