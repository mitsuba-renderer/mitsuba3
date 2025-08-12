# Only superficial testing here, main coverage achieved
# via 'src/libcore/tests/test_distr.py'

import pytest
import drjit as dr
import mitsuba as mi


@pytest.fixture()
def obj():
    return mi.load_dict({
        "type" : "irregular",
        "wavelengths" : "500, 600, 650",
        "values" : "1, 2, .5"
    })
    
@pytest.fixture()
def obj_acoustic():
    return mi.load_dict({
        "type" : "irregular",
        "frequencies" : "500, 600, 650",
        "values" : "1, 2, .5"
    })


def test01_eval(variant_scalar_spectral, obj):
    si = mi.SurfaceInteraction3f()

    values = [0, 1, 1.5, 2, .5, 0]
    for i in range(6):
        si.wavelengths = 450 + 50 * i
        assert dr.allclose(obj.eval(si), values[i])
        assert dr.allclose(obj.pdf_spectrum(si), values[i] / 212.5)

    with pytest.raises(RuntimeError) as excinfo:
        obj.eval_1(si)
    assert 'not implemented' in str(excinfo.value)

    with pytest.raises(RuntimeError) as excinfo:
        obj.eval_3(si)
    assert 'not implemented' in str(excinfo.value)


def test02_sample_spectrum(variant_scalar_spectral, obj):
    si = mi.SurfaceInteraction3f()
    assert dr.allclose(obj.sample_spectrum(si, 0), [500, 212.5])
    assert dr.allclose(obj.sample_spectrum(si, 1), [650, 212.5])
    assert dr.allclose(
        obj.sample_spectrum(si, .5),
        [576.777, 212.5]
    )

def test_03_acoustic_initialization(variant_scalar_acoustic):
    with pytest.raises(RuntimeError) as excinfo:
        mi.load_dict({
            "type" : "irregular",
            "frequencies" : "500, 600, 650",
            "wavelengths" : "500, 600, 650",
            "values" : "1, 2, .5"
        })
    assert 'Only one of' in str(excinfo.value)

def test03_acoustic_compatibility(variant_scalar_acoustic, obj, obj_acoustic):
    "make sure that spectra are equivalent when using frequencies vs wavelengths"

    si = mi.SurfaceInteraction3f()

    for i in range(6):
        si.wavelengths = 450 + 50 * i
        assert dr.allclose(obj_acoustic.eval(si), obj.eval(si))
        assert dr.allclose(obj_acoustic.pdf_spectrum(si), obj.pdf_spectrum(si))

    assert dr.allclose(obj_acoustic.sample_spectrum(si, 0), obj.sample_spectrum(si, 0))
    assert dr.allclose(obj_acoustic.sample_spectrum(si, 1), obj.sample_spectrum(si, 1))
    assert dr.allclose(obj_acoustic.sample_spectrum(si, .5), obj.sample_spectrum(si, .5))
