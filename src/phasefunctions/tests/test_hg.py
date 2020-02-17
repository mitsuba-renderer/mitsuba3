import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float


def test01_create(variant_scalar_rgb):
    from mitsuba.core.xml import load_string
    p = load_string("""<phase version='2.0.0' type='hg'>
        <float name="g" value="0.4"/>
    </phase>""")
    assert p is not None

def test02_chi2(variant_packet_rgb):
    from mitsuba.python.chi2 import PhaseFunctionAdapter, ChiSquareTest, SphericalDomain
    from mitsuba.core import ScalarBoundingBox2f

    sample_func, pdf_func = PhaseFunctionAdapter("hg", '<float name="g" value="0.6"/>')

    chi2 = ChiSquareTest(
        domain = SphericalDomain(),
        sample_func = sample_func,
        pdf_func = pdf_func,
        sample_dim = 2
    )

    result = chi2.run(0.1)
    chi2._dump_tables()
    assert result
