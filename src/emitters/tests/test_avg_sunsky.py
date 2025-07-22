import mitsuba as mi

def test(variants_vec_backends_once):
    sky = {
        "type": "avg_sunsky",
        "time_resolution": 200
    }

    sample_func, pdf_func = mi.chi2.EmitterAdapter("avg_sunsky", sky)
    test = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        pdf_func= pdf_func,
        sample_func= sample_func,
        sample_dim=2,
        sample_count=200_000,
        res=55,
        ires=32
    )

    assert test.run(), "Chi2 test failed"