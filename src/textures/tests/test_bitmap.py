import mitsuba
import pytest

from mitsuba.python.test.util import fresolver_append_path

@fresolver_append_path
@pytest.mark.parametrize('filter_type', ['nearest', 'bilinear'])
@pytest.mark.parametrize('wrap_mode', ['repeat', 'clamp', 'mirror'])
def test_sample_position(variant_packet_rgb, filter_type, wrap_mode):
    from mitsuba.core.xml import load_string
    from mitsuba.python.chi2 import ChiSquareTest, PlanarDomain
    from mitsuba.core import ScalarBoundingBox2f

    bitmap = load_string("""
    <texture type="bitmap" version="2.0.0">
        <string name="filename" value="resources/data/common/textures/carrot.png"/>
        <string name="filter_type" value="%s"/>
        <string name="wrap_mode" value="%s"/>
    </texture>""" % (filter_type, wrap_mode)).expand()[0]

    chi2 = ChiSquareTest(
        domain=PlanarDomain(ScalarBoundingBox2f([0, 0], [1, 1])),
        sample_func=lambda s: bitmap.sample_position(s)[0],
        pdf_func=lambda p: bitmap.pdf_position(p),
        sample_dim=2,
        res=201,
        ires=8
    )

    assert chi2.run()
