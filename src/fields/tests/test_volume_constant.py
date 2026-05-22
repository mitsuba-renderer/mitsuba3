import pytest
import drjit as dr
import mitsuba as mi


def test01_constant_construct(variant_scalar_rgb):
    vol = mi.load_dict({
        "type": "constvolume",
        "to_world": mi.ScalarTransform4f().scale([2, 0.2, 1]),
        "value": {
            "type": "regular",
            "wavelength_min": 500,
            "wavelength_max": 600,
            "values": "3.0, 3.0"
        }
    })

    assert vol is not None
    assert vol.bbox() == mi.BoundingBox3f([0, 0, 0], [2, 0.2, 1])


def test02_constant_eval(variant_scalar_rgb):
    vol = mi.load_dict({
        "type": "constvolume",
        "value": {
            "type" : "srgb",
            "color" : mi.Color3f([0.5, 1.0, 0.3])
        }
    })

    it = dr.zeros(mi.Interaction3f, 1)
    assert dr.allclose(vol.eval(it), mi.Color3f(0.5, 1.0, 0.3))
    assert vol.bbox() == mi.BoundingBox3f([0, 0, 0], [1, 1, 1])
