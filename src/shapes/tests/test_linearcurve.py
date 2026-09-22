import pytest
import drjit as dr
import mitsuba as mi

from drjit.scalar import ArrayXf as Float
from mitsuba.scalar_rgb.test.util import fresolver_append_path

@fresolver_append_path
def test01_create(variants_all_rgb):
    s = mi.load_dict({
        "type" : "linearcurve",
        "filename" : "resources/data/common/meshes/curve.txt",
    })
    assert s is not None
    assert s.primitive_count() == 3


@fresolver_append_path
def test02_create_multiple_curves(variants_all_rgb):
    s = mi.load_dict({
        "type" : "linearcurve",
        "filename" : "resources/data/common/meshes/curve_6.txt",
    })
    assert s is not None
    assert s.primitive_count() == 3 + 5 + 3 + 3


@fresolver_append_path
def test02_bbox(variants_all_rgb):
    T = mi.ScalarTransform4f
    for sx in [1, 2, 4]:
        for translate in [mi.ScalarVector3f([1.3, -3.0, 5]),
                          mi.ScalarVector3f([-10000, 3.0, 31])]:
            s = mi.load_dict({
                "type" : "linearcurve",
                "filename" : "resources/data/common/meshes/curve.txt",
                "to_world" : T().translate(translate) @ T().scale((sx, 1, 1))
            })
            b = s.bbox()

            assert b.valid()
            assert dr.allclose(b.center(), translate)
            assert dr.allclose(b.min, translate - [1, 0, 0] - [sx, 1, 1])
            assert dr.allclose(b.max, translate + [1, 0, 0] + [sx, 1, 1])


@fresolver_append_path
def test03_parameters_changed(variants_vec_rgb):
    pytest.importorskip("numpy")
    import numpy as np

    new_control_points = np.array([
        1, 0, 0, 1,
        2, 0, 0, 1,
        3, 0, 0, 1,
        4, 0, 0, 1,
    ]).reshape((4, 4))
    new_control_points = mi.Point4f(new_control_points)

    s = mi.load_dict({
        "type" : "linearcurve",
        "filename" : "resources/data/common/meshes/curve.txt",
    })

    params = mi.traverse(s)
    params['control_points'] = dr.ravel(new_control_points)
    params.update()

    params = mi.traverse(s)
    assert dr.allclose(dr.ravel(new_control_points), params['control_points'])


@fresolver_append_path
def test04_ray_intersect(variant_scalar_rgb):
    for translate in [mi.Vector3f([0.0, 0.0, 0.0]),
                      mi.Vector3f([0.0, 0.0, 0.0])]:
        s = mi.load_dict({
            "type" : "scene",
            "foo" : {
                "type" : "linearcurve",
                "filename" : "resources/data/common/meshes/curve.txt",
                "to_world" : mi.Transform4f().translate(translate).scale(10)
            }
        })

        # grid size
        n = 10
        for x in dr.linspace(Float, -0.5, 0.5, n):
            for y in dr.linspace(Float, -1.5, 1.5, n):
                x = x * 10 - translate[0]
                y = y - translate[1]

                ray = mi.Ray3f(o=[x, y, -10], d=[0, 0, 1],
                            time=0.0, wavelengths=[])
                si_found = s.ray_test(ray)

                assert si_found == (x <= 3 + 1 and -3 - 1 <= x and y <= 1 and -1 <= y)

                if si_found:
                    ray = mi.Ray3f(o=[x, y, -10], d=[0, 0, 1],
                                time=0.0, wavelengths=[])

                    si = s.ray_intersect(ray, mi.RayFlags.Default, True)
                    ray_u = mi.Ray3f(ray)
                    ray_v = mi.Ray3f(ray)
                    eps = 1e-4
                    ray_u.o += si.dp_du * eps
                    ray_v.o += si.dp_dv * eps
                    si_u = s.ray_intersect(ray_u)
                    si_v = s.ray_intersect(ray_v)

                    if si_u.is_valid():
                        dp_du = (si_u.p - si.p) / eps
                        assert dr.allclose(dp_du, si.dp_du, atol=2e-2)
                    if si_v.is_valid():
                        dp_dv = (si_v.p - si.p) / eps
                        assert dr.allclose(dp_dv, si.dp_dv, atol=2e-2)


@fresolver_append_path
def test05_ray_intersect_vec(variant_scalar_rgb):
    from mitsuba.scalar_rgb.test.util import check_vectorization

    def kernel(o):
        scene = mi.load_dict({
            "type" : "scene",
            "foo" : {
                "type" : "linearcurve",
                "filename" : "resources/data/common/meshes/curve.txt",
            }
        })

        o = 2.0 * o - 1.0
        o.z = 5.0

        t = scene.ray_intersect(mi.Ray3f(o, [0, 0, -1])).t
        dr.eval(t)
        return t

    check_vectorization(kernel, arg_dims = [3], atol=1e-5)


@fresolver_append_path
def test08_instancing(variants_all_rgb):
    T = mi.ScalarTransform4f
    scene = mi.load_dict({
        "type" : "scene",
        "group": {
            "type" : "shapegroup",
            "curve" : {
                "type" : "linearcurve",
                "filename" : "resources/data/common/meshes/curve.txt",
            }
        },
        "instance1": {
            "type" : "instance",
            "to_world": T().translate([0, -2, 0]),
            "shapegroup": {
                "type": "ref",
                "id": "group"
            }
        },
        "instance2": {
            "type" : "instance",
            "to_world": T().translate([0, 2, 0]),
            "shapegroup": {
                "type": "ref",
                "id": "group"
            }
        }
    })

    ray1 = mi.Ray3f(o=mi.Point3f(0, -2, 2), d=mi.Vector3f(0, 0, -1))
    pi1 = scene.ray_intersect_preliminary(ray1)

    ray2 = mi.Ray3f(o=mi.Point3f(0, 2, 2), d=mi.Vector3f(0, 0, -1))
    pi2 = scene.ray_intersect_preliminary(ray2)

    assert dr.all(pi1.is_valid())
    assert dr.all(pi2.is_valid())


@fresolver_append_path
def test09_backface_culling(variants_vec_rgb):
    if "metal" in variants_vec_rgb:
        pytest.skip("Metal accepts inside-tube round curve hits.")

    scene =  mi.load_dict({
        "type" : "scene",
        "foo" : {
            "type" : "linearcurve",
            "filename" : "resources/data/common/meshes/curve.txt",
        }
    })

    # Ray inside the curve
    ray = mi.Ray3f(o=[0, 0, 0], d=[0, 0, -1])
    si = scene.ray_intersect(ray)
    assert dr.all(~si.is_valid())

    # Ray outside the curve
    ray = mi.Ray3f(o=[0, 0, 2], d=[0, 0, -1])
    si = scene.ray_intersect(ray)
    assert dr.all(si.is_valid())


@fresolver_append_path
def test10_shape_type(variant_scalar_rgb):
    curve = mi.load_dict({
        "type" : "linearcurve",
        "filename" : "resources/data/common/meshes/curve_6.txt",
    })
    assert curve.shape_type() == mi.ShapeType.LinearCurve.value;


def read_text_curves(filename):
    """Parse a curve text file into (control points, offsets) arrays"""
    import numpy as np
    points, offsets, new_curve = [], [], True
    for line in open(str(mi.file_resolver().resolve(filename))):
        if not line.strip():
            new_curve = True
            continue
        if new_curve:
            offsets.append(len(points))
            new_curve = False
        points.append([float(v) for v in line.split()])
    offsets.append(len(points))
    return (np.array(points, dtype=np.float32),
            np.array(offsets, dtype=np.uint32))


def write_packed_curves(filename, entries):
    """Write ``entries`` (name -> (control points, offsets)) to a container"""
    import struct
    pf = mi.PackedFile(filename)
    for name, (points, offsets) in entries.items():
        pf.begin(name)
        pf.stream().write(b'CURV' + struct.pack('<III', 1, len(offsets) - 1,
                                                len(points)))
        pf.write_array(offsets.tobytes())
        pf.write_array(points.tobytes())
    pf.close()


@fresolver_append_path
def test11_packed_container(variants_all_rgb, tmp_path):
    pytest.importorskip("numpy")
    import numpy as np

    entries = {
        'single': 'curve.txt',
        'multiple': 'curve_6.txt',
    }
    filename = str(tmp_path / 'curves.packed')
    write_packed_curves(filename, {
        name: read_text_curves(f'resources/data/common/meshes/{text}')
        for name, text in entries.items()})

    T = mi.ScalarTransform4f
    to_world = T().translate([1, 2, 3]) @ T().scale(2)
    for index, (name, text) in enumerate(entries.items()):
        for selector in ({'index': index}, {'name': name}):
            packed = mi.load_dict({
                'type': 'linearcurve',
                'filename': filename,
                'to_world': to_world,
                **selector,
            })
            reference = mi.load_dict({
                'type': 'linearcurve',
                'filename': f'resources/data/common/meshes/{text}',
                'to_world': to_world,
            })
            assert packed.primitive_count() == reference.primitive_count()
            p1 = mi.traverse(packed)
            p2 = mi.traverse(reference)
            assert dr.allclose(p1['control_points'], p2['control_points'])
            assert dr.all(p1['segment_indices'] == p2['segment_indices'])
            assert dr.allclose(packed.bbox().min, reference.bbox().min)
            assert dr.allclose(packed.bbox().max, reference.bbox().max)

    with pytest.raises(RuntimeError, match='no entry named'):
        mi.load_dict({'type': 'linearcurve', 'filename': filename,
                      'name': 'missing'})
    with pytest.raises(RuntimeError, match='out of range'):
        mi.load_dict({'type': 'linearcurve', 'filename': filename,
                      'index': 2})

    # Curves of a single control point are rejected
    points, _ = read_text_curves('resources/data/common/meshes/curve.txt')
    write_packed_curves(filename, {
        'short': (points[:1], np.array([0, 1], dtype=np.uint32))})
    with pytest.raises(RuntimeError, match='at least 2 control points'):
        mi.load_dict({'type': 'linearcurve', 'filename': filename})
