import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


def create_rectangle_with_extra_uvs():
    mesh = mi.load_dict({"type": "rectangle"})
    mesh.add_attribute(
        "vertex_uv1", 2, [0.5, 0.5, 1.5, 0.5, 0.5, 1.5, 1.5, 1.5]
    )
    return mesh


def test01_uv_selector_checkerboard(variant_scalar_rgb):
    mesh = create_rectangle_with_extra_uvs()
    tensor = mi.TensorXf(
        np.array(
            [[[0.1, 0.1, 0.1], [0.4, 0.9, 0.9]], [[0.5, 0.9, 0.1], [0.1, 0.3, 0.1]]]
        )
    )
    bitmap = mi.load_dict({"type": "bitmap", "bitmap": mi.Bitmap(tensor)})

    texture = mi.load_dict(
        {"type": "uv_set_selector", "uv_set": "vertex_uv1", "nested": bitmap}
    )

    for u, v in [(0.0, 0.0), (0.4, 0.4), (0.5, 0.5), (0.9, 0.2)]:
        si = mesh.eval_parameterization([u, v])
        actual = texture.eval(si)
        si.uv += mi.Point2f(0.5, 0.5)
        expected = bitmap.eval(si)
        assert dr.allclose(actual, expected)


def test02_invalid_uv_set(variant_scalar_rgb):
    with pytest.raises(Exception):
        mi.load_dict(
            {
                "type": "uv_set_selector",
                "uv_set": "invalid_name",
                "nested": {"type": "checkerboard"},
            }
        )

