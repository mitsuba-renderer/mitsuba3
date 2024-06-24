import pytest
import drjit as dr
import mitsuba as mi


def test01_create(variant_scalar_rgb):
    s = mi.load_dict({"type" : "cube"})
    assert s is not None
    assert s.primitive_count() == 12
    assert dr.allclose(s.surface_area(), 24.0)


def test02_bbox(variant_scalar_rgb):
    for r in [1, 2, 4]:
        s = mi.load_dict({
            "type" : "cube",
            "to_world" : mi.Transform4f().scale([r, r, r])
        })

        b = s.bbox()
        assert b.valid()
        assert dr.allclose(b.center(), [0, 0, 0])
        assert dr.allclose(b.min, [-r, -r, -r])
        assert dr.allclose(b.max, [r, r, r])

def test03_ray_intersect(variant_scalar_rgb):
    for scale in [mi.Vector3f([1.0, 1.0, 1.0]), mi.Vector3f([2.0, 1.0, 1.0]),
            mi.Vector3f([1.0, 2.0, 1.0])]:
        for coordsX in [-1.5, -0.9, -0.5, 0, 0.5, 0.9, 1.5]:
            for coordsY in [-1.5, -0.9, -0.5, 0, 0.5, 0.9, 1.5]:
                s = mi.load_dict({
                    "type" : "scene",
                    "foo" : {
                        "type" : 'cube',
                        "to_world": mi.Transform4f().scale(scale),
                    }

                })

                ray = mi.Ray3f(o=[coordsX, coordsY, -8], d=[0.0, 0.0, 1.0],
                        time=0.0, wavelengths=[])

                si_found = s.ray_test(ray)
                assert si_found == ((abs(coordsX) <= scale.x) and (abs(coordsY) <= scale.y))

                if si_found:
                    si = s.ray_intersect(ray)
                    ray_u = mi.Ray3f(ray)
                    ray_v = mi.Ray3f(ray)
                    eps = 1e-4
                    ray_u.o += si.dp_du * eps
                    ray_v.o += si.dp_dv * eps
                    si_u = s.ray_intersect(ray_u)
                    si_v = s.ray_intersect(ray_v)
                    if si_u.is_valid():
                        du = (si_u.uv - si.uv) / eps
                        assert dr.allclose(du, [1, 0], atol=2e-2)
                    if si_v.is_valid():
                        dv = (si_v.uv - si.uv) / eps
                        assert dr.allclose(dv, [0, 1], atol=2e-2)

                    # Check normal
                    assert dr.all(si.n == mi.Vector3f([0,0,-1]))

                    # Check UV
                    oo  = (coordsY - (-scale.y)) / ((scale.y) - (-scale.y))
                    tt = (coordsX - (-scale.x)) / (scale.x - (-scale.x ))
                    assert dr.allclose(si.uv, mi.Vector2f([1.0-oo, tt]), atol=1e-5, rtol=1e-5)

def test04_ray_intersect_vec(variant_scalar_rgb):
    from mitsuba.scalar_rgb.test.util import check_vectorization

    def kernel(o):
        scene = mi.load_dict({
            "type" : "scene",
            "foo" : {
                "type" : "cube",
                "to_world" : mi.ScalarTransform4f().scale((0.5, 0.5, 0.5))
            }
        })

        o = 2.0 * o - 1.0
        o.z = -8.0

        t = scene.ray_intersect(mi.Ray3f(o, [0, 0, 1])).t
        dr.eval(t)
        return t

    check_vectorization(kernel, arg_dims = [3], atol=1e-5)


def test05_check_normals(variant_scalar_rgb):
    s = mi.load_dict({
        "type" : "scene",
        "foo" : {
            "type" : 'cube'
        }
    })

    for face in [
        (mi.Vector3f(0,0,-8), mi.Vector3f(0,0,1), mi.Vector3f(0,0,-1)),
        (mi.Vector3f(0,0,8), mi.Vector3f(0,0,-1), mi.Vector3f(0,0,1)),
        (mi.Vector3f(-8,0,0), mi.Vector3f(1,0,0), mi.Vector3f(-1,0,0)),
        (mi.Vector3f(8,0,0), mi.Vector3f(-1,0,0), mi.Vector3f(1,0,0)),
        (mi.Vector3f(0,-8,0), mi.Vector3f(0,1,0), mi.Vector3f(0,-1,0)),
        (mi.Vector3f(0,8,0), mi.Vector3f(0,-1,0), mi.Vector3f(0,1,0)),
    ]:
        ray = mi.Ray3f(o=face[0], d=face[1],
                                    time=0.0, wavelengths=[])
        si = s.ray_intersect(ray)
        assert dr.all(si.n == face[2])

    ss = mi.load_dict({
        "type" : "scene",
        "foo" : {
            "type" : 'cube',
            "flip_normals": True
        }
    })

    for face in [
        (mi.Vector3f(0,0,-8), mi.Vector3f(0,0,1), mi.Vector3f(0,0,1)),
        (mi.Vector3f(0,0,8), mi.Vector3f(0,0,-1), mi.Vector3f(0,0,-1)),
        (mi.Vector3f(-8,0,0), mi.Vector3f(1,0,0), mi.Vector3f(1,0,0)),
        (mi.Vector3f(8,0,0), mi.Vector3f(-1,0,0), mi.Vector3f(-1,0,0)),
        (mi.Vector3f(0,-8,0), mi.Vector3f(0,1,0), mi.Vector3f(0,1,0)),
        (mi.Vector3f(0,8,0), mi.Vector3f(0,-1,0), mi.Vector3f(0,-1,0)),
    ]:
        ray = mi.Ray3f(o=face[0], d=face[1],
                                    time=0.0, wavelengths=[])
        si2 = ss.ray_intersect(ray)
        assert dr.all(si2.n == face[2])


def test06_medium_accesors(variants_vec_rgb):
    shape = mi.load_dict({
        'interior': {
            'type': 'homogeneous',
            'albedo': {
                'type': 'rgb',
                'value': [0.99, 0.9, 0.96]
            }
        },
        'exterior': {
            'type': 'homogeneous',
            'albedo': {
                'type': 'rgb',
                'value': [0.1, 0.1, 0.12]
            }
        },
        'type': 'cube',
    })

    shape_ptr = mi.ShapePtr(shape)

    assert type(shape.interior_medium()) == mi.Medium
    assert type(shape_ptr.interior_medium()) == mi.MediumPtr

    assert type(shape.exterior_medium()) == mi.Medium
    assert type(shape_ptr.exterior_medium()) == mi.MediumPtr


def test07_bsdf_accesors(variants_vec_rgb):
    shape = mi.load_dict({
       'type': 'cube',
       'bsdf': {
           'type': 'diffuse',
       }
    })
    shape_ptr = mi.ShapePtr(shape)

    assert type(shape.bsdf()) == mi.BSDF
    assert type(shape_ptr.bsdf()) == mi.BSDFPtr
