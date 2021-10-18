import mitsuba
import pytest
import enoki as ek
from enoki.scalar import ArrayXf as Float

def test01_create(variant_scalar_rgb):
    from mitsuba.core import xml, ScalarTransform4f

    s = xml.load_dict({"type" : "cube"})
    assert s is not None
    assert s.primitive_count() == 12
    assert ek.allclose(s.surface_area(), 24.0)


def test02_bbox(variant_scalar_rgb):
    from mitsuba.core import xml, Transform4f

    for r in [1, 2, 4]:
        s = xml.load_dict({
            "type" : "cube",
            "to_world" : Transform4f.scale([r, r, r])
        })

        b = s.bbox()
        assert b.valid()
        assert ek.allclose(b.center(), [0, 0, 0])
        assert ek.allclose(b.min, [-r, -r, -r])
        assert ek.allclose(b.max, [r, r, r])

def test03_ray_intersect(variant_scalar_rgb):
    from mitsuba.core import xml, Ray3f, Transform4f, Vector3f, Vector2f

    for scale in [Vector3f([1.0, 1.0, 1.0]), Vector3f([2.0, 1.0, 1.0]),
                    Vector3f([1.0, 2.0, 1.0])]:
        for coordsX in [-1.5, -0.9, -0.5, 0, 0.5, 0.9, 1.5]:
            for coordsY in [-1.5, -0.9, -0.5, 0, 0.5, 0.9, 1.5]:
                s = xml.load_dict({
                    "type" : "scene",
                    "foo" : {
                        "type" : 'cube',
                        "to_world": Transform4f.scale(scale),
                    }

                })

                ray = Ray3f(o=[coordsX, coordsY, -8], d=[0.0, 0.0, 1.0],
                                    time=0.0, wavelengths=[])

                si_found = s.ray_test(ray)
                assert si_found == ((abs(coordsX) <= scale.x) and (abs(coordsY) <= scale.y))

                if si_found:
                    si = s.ray_intersect(ray)
                    ray_u = Ray3f(ray)
                    ray_v = Ray3f(ray)
                    eps = 1e-4
                    ray_u.o += si.dp_du * eps
                    ray_v.o += si.dp_dv * eps
                    si_u = s.ray_intersect(ray_u)
                    si_v = s.ray_intersect(ray_v)
                    if si_u.is_valid():
                        du = (si_u.uv - si.uv) / eps
                        assert ek.allclose(du, [1, 0], atol=2e-2)
                    if si_v.is_valid():
                        dv = (si_v.uv - si.uv) / eps
                        assert ek.allclose(dv, [0, 1], atol=2e-2)

                    # Check normal
                    assert si.n == Vector3f([0,0,-1])

                    # Check UV
                    oo  = (coordsY - (-scale.y)) / ((scale.y) - (-scale.y))
                    tt = (coordsX - (-scale.x)) / (scale.x - (-scale.x ))
                    assert ek.allclose(si.uv, Vector2f([1.0-oo, tt]), atol=1e-5, rtol=1e-5)

def test04_ray_intersect_vec(variant_scalar_rgb):
    from mitsuba.python.test.util import check_vectorization

    def kernel(o):
        from mitsuba.core import xml, ScalarTransform4f
        from mitsuba.core import Ray3f

        scene = xml.load_dict({
            "type" : "scene",
            "foo" : {
                "type" : "cube",
                "to_world" : ScalarTransform4f.scale((0.5, 0.5, 0.5))
            }
        })

        o = 2.0 * o - 1.0
        o.z = -8.0

        t = scene.ray_intersect(Ray3f(o, [0, 0, 1])).t
        ek.eval(t)
        return t

    check_vectorization(kernel, arg_dims = [3], atol=1e-5)


def test05_check_normals(variant_scalar_rgb):
    from mitsuba.core import xml, Vector3f, Ray3f

    s = xml.load_dict({
        "type" : "scene",
        "foo" : {
            "type" : 'cube'
        }
    })

    for face in [
        (Vector3f(0,0,-8), Vector3f(0,0,1), Vector3f(0,0,-1)),
        (Vector3f(0,0,8), Vector3f(0,0,-1), Vector3f(0,0,1)),
        (Vector3f(-8,0,0), Vector3f(1,0,0), Vector3f(-1,0,0)),
        (Vector3f(8,0,0), Vector3f(-1,0,0), Vector3f(1,0,0)),
        (Vector3f(0,-8,0), Vector3f(0,1,0), Vector3f(0,-1,0)),
        (Vector3f(0,8,0), Vector3f(0,-1,0), Vector3f(0,1,0)),
    ]:
        ray = Ray3f(o=face[0], d=face[1],
                                    time=0.0, wavelengths=[])
        si = s.ray_intersect(ray)
        assert si.n == face[2]

    ss = xml.load_dict({
        "type" : "scene",
        "foo" : {
            "type" : 'cube',
            "flip_normals": True
        }
    })

    for face in [
        (Vector3f(0,0,-8), Vector3f(0,0,1), Vector3f(0,0,1)),
        (Vector3f(0,0,8), Vector3f(0,0,-1), Vector3f(0,0,-1)),
        (Vector3f(-8,0,0), Vector3f(1,0,0), Vector3f(1,0,0)),
        (Vector3f(8,0,0), Vector3f(-1,0,0), Vector3f(-1,0,0)),
        (Vector3f(0,-8,0), Vector3f(0,1,0), Vector3f(0,1,0)),
        (Vector3f(0,8,0), Vector3f(0,-1,0), Vector3f(0,-1,0)),
    ]:
        ray = Ray3f(o=face[0], d=face[1],
                                    time=0.0, wavelengths=[])
        si2 = ss.ray_intersect(ray)
        assert si2.n == face[2]
