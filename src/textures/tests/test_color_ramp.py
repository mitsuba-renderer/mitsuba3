import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path


@fresolver_append_path
@pytest.mark.parametrize('filter_type', ['nearest', 'bilinear'])
@pytest.mark.parametrize('wrap_mode', ['repeat', 'clamp', 'mirror'])
def test01_sample_position(variants_vec_backends_once, filter_type, wrap_mode):
    color_ramp = mi.load_dict({
        "type" : "color_ramp",
        'input_fac': {
            'type': 'bitmap',
            'filename': 'resources/data/common/textures/carrot.png',
            "filter_type": filter_type,
            "wrap_mode": wrap_mode,
        },
        "mode" : "linear",
        "num_band" : 2.0,
        "pos0" : 0.0,
        "pos1" : 1.0,
        "color0" : {
            'type': 'rgb',
            'value': [0.0, 0.0, 0.0]
        },
        "color1" : {
            'type': 'rgb',
            'value': [0.0, 0.0, 0.0]
        },
    })

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.PlanarDomain(mi.ScalarBoundingBox2f([0, 0], [1, 1])),
        sample_func=lambda s: color_ramp.sample_position(s)[0],
        pdf_func=lambda p: color_ramp.pdf_position(p),
        sample_dim=2,
        res=201,
        ires=8
    )

    assert chi2.run()

@fresolver_append_path
def test04_eval_rgb(variants_vec_backends_once_rgb):
    import numpy as np

    # RGB image
    color_ramp = mi.load_dict({
        'type' : 'color_ramp',
        'input_fac': {
            'type': 'bitmap',
            'filename': 'resources/data/common/textures/carrot.png',
        },
        "mode": "linear",
        "num_band": 2.0,
        "pos0": 0.0,
        "pos1": 1.0,
        "color0": {
            'type': 'rgb',
            'value': [0.0, 0.0, 0.0]
        },
        "color1": {
            'type': 'rgb',
            'value': [1.0, 1.0, 1.0]
        },
    })

    x_res, y_res =  color_ramp.resolution()
    # Coordinates of green pixel: (7, 1)
    x = (1 / x_res) * 7 + (1 / (2 * x_res))
    y = (1 / y_res) * 1 + (1 / (2 * y_res))

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.uv = [x, y]

    mono = color_ramp.eval_1(si)
    spec = color_ramp.eval(si)
    assert dr.allclose(mi.luminance(spec), mono, atol=1e-04)

    # Grayscale image
    color_ramp = mi.load_dict({
        'type' : 'color_ramp',
        'input_fac': {
            'type': 'bitmap',
            'filename': 'resources/data/common/textures/noise_02.png',
        },
        "mode": "linear",
        "num_band": 2,
        "pos0": 0.0,
        "pos1": 1.0,
        "color0": {
            'type': 'rgb',
            'value': [0.0, 0.0, 0.0]
        },
        "color1": {
            'type': 'rgb',
            'value': [1.0, 1.0, 1.0]
        },
    })

    x_res, y_res =  color_ramp.resolution()
    # Coordinates of gray pixel: (0, 15)
    x = (1 / x_res) * 0 + (1 / (2 * x_res))
    y = (1 / y_res) * 15 + (1 / (2 * y_res))

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.uv = [x, y]

    mono = color_ramp.eval_1(si)
    with pytest.raises(RuntimeError):
        color = color_ramp.eval_3(si)
    spec = color_ramp.eval(si)

    expected = 0.5394
    assert dr.allclose(expected, spec, atol=1e-04)
    assert dr.allclose(expected, mono, atol=1e-04)

@fresolver_append_path
def test05_eval_spectral(variants_vec_backends_once_spectral):
    import numpy as np

    # RGB image
    color_ramp = mi.load_dict({
        'type' : 'color_ramp',
        'input_fac': {
            'type': 'bitmap',
            'filename': 'resources/data/common/textures/carrot.png',
            'raw': False
        },
        "mode": "linear",
        "num_band": 2.0,
        "pos0": 0.0,
        "pos1": 1.0,
        "color0": {
            'type': 'rgb',
            'value': [0.0, 0.0, 0.0]
        },
        "color1": {
            'type': 'rgb',
            'value': [1.0, 1.0, 1.0]
        },
    })

    x_res, y_res =  color_ramp.resolution()
    # Coordinates of green pixel: (7, 1)
    x = (1 / x_res) * 7 + (1 / (2 * x_res))
    y = (1 / y_res) * 1 + (1 / (2 * y_res))

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.wavelengths = np.linspace(mi.MI_CIE_MIN, mi.MI_CIE_MAX, mi.MI_WAVELENGTH_SAMPLES)
    si.uv = [x, y]

    spec = color_ramp.eval(si)

    expected = [0.0023, 0.1910, 0.0059, 0.0003]
    assert dr.allclose(expected, spec, atol=1e-04)

    # Grayscale image
    color_ramp = mi.load_dict({
        'type' : 'color_ramp',
        'input_fac': {
            'type': 'bitmap',
            'filename': 'resources/data/common/textures/noise_02.png',
            'raw': False
        },
        "mode": "linear",
        "num_band": 2.0,
        "pos0": 0.0,
        "pos1": 1.0,
        "color0": {
            'type': 'rgb',
            'value': [0.0, 0.0, 0.0]
        },
        "color1": {
            'type': 'rgb',
            'value': [1.0, 1.0, 1.0]
        },
    })

    x_res, y_res =  color_ramp.resolution()
    # Coordinates of gray pixel: (0, 15)
    x = (1 / x_res) * 0 + (1 / (2 * x_res))
    y = (1 / y_res) * 15 + (1 / (2 * y_res))

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.uv = [x, y]

    mono = color_ramp.eval_1(si)
    spec = color_ramp.eval(si)

    expected = 0.5394
    assert dr.allclose(expected, spec, atol=1e-04)
    assert dr.allclose(expected, mono, atol=1e-04)