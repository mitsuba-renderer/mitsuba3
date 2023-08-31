import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path

@fresolver_append_path
def test01_create(variant_scalar_rgb):
    import numpy as np

    color_ramp = mi.load_dict({
        'type' : 'color_ramp',
        'input': 0.5,
        "mode": "linear",
        "num_bands": 2,
        "pos0": 0.0,
        "pos1": 1.0,
        "color0" : [0.0, 0.0, 0.0],
        "color1" : [1.0, 1.0, 1.0]
    })

    assert color_ramp is not None

    color_ramp = mi.load_dict({
        'type' : 'color_ramp',
        'input': {
            'type' : 'bitmap',
            'filename' : 'resources/data/common/textures/carrot.png'
        },
        "mode": "constant",
        "num_bands": 4,
        "pos0": 0.0,
        "pos1": 0.2,
        "pos2": 0.5,
        "pos3": 0.9,
        "color0" : [0.0, 0.0, 0.0],
        "color1" : [1.0, 0.0, 1.0],
        "color2" : [1.0, 1.0, 1.0],
        "color3" : [1.0, 1.0, 1.0]
    })

    assert color_ramp is not None

def test02_error(variant_scalar_rgb):

    # Invalid num of bands
    with pytest.raises(RuntimeError):
        color_ramp = mi.load_dict({
            'type': 'color_ramp',
            'num_bands': 0
        })

    # Missing positions and colors
    with pytest.raises(RuntimeError):
        color_ramp = mi.load_dict({
            'type': 'color_ramp',
            'num_bands': 2
        })

    # Invalid positions
    with pytest.raises(RuntimeError):
        color_ramp = mi.load_dict({
            'type': 'color_ramp',
            'num_bands': 2,
            'pos0': 0.0,
            'pos1': -1.0,
            'color0' : [0.0, 0.0, 0.0],
            'color1' : [1.0, 1.0, 1.0]
        })

    # Invalid position sequence
    with pytest.raises(RuntimeError):
        color_ramp = mi.load_dict({
            'type': 'color_ramp',
            'num_bands': 2,
            'pos0': 0.5,
            'pos1': 0.0,
            'color0' : [0.0, 0.0, 0.0],
            'color1' : [1.0, 1.0, 1.0]
        })


def test03_eval_rgb(variants_vec_backends_once_rgb):

    # test linear basic
    color_ramp = mi.load_dict({
        'type' : 'color_ramp',
        'input': 0.5,
        "mode": "linear",
        "num_bands": 2,
        "pos0": 0.0,
        "pos1": 1.0,
        "color0" : [0.0, 0.0, 0.0],
        "color1" : [1.0, 1.0, 1.0]
    })

    si = dr.zeros(mi.SurfaceInteraction3f)
    assert dr.allclose(color_ramp.eval(si), [0.5, 0.5, 0.5])

    # constant basic
    color_ramp = mi.load_dict({
        'type' : 'color_ramp',
        'input': 0.5,
        "mode": "constant",
        "num_bands": 2,
        "pos0": 0.0,
        "pos1": 1.0,
        "color0" : [0.0, 0.0, 0.0],
        "color1" : [1.0, 1.0, 1.0]
    })

    assert dr.allclose(color_ramp.eval(si), [0.0, 0.0, 0.0])

    # test linear right-padding
    color_ramp = mi.load_dict({
        'type' : 'color_ramp',
        'input': 0.9,
        "mode": "linear",
        "num_bands": 2,
        "pos0": 0.3,
        "pos1": 0.8,
        "color0" : [1.0, 0.0, 0.0],
        "color1" : [1.0, 1.0, 1.0]
    })

    assert dr.allclose(color_ramp.eval(si), [1.0, 1.0, 1.0])

    # test linear left-padding
    color_ramp = mi.load_dict({
        'type' : 'color_ramp',
        'input': 0.1,
        "mode": "linear",
        "num_bands": 2,
        "pos0": 0.3,
        "pos1": 0.8,
        "color0" : [1.0, 0.0, 0.0],
        "color1" : [1.0, 1.0, 1.0]
    })

    assert dr.allclose(color_ramp.eval(si), [1.0, 0.0, 0.0])

    # constant multi positions
    color_ramp = mi.load_dict({
        'type' : 'color_ramp',
        'input': 0.5,
        "mode": "constant",
        "num_bands": 4,
        "pos0": 0.0,
        "pos1": 0.4,
        "pos2": 0.7,
        "pos3": 1.0,
        "color0" : [1.0, 0.0, 0.0],
        "color1" : [1.0, 1.0, 1.0],
        "color2" : [1.0, 0.0, 1.0],
        "color3" : [0.9, 0.0, 1.0],
    })

    assert dr.allclose(color_ramp.eval(si), [1.0, 1.0, 1.0])