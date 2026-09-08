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
        0.1278684288263321,
        0.12396042793989182,
        0.12006281316280365,
        0.11602213233709335,
        0.11171980947256088,
        0.10706986486911774,
        0.10201431810855865,
        0.09651850908994675,
        0.09056667238473892,
        0.08415845781564713,
        0.07730592042207718,
        0.0700313001871109,
        0.062365103513002396,
        0.05434466153383255,
        0.04601289704442024,
        0.037417300045490265,
        0.028609082102775574,
        0.01964237540960312,
        0.010573581792414188,
        0.0014607204357162118]
    evaluate_true = [
        0.05598372220993042,
        0.05437241494655609,
        0.052839115262031555,
        0.05131036788225174,
        0.04972849041223526,
        0.0480492077767849,
        0.04623885080218315,
        0.04427193105220795,
        0.04212912917137146,
        0.03979571536183357,
        0.03725983202457428,
        0.03451000526547432,
        0.031530797481536865,
        0.028296049684286118,
        0.02476016990840435,
        0.02085060626268387,
        0.01646892912685871,
        0.011514821089804173,
        0.0059579359367489815,
        4.4263191447973e-18]

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
