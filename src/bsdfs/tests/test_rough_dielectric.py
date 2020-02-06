import mitsuba
import pytest
import enoki as ek

from mitsuba.python.test import variant_packet
from mitsuba.python.chi2 import ChiSquareTest, BSDFAdapter, SphericalDomain


def test01_chi2_smooth(variant_packet):
    sample_func, pdf_func = BSDFAdapter("roughdielectric", """<float name="alpha" value="0.05"/>""")

    from mitsuba.core import ScalarBoundingBox2f
    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    result = chi2.run(0.01)
    print(chi2.messages)
    chi2._dump_tables()
    assert result


def test02_chi2_rough(variant_packet):
    sample_func, pdf_func = BSDFAdapter("roughdielectric", """<float name="alpha" value="0.25"/>""")

    from mitsuba.core import ScalarBoundingBox2f
    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    result = chi2.run(0.01)
    print(chi2.messages)
    chi2._dump_tables()
    assert result


def test03_chi2_rough_alt_wi(variant_packet):
    sample_func, pdf_func = BSDFAdapter("roughdielectric", """<float name="alpha" value="0.25"/>""", wi=[0.48666426,  0.32444284,  0.81110711])

    from mitsuba.core import ScalarBoundingBox2f
    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    result = chi2.run(0.01)
    print(chi2.messages)
    chi2._dump_tables()
    assert result