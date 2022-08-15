# Only superficial testing here, main coverage achieved
# via 'src/libcore/tests/test_distr.py'

import pytest
import drjit as dr
import mitsuba as mi


@pytest.fixture()
def obj():
    return mi.load_dict({
        "type" : "regular",
        "wavelength_min" : 500,
        "wavelength_max" : 600,
        "values" : "1, 2"
    })


def test01_eval(variant_scalar_spectral, obj):
    si = mi.SurfaceInteraction3f()

    values = [0, 1, 1.5, 2, 0]
    for i in range(5):
        si.wavelengths = 450 + 50 * i
        assert dr.allclose(obj.eval(si), values[i])
        assert dr.allclose(obj.pdf_spectrum(si), values[i] / 150.0)

    with pytest.raises(RuntimeError) as excinfo:
        obj.eval_1(si)
    assert 'not implemented' in str(excinfo.value)

    with pytest.raises(RuntimeError) as excinfo:
        obj.eval_3(si)
    assert 'not implemented' in str(excinfo.value)


def test02_sample_spectrum(variant_scalar_spectral, obj):
    si = mi.SurfaceInteraction3f()
    assert dr.allclose(obj.sample_spectrum(si, 0), [500, 150])
    assert dr.allclose(obj.sample_spectrum(si, 1), [600, 150])
    assert dr.allclose(
        obj.sample_spectrum(si, .5),
        [500 + 100 * (dr.sqrt(10) / 2 - 1), 150]
    )
