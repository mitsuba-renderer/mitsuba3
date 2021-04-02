def test_create(variant_scalar_rgb):
    from mitsuba.core.xml import load_dict

    p = load_dict({"type": "rayleigh"})
    assert p is not None


def test_chi2(variants_vec_backends_once_rgb):
    from mitsuba.python.chi2 import PhaseFunctionAdapter, ChiSquareTest, SphericalDomain

    sample_func, pdf_func = PhaseFunctionAdapter("rayleigh", "")

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    result = chi2.run(0.1)
    chi2._dump_tables()
    assert result
