import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path


def _single_ellipsoid_data():
    pytest.importorskip("numpy")
    import numpy as np

    return mi.TensorXf(np.array([
        [0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0]
    ], dtype=np.float32))


def _assert_invalid(value):
    assert not bool(dr.any(value))


def _assert_valid(value):
    assert bool(dr.all(value))


def test01_backface_culling_contract(variants_all_rgb):
    scene = mi.load_dict({
        "type": "scene",
        "em": {
            "type": "ellipsoidsmesh",
            "data": _single_ellipsoid_data(),
            "extent": 1.0,
            "shell": "ico_sphere"
        }
    })

    ray = mi.Ray3f(o=mi.Point3f([0, 0, 0]), d=mi.Vector3f([0, 0, -1]))
    _assert_invalid(scene.ray_test(ray))
    _assert_invalid(scene.ray_intersect_preliminary(ray).is_valid())
    _assert_invalid(scene.ray_intersect(ray, mi.RayFlags.All, True).is_valid())

    ray = mi.Ray3f(o=mi.Point3f([0, 0, 2]), d=mi.Vector3f([0, 0, -1]))
    _assert_valid(scene.ray_test(ray))
    _assert_valid(scene.ray_intersect_preliminary(ray).is_valid())
    _assert_valid(scene.ray_intersect(ray, mi.RayFlags.All, True).is_valid())


@fresolver_append_path
def test02_mixed_mesh_backfaces_remain_double_sided(variants_all_rgb):
    from mitsuba import ScalarTransform4f as T

    scene = mi.load_dict({
        "type": "scene",
        "group": {
            "type": "shapegroup",
            "mesh": {
                "type": "obj",
                "filename": "resources/data/common/meshes/rectangle.obj",
                "to_world": T().translate([3.0, 0.0, 0.0])
            },
            "em": {
                "type": "ellipsoidsmesh",
                "data": _single_ellipsoid_data(),
                "extent": 1.0,
                "shell": "ico_sphere"
            }
        },
        "inst": {
            "type": "instance",
            "to_world": T(),
            "shapegroup": {
                "type": "ref",
                "id": "group"
            }
        }
    })

    ray = mi.Ray3f(o=mi.Point3f([3, 0, -2]), d=mi.Vector3f([0, 0, 1]))
    _assert_valid(scene.ray_test(ray))
    _assert_valid(scene.ray_intersect_preliminary(ray).is_valid())
    _assert_valid(scene.ray_intersect(ray, mi.RayFlags.All, True).is_valid())
