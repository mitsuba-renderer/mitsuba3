import pytest
import drjit as dr
import mitsuba as mi

from drjit.scalar import ArrayXf as Float


def test01_create(variant_scalar_rgb):
    s = mi.load_dict({"type" : "rectangle"})
    assert s is not None
    assert s.primitive_count() == 1
    assert dr.allclose(s.surface_area(), 4.0)


def test02_bbox(variant_scalar_rgb):
    sy = 2.5
    for sx in [1, 2, 4]:
        for translate in [mi.Vector3f([1.3, -3.0, 5]),
                          mi.Vector3f([-10000, 3.0, 31])]:
            s = mi.load_dict({
                "type" : "rectangle",
                "to_world" : mi.Transform4f.translate(translate) @ mi.Transform4f.scale((sx, sy, 1.0))
            })
            b = s.bbox()

            assert dr.allclose(s.surface_area(), sx * sy * 4)

            assert b.valid()
            assert dr.allclose(b.center(), translate)
            assert dr.allclose(b.min, translate - [sx, sy, 0.0])
            assert dr.allclose(b.max, translate + [sx, sy, 0.0])


def test03_ray_intersect(variant_scalar_rgb):
    # Scalar
    scene = mi.load_dict({
        "type" : "scene",
        "foo" : {
            "type" : "rectangle",
            "to_world" : mi.Transform4f.scale((2.0, 0.5, 1.0))
        }
    })

    n = 15
    coords = dr.linspace(Float, -1, 1, n)
    rays = [mi.Ray3f(o=[a, a, 5], d=[0, 0, -1], time=0.0,
                     wavelengths=[]) for a in coords]
    si_scalar = []
    valid_count = 0
    for i in range(n):
        its_found = scene.ray_test(rays[i])
        si = scene.ray_intersect(rays[i])

        assert its_found == (abs(coords[i]) <= 0.5)
        assert si.is_valid() == its_found
        si_scalar.append(si)
        valid_count += its_found

    assert valid_count == 7


def test04_ray_intersect_vec(variant_scalar_rgb):
    from mitsuba.scalar_rgb.test.util import check_vectorization

    def kernel(o):
        scene = mi.load_dict({
            "type" : "scene",
            "foo" : {
                "type" : "rectangle",
                "to_world" : mi.ScalarTransform4f.scale((2.0, 0.5, 1.0))
            }
        })

        o = 2.0 * o - 1.0
        o.z = 5.0

        t = scene.ray_intersect(mi.Ray3f(o, [0, 0, -1])).t
        dr.eval(t)
        return t

    check_vectorization(kernel, arg_dims = [3], atol=1e-5)


def test05_surface_area(variant_scalar_rgb):
    # Unifomly-scaled rectangle
    rect = mi.load_dict({
        "type" : "rectangle",
        "to_world" : mi.Transform4f([[2, 0, 0, 0],
                                  [0, 2, 0, 0],
                                  [0, 0, 1, 0],
                                  [0, 0, 0, 1]])
    })
    assert dr.allclose(rect.surface_area(), 2.0 * 2.0 * 2.0 * 2.0)

    # Rectangle sheared along the Z-axis
    rect = mi.load_dict({
        "type" : "rectangle",
        "to_world" : mi.Transform4f([[1, 0, 0, 0],
                                  [0, 1, 0, 0],
                                  [1, 0, 1, 0],
                                  [0, 0, 0, 1]])
    })
    assert dr.allclose(rect.surface_area(), 2.0 * 2.0 * dr.sqrt(2.0))

    # Rectangle sheared along the X-axis (shouldn't affect surface_area)
    rect = mi.load_dict({
        "type" : "rectangle",
        "to_world" : mi.Transform4f([[1, 1, 0, 0],
                                  [0, 1, 0, 0],
                                  [0, 0, 1, 0],
                                  [0, 0, 0, 1]])
    })
    assert dr.allclose(rect.surface_area(), 2.0 * 2.0)


def test06_differentiable_surface_interaction_ray_forward(variants_all_ad_rgb):
    shape = mi.load_dict({'type' : 'rectangle'})

    ray = mi.Ray3f(mi.Vector3f(-0.3, -0.3, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = shape.ray_intersect_preliminary(ray)

    dr.enable_grad(ray.o)
    dr.enable_grad(ray.d)

    # If the ray origin is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    si.p *= 1.0
    dr.forward(ray.o.x)
    assert dr.allclose(dr.grad(si.p), [1, 0, 0])

    # If the ray origin is shifted along the y-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    si.p *= 1.0
    dr.forward(ray.o.y)
    assert dr.allclose(dr.grad(si.p), [0, 1, 0])

    # If the ray origin is shifted along the x-axis, so does si.uv
    si = pi.compute_surface_interaction(ray)
    si.uv *= 1.0
    dr.forward(ray.o.x)
    assert dr.allclose(dr.grad(si.uv), [0.5, 0])

    # If the ray origin is shifted along the z-axis, so does si.t
    si = pi.compute_surface_interaction(ray)
    si.t *= 1.0
    dr.forward(ray.o.z)
    assert dr.allclose(dr.grad(si.t), -1)

    # If the ray direction is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    si.p *= 1.0
    dr.forward(ray.d.x)
    assert dr.allclose(dr.grad(si.p), [10, 0, 0])


def test07_differentiable_surface_interaction_ray_backward(variants_all_ad_rgb):
    shape = mi.load_dict({'type' : 'rectangle'})

    ray = mi.Ray3f(mi.Vector3f(-0.3, -0.3, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = shape.ray_intersect_preliminary(ray)

    dr.enable_grad(ray.o)

    # If si.p is shifted along the x-axis, so does the ray origin
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.p.x)
    assert dr.allclose(dr.grad(ray.o), [1, 0, 0])

    # If si.t is changed, so does the ray origin along the z-axis
    dr.set_grad(ray.o, 0.0)
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.t)
    assert dr.allclose(dr.grad(ray.o), [0, 0, -1])
