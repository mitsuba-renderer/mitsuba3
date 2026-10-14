"""The emitter selection interface: Emitter.bounds(), Shape.normal_cone()
and Emitter.irradiance_estimate()"""

import math

import numpy as np
import pytest
import drjit as dr
import mitsuba as mi


def write_obj(path, vertices, faces):
    with open(path, 'w') as f:
        for v in vertices:
            f.write('v %g %g %g\n' % tuple(v))
        for face in faces:
            f.write('f %d %d %d\n' % tuple(i + 1 for i in face))
    return str(path)


def bitmap(shape, value):
    return mi.Bitmap(np.full(shape, value, dtype=np.float32))


# Emitter definition and the expected bounds, or None for infinite emitters
BOUNDS = {
    'point': (
        lambda: {'type': 'point', 'position': [1, 2, 3],
                 'intensity': {'type': 'rgb', 'value': [2, 4, 1]}},
        {'bbox': ([1, 2, 3], [1, 2, 3]), 'theta_o': math.pi,
         'theta_e': math.pi / 2, 'energy': 4.0}),
    'spot': (
        lambda: {'type': 'spot', 'cutoff_angle': 30, 'intensity': 5.0,
                 'to_world': mi.ScalarTransform4f().look_at([0, 0, 5], [1, 0, 5], [0, 0, 1])},
        {'bbox': ([0, 0, 5], [0, 0, 5]), 'axis': [1, 0, 0], 'theta_o': 0,
         'theta_e': math.radians(30), 'energy': 5.0}),
    'projector': (
        # Half the diagonal of a 16:8 image plane at unit distance
        lambda: {'type': 'projector', 'fov': 90, 'scale': 2.0,
                 'to_world': mi.ScalarTransform4f().translate([1, 0, 0]),
                 'irradiance': {'type': 'bitmap', 'bitmap': bitmap((8, 16, 3), 3.0)}},
        {'bbox': ([1, 0, 0], [1, 0, 0]), 'axis': [0, 0, 1], 'theta_o': 0,
         'theta_e': math.atan(math.sqrt(1.25)), 'energy': 2.0}),
    'rectangle': (
        # The rectangle normal +Z turns into -Y
        lambda: {'type': 'rectangle', 'emitter': {'type': 'area', 'radiance': 0.5},
                 'to_world': mi.ScalarTransform4f().translate([0, 0, 2])
                     .rotate([1, 0, 0], 90).scale([3, 2, 1])},
        {'bbox': ([-3, 0, 0], [3, 0, 4]), 'axis': [0, -1, 0], 'theta_o': 0,
         'theta_e': math.pi / 2, 'energy': 0.5 * 24}),
    'sphere': (
        lambda: {'type': 'sphere', 'radius': 2,
                 'emitter': {'type': 'area', 'radiance': 3.0}},
        {'bbox': ([-2, -2, -2], [2, 2, 2]), 'theta_o': math.pi,
         'energy': 3.0 * 4 * math.pi * 4}),
    'disk': (
        lambda: {'type': 'disk', 'flip_normals': True,
                 'emitter': {'type': 'area', 'radiance': 1.0}},
        {'axis': [0, 0, -1], 'theta_o': 0, 'energy': math.pi}),
    'textured_area': (
        # A texture counts as a modulation bounded by one
        lambda: {'type': 'rectangle', 'emitter': {'type': 'area', 'radiance': {
                     'type': 'bitmap', 'bitmap': bitmap((4, 4, 3), 5.0)}}},
        {'energy': 4.0}),
    'directionalarea': (
        lambda: {'type': 'rectangle',
                 'emitter': {'type': 'directionalarea', 'radiance': 2.0}},
        {'axis': [0, 0, 1], 'theta_o': 0, 'theta_e': 0, 'energy': 8.0}),
    'constant': (lambda: {'type': 'constant'}, None),
    'directional': (lambda: {'type': 'directional', 'direction': [0, 0, -1]}, None),
    'envmap': (lambda: {'type': 'envmap', 'bitmap': bitmap((4, 8, 3), 1.0)}, None),
}


@pytest.mark.parametrize('name', BOUNDS.keys())
def test01_bounds(variants_all_rgb, name):
    make, expected = BOUNDS[name]
    obj = mi.load_dict(make())
    emitter = obj.emitter() if isinstance(obj, mi.Shape) else obj
    if expected is None:
        with pytest.raises(RuntimeError, match='finite extent'):
            emitter.bounds()
        return
    b = emitter.bounds()
    for key, value in expected.items():
        if key == 'bbox':
            assert dr.allclose(b.bbox.min, value[0]) and dr.allclose(b.bbox.max, value[1])
        elif key == 'axis':
            assert dr.allclose(b.axis, value, atol=1e-6)
        else:
            assert getattr(b, key) == pytest.approx(value, abs=1e-6)


def test02_mesh_normal_cone(variants_all_rgb, tmp_path):
    # Two unit squares meeting at a right angle along the x axis: their
    # normals +Z and +Y are 45 degrees from the cone axis
    vertices = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0),
                (1, 0, -1), (0, 0, -1)]
    faces = [(0, 1, 2), (0, 2, 3), (0, 1, 4), (0, 4, 5)]
    path = write_obj(tmp_path / 'tent.obj', vertices, faces)
    shape = mi.load_dict({'type': 'obj', 'filename': path,
                          'emitter': {'type': 'area', 'radiance': 2.0}})
    axis, theta_o = shape.normal_cone()
    assert dr.allclose(axis, [0, 1 / math.sqrt(2), 1 / math.sqrt(2)], atol=1e-5)
    assert theta_o == pytest.approx(math.pi / 4, abs=1e-5)
    b = shape.emitter().bounds()
    assert dr.allclose(b.axis, axis)
    assert b.theta_o == pytest.approx(theta_o)
    assert b.energy == pytest.approx(4.0)

    # A closed box has no preferred direction
    cube = mi.load_dict({'type': 'cube', 'emitter': {'type': 'area'}})
    assert cube.normal_cone()[1] == pytest.approx(math.pi)


# Emitter definition and the expected direction and irradiance, or None for
# emitters of finite extent
IRRADIANCE = {
    'constant': (lambda: {'type': 'constant', 'radiance': 0.5},
                 ([0, 0, 0], 0.5 * math.pi)),
    'directional': (lambda: {'type': 'directional', 'direction': [0, 0, -1],
                             'irradiance': 2.0},
                    ([0, 0, 1], 2.0)),
    'envmap': (lambda: {'type': 'envmap', 'scale': 2.0,
                        'bitmap': bitmap((4, 8, 3), 0.25)},
               ([0, 0, 0], 0.5 * math.pi)),
    'point': (lambda: {'type': 'point'}, None),
}


@pytest.mark.parametrize('name', IRRADIANCE.keys())
def test03_irradiance_estimate(variants_all_rgb, name):
    make, expected = IRRADIANCE[name]
    emitter = mi.load_dict(make())
    if expected is None:
        with pytest.raises(RuntimeError, match='not an infinite emitter'):
            emitter.irradiance_estimate()
        return
    d, e = emitter.irradiance_estimate()
    assert dr.allclose(d, expected[0], atol=1e-6)
    assert e == pytest.approx(expected[1], rel=1e-3)
