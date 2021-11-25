import mitsuba
import pytest
import enoki as ek
from enoki.scalar import ArrayXf as Float


def test01_create(variant_scalar_rgb):
    from mitsuba.core import load_dict

    s = load_dict({"type" : "disk"})
    assert s is not None
    assert s.primitive_count() == 1
    assert ek.allclose(s.surface_area(), ek.Pi)


def test02_bbox(variant_scalar_rgb):
    from mitsuba.core import load_dict, Vector3f, Transform4f

    sy = 2.5
    for sx in [1, 2, 4]:
        for translate in [Vector3f([1.3, -3.0, 5]),
                          Vector3f([-10000, 3.0, 31])]:
            s = load_dict({
                "type" : "disk",
                "to_world" : Transform4f.translate(translate) * Transform4f.scale((sx, sy, 1.0))
            })
            b = s.bbox()

            assert ek.allclose(s.surface_area(), sx * sy * ek.Pi)

            assert b.valid()
            assert ek.allclose(b.center(), translate)
            assert ek.allclose(b.min, translate - [sx, sy, 0.0])
            assert ek.allclose(b.max, translate + [sx, sy, 0.0])


def test03_ray_intersect(variant_scalar_rgb):
    from mitsuba.core import load_dict, Ray3f, Vector3f, Transform4f
    from mitsuba.render import RayFlags

    for r in [1, 3, 5]:
        for translate in [Vector3f([0.0, 0.0, 0.0]),
                          Vector3f([1.0, -5.0, 0.0])]:
            s = load_dict({
                "type" : "scene",
                "foo" : {
                    "type" : "disk",
                    "to_world" : Transform4f.translate(translate) * Transform4f.scale((r, r, 1.0))
                }
            })

            # grid size
            n = 10
            for x in ek.linspace(Float, -1, 1, n):
                for y in ek.linspace(Float, -1, 1, n):
                    x = 1.1 * r * (x - translate[0])
                    y = 1.1 * r * (y - translate[1])

                    ray = Ray3f(o=[x, y, -10], d=[0, 0, 1],
                                time=0.0, wavelengths=[])
                    si_found = s.ray_test(ray)

                    assert si_found == (x**2 + y**2 <= r*r)

                    if si_found:
                        ray = Ray3f(o=[x, y, -10], d=[0, 0, 1],
                                    time=0.0, wavelengths=[])

                        si = s.ray_intersect(ray, RayFlags.All | RayFlags.dNSdUV, True)
                        ray_u = Ray3f(ray)
                        ray_v = Ray3f(ray)
                        eps = 1e-4
                        ray_u.o += si.dp_du * eps
                        ray_v.o += si.dp_dv * eps
                        si_u = s.ray_intersect(ray_u)
                        si_v = s.ray_intersect(ray_v)

                        if si_u.is_valid():
                            dp_du = (si_u.p - si.p) / eps
                            dn_du = (si_u.n - si.n) / eps
                            assert ek.allclose(dp_du, si.dp_du, atol=2e-2)
                            assert ek.allclose(dn_du, si.dn_du, atol=2e-2)
                        if si_v.is_valid():
                            dp_dv = (si_v.p - si.p) / eps
                            dn_dv = (si_v.n - si.n) / eps
                            assert ek.allclose(dp_dv, si.dp_dv, atol=2e-2)
                            assert ek.allclose(dn_dv, si.dn_dv, atol=2e-2)


def test04_ray_intersect_vec(variant_scalar_rgb):
    from mitsuba.python.test.util import check_vectorization

    def kernel(o):
        from mitsuba.core import load_dict, ScalarTransform4f, Ray3f

        scene = load_dict({
            "type" : "scene",
            "foo" : {
                "type" : "disk",
                "to_world" : ScalarTransform4f.scale((2.0, 0.5, 1.0))
            }
        })

        o = 2.0 * o - 1.0
        o.z = 5.0

        t = scene.ray_intersect(Ray3f(o, [0, 0, -1])).t
        ek.eval(t)
        return t

    check_vectorization(kernel, arg_dims = [3], atol=1e-5)


def test05_differentiable_surface_interaction_ray_forward(variants_all_ad_rgb):
    from mitsuba.core import load_dict, Ray3f, Vector3f

    shape = load_dict({'type' : 'disk'})

    ray = Ray3f(Vector3f(0.1, -0.2, -10.0), Vector3f(0.0, 0.0, 1.0))
    pi = shape.ray_intersect_preliminary(ray)

    ek.enable_grad(ray.o)
    ek.enable_grad(ray.d)

    # If the ray origin is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    si.p *= 1.0
    ek.forward(ray.o.x)
    assert ek.allclose(ek.grad(si.p), [1, 0, 0])

    # If the ray origin is shifted along the y-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    si.p *= 1.0
    ek.forward(ray.o.y)
    assert ek.allclose(ek.grad(si.p), [0, 1, 0])

    # If the ray origin is shifted along the z-axis, so does si.t
    si = pi.compute_surface_interaction(ray)
    si.t *= 1.0
    ek.forward(ray.o.z)
    assert ek.allclose(ek.grad(si.t), -1)

    # If the ray direction is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    si.p *= 1.0
    ek.forward(ray.d.x)
    assert ek.allclose(ek.grad(si.p), [10, 0, 0])

    # If the ray origin is shifted toward the center of the disk, so does si.uv.x
    ray = Ray3f(Vector3f(0.9999999, 0.0, -10.0), Vector3f(0.0, 0.0, 1.0))
    ek.enable_grad(ray.o)
    si = shape.ray_intersect(ray)
    si.uv *= 1.0
    ek.forward(ray.o.x)
    assert ek.allclose(ek.grad(si.uv), [1, 0])

    # If the ray origin is shifted tangent to the disk, si.uv.y moves by 1 / (2pi)
    si = shape.ray_intersect(ray)
    si.uv *= 1.0
    ek.forward(ray.o.y)
    assert ek.allclose(ek.grad(si.uv), [0, 0.5 / ek.Pi], atol=1e-5)

    # If the ray origin is shifted tangent to the disk, si.dp_dv will also have a component is x
    si = shape.ray_intersect(ray)
    si.dp_dv *= 1.0
    ek.forward(ray.o.y)
    assert ek.allclose(ek.grad(si.dp_dv), [-1, 0, 0])


def test06_differentiable_surface_interaction_ray_backward(variants_all_ad_rgb):
    from mitsuba.core import load_dict, Ray3f, Vector3f

    shape = load_dict({'type' : 'disk'})

    ray = Ray3f(Vector3f(-0.3, -0.3, -10.0), Vector3f(0.0, 0.0, 1.0))
    pi = shape.ray_intersect_preliminary(ray)

    ek.enable_grad(ray.o)

    # If si.p is shifted along the x-axis, so does the ray origin
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.p.x)
    assert ek.allclose(ek.grad(ray.o), [1, 0, 0])

    # If si.t is changed, so does the ray origin along the z-axis
    ek.set_grad(ray.o, 0.0)
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.t)
    assert ek.allclose(ek.grad(ray.o), [0, 0, -1])
