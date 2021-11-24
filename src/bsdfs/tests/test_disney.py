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

    pdf_true = [[0.19930407404899597], [0.18617326021194458], [0.1747969686985016], [0.1646047681570053], [0.15516547858715057], [0.1461564600467682], [0.13733986020088196], [0.1285434067249298], [0.11964600533246994], [0.11056657880544662], [0.10125553607940674], [0.09168823808431625], [0.08185981959104538], [0.07178140431642532], [0.06147685647010803], [0.0509803406894207], [0.04033433645963669], [0.029587971046566963], [0.018795615062117577], [0.008015769533813]]
    evaluate_true = [[0.06745785474777222], [0.06373678892850876], [0.06044566631317139], [0.057448845356702805], [0.05463835224509239], [0.05192883685231209], [0.0492534302175045], [0.04656057804822922], [0.043812345713377], [0.04098380729556084], [0.03806281462311745], [0.03504826873540878], [0.03194396570324898], [0.028745237737894058], [0.02541625313460827], [0.0218595489859581], [0.017884166911244392], [0.013185622170567513], [0.007360770367085934], [6.092421371517933e-18]]
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
        assert ek.allclose(bsdf.pdf(ctx, si, wo=wo), pdf_true[i][0])
        assert ek.allclose(bsdf.eval(ctx, si, wo=wo)[0],evaluate_true[i][0])

def test08_eval_pdf_thin(variant_scalar_rgb):
    from mitsuba.core import Frame3f,load_string
    from mitsuba.render import BSDFContext, BSDFFlags, SurfaceInteraction3f

    pdf_true=[[0.17940989136695862], [0.1641812026500702], [0.15115775167942047], [0.1398012936115265], [0.12968260049819946], [0.12046343088150024], [0.1118806004524231], [0.10373164713382721], [0.09586330503225327], [0.08816180378198624], [0.08054524660110474], [0.07295729219913483], [0.06536220014095306], [0.05774100124835968], [0.050088174641132355], [0.042409155517816544], [0.03471829369664192], [0.027037162333726883], [0.019393159076571465], [0.011818429455161095]]
    evaluate_true = [[0.04944431781768799], [0.04872172325849533], [0.047846339643001556], [0.04680284112691879], [0.04557879641652107], [0.04416435956954956], [0.04255237802863121], [0.04073910415172577], [0.03872615844011307], [0.03652353212237358], [0.03415266424417496], [0.031646985560655594], [0.0290457084774971], [0.026376424357295036], [0.023623298853635788], [0.020682720467448235], [0.017315993085503578], [0.013119283132255077], [0.007542632520198822], [6.433286785519397e-18]]

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
        assert ek.allclose(bsdf.pdf(ctx, si, wo=wo), pdf_true[i][0])
        assert ek.allclose(bsdf.eval(ctx, si, wo=wo)[0],evaluate_true[i][0])
