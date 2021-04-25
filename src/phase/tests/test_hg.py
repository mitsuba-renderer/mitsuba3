import mitsuba


def test01_create(variant_scalar_rgb):
    from mitsuba.core.xml import load_dict

    p = load_dict({"type": "hg", "g": 0.4})
    assert p is not None

def test02_chi2(variants_vec_backends_once_rgb):
    from mitsuba.python.chi2 import PhaseFunctionAdapter, ChiSquareTest, SphericalDomain

    sample_func, pdf_func = PhaseFunctionAdapter("hg", '<float name="g" value="0.6"/>')

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    result = chi2.run(0.1)
    chi2._dump_tables()
    assert result
