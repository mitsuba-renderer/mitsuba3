import pytest
import drjit as dr
import mitsuba as mi


def test01_chi2(variants_vec_spectral):
    sample_func, pdf_func = mi.chi2.SpectrumAdapter(
        '<spectrum version="3.0.0" type="d65"/>')

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.LineDomain(bounds=[360.0, 830.0]),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=1
    )

    assert chi2.run()
