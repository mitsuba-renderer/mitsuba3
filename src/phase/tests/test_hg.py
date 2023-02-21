import mitsuba as mi


def test01_create(variant_scalar_rgb):
    p = mi.load_dict({"type": "hg", "g": 0.4})
    assert p is not None

def test02_chi2(variants_vec_backends_once_rgb):
    sample_func, pdf_func = mi.chi2.PhaseFunctionAdapter("hg", '<float name="g" value="0.6"/>')

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    assert chi2.run()
