# Only superficial testing here, main coverage achieved
# via 'src/libcore/tests/test_distr.py'

import mitsuba
import pytest
import enoki as ek


@pytest.fixture()
def obj():
    return mitsuba.core.xml.load_string('''
        <spectrum version='2.0.0' type='irregular'>
            <string name="wavelengths" value="500, 600, 650"/>
            <string name="values" value="1, 2, .5"/>
        </spectrum>''')


def test01_eval(variant_scalar_spectral, obj):
    from mitsuba.render import SurfaceInteraction3f

    si = SurfaceInteraction3f()

    values = [0, 1, 1.5, 2, .5, 0]
    for i in range(6):
        si.wavelengths = 450 + 50 * i
        assert ek.allclose(obj.eval(si), values[i])
        assert ek.allclose(obj.pdf_spectrum(si), values[i] / 212.5)

    with pytest.raises(RuntimeError) as excinfo:
        obj.eval_1(si)
    assert 'not implemented' in str(excinfo.value)

    with pytest.raises(RuntimeError) as excinfo:
        obj.eval_3(si)
    assert 'not implemented' in str(excinfo.value)


def test02_sample_spectrum(variant_scalar_spectral, obj):
    from mitsuba.render import SurfaceInteraction3f

    si = SurfaceInteraction3f()
    assert ek.allclose(obj.sample_spectrum(si, 0), [500, 212.5])
    assert ek.allclose(obj.sample_spectrum(si, 1), [650, 212.5])
    assert ek.allclose(
        obj.sample_spectrum(si, .5),
        [576.777, 212.5]
    )
