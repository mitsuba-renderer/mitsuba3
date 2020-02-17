import mitsuba
import pytest
import enoki as ek


def test01_chi2(variant_packet_spectral):
    from mitsuba.python.chi2 import SpectrumAdapter, ChiSquareTest, LineDomain

    sample_func, pdf_func = SpectrumAdapter(
        '<spectrum version="2.0.0" type="d65"/>')

    chi2 = ChiSquareTest(
        domain=LineDomain(bounds=[360.0, 830.0]),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=1
    )

    assert chi2.run()
