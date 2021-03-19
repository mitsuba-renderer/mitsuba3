import mitsuba
import pytest
import enoki as ek
from enoki.scalar import ArrayXf as Float



def test01_create(variant_scalar_rgb):
    from mitsuba.core.xml import load_string
    from mitsuba.render import PhaseFunctionFlags

    p = load_string("""<phase version='2.0.0' type='sggx'>
                            <boolean name="diffuse" value="true"/>
                            <volume type="constvolume" name="S">
                                <string name="value" value="1.0, 1.0, 1.0, 0.0, 0.0, 0.0"/>
                            </volume>
                        </phase>""")

    assert p is not None
    assert p.flags() == PhaseFunctionFlags.Anisotropic | PhaseFunctionFlags.Microflake

@pytest.mark.slow
def test02_chi2_simple(variants_vec_backends_once_rgb):
    from mitsuba.python.chi2 import PhaseFunctionAdapter, ChiSquareTest, SphericalDomain
    from mitsuba.core import ScalarBoundingBox2f

    sample_func, pdf_func = PhaseFunctionAdapter("sggx",
         """<boolean name="diffuse" value="false"/>
            <volume type="constvolume" name="S">
                <string name="value" value="0.15, 1.0, 0.8, 0.0, 0.0, 0.0"/>
            </volume>
         """)

    chi2 = ChiSquareTest(
        domain = SphericalDomain(),
        sample_func = sample_func,
        pdf_func = pdf_func,
        sample_dim = 3
    )

    result = chi2.run(0.1)
    chi2._dump_tables()
    assert result

@pytest.mark.slow
def test03_chi2_skewed(variants_vec_backends_once_rgb):
    from mitsuba.python.chi2 import PhaseFunctionAdapter, ChiSquareTest, SphericalDomain
    from mitsuba.core import ScalarBoundingBox2f

    sample_func, pdf_func = PhaseFunctionAdapter("sggx",
         """<boolean name="diffuse" value="false"/>
            <volume type="constvolume" name="S">
                <string name="value" value="1.0, 0.35, 0.32, 0.52, 0.44, 0.2"/>
            </volume>
         """)

    chi2 = ChiSquareTest(
        domain = SphericalDomain(),
        sample_func = sample_func,
        pdf_func = pdf_func,
        sample_dim = 3
    )

    result = chi2.run(0.1)
    chi2._dump_tables()
    assert result

