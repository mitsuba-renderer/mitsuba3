import pytest
import drjit as dr
import mitsuba as mi


def test01_create(variant_scalar_rgb):
    p = mi.load_dict({"type": "isotropic"})
    assert p is not None


def test02_eval(variants_vec_backends_once_rgb):
    p = mi.load_dict({"type": "isotropic"})
    ctx = mi.PhaseFunctionContext()
    mei = mi.MediumInteraction3f()

    theta  = dr.linspace(mi.Float, 0, dr.pi / 2, 4)
    ph  = dr.linspace(mi.Float, 0, dr.pi, 4)
    wo = [dr.sin(theta), 0, dr.cos(theta)]
    v_eval = p.eval_pdf(ctx, mei, wo)[0]

    assert dr.allclose(v_eval, 1.0 / (4 * dr.pi))


def test03_chi2(variants_vec_backends_once_rgb):
    sample_func, pdf_func = mi.chi2.PhaseFunctionAdapter("isotropic", "")

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    assert chi2.run()
