import pytest
import drjit as dr
import mitsuba as mi

def test_create(variant_scalar_rgb):
    p = mi.load_dict({"type": "rayleigh"})
    assert p is not None


def test_chi2(variants_vec_backends_once_rgb):
    sample_func, pdf_func = mi.chi2.PhaseFunctionAdapter("rayleigh", "")

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    assert chi2.run()
