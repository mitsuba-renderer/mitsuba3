import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float


def test01_create(variant_scalar_rgb):
    from mitsuba.core import xml

    s = xml.load_dict({"type" : "rectangle"})
    assert s is not None
    assert s.primitive_count() == 1
    assert ek.allclose(s.surface_area(), 4.0)


def test02_bbox(variant_scalar_rgb):
    from mitsuba.core import xml, Vector3f, Transform4f

    sy = 2.5
    for sx in [1, 2, 4]:
        for translate in [Vector3f([1.3, -3.0, 5]),
                          Vector3f([-10000, 3.0, 31])]:
            s = xml.load_dict({
                "type" : "rectangle",
                "to_world" : Transform4f.translate(translate) * Transform4f.scale((sx, sy, 1.0))
            })
            b = s.bbox()

            assert ek.allclose(s.surface_area(), sx * sy * 4)

            assert b.valid()
            assert ek.allclose(b.center(), translate)
            assert ek.allclose(b.min, translate - [sx, sy, 0.0])
            assert ek.allclose(b.max, translate + [sx, sy, 0.0])


def test03_ray_intersect(variant_scalar_rgb):
    from mitsuba.core import xml, Ray3f, Transform4f

    # Scalar
    scene = xml.load_dict({
        "type" : "scene",
        "foo" : {
            "type" : "rectangle",
            "to_world" : Transform4f.scale((2.0, 0.5, 1.0))
        }
    })

    n = 15
    coords = ek.linspace(Float, -1, 1, n)
    rays = [Ray3f(o=[a, a, 5], d=[0, 0, -1], time=0.0,
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

    try:
        mitsuba.set_variant('packet_rgb')
        from mitsuba.core import xml, Ray3f as Ray3fX
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    # Packet
    scene_p = xml.load_dict({
        "type" : "scene",
        "foo" : {
            "type" : "rectangle",
            "to_world" : Transform4f.scale((2.0, 0.5, 1.0))
        }
    })

    packet = Ray3fX.zero(n)
    for i in range(n):
        packet[i] = rays[i]

    si_p = scene_p.ray_intersect(packet)
    its_found_p = scene_p.ray_test(packet)

    assert ek.all(si_p.is_valid() == its_found_p)

    for i in range(n):
        assert ek.allclose(si_p.t[i], si_scalar[i].t)


def test04_surface_area(variant_scalar_rgb):
    from mitsuba.core import xml, Transform4f

    # Unifomly-scaled rectangle
    rect = xml.load_dict({
        "type" : "rectangle",
        "to_world" : Transform4f([[2, 0, 0, 0],
                                  [0, 2, 0, 0],
                                  [0, 0, 1, 0],
                                  [0, 0, 0, 1]])
    })
    assert ek.allclose(rect.surface_area(), 2.0 * 2.0 * 2.0 * 2.0)

    # Rectangle sheared along the Z-axis
    rect = xml.load_dict({
        "type" : "rectangle",
        "to_world" : Transform4f([[1, 0, 0, 0],
                                  [0, 1, 0, 0],
                                  [1, 0, 1, 0],
                                  [0, 0, 0, 1]])
    })
    assert ek.allclose(rect.surface_area(), 2.0 * 2.0 * ek.sqrt(2.0))

    # Rectangle sheared along the X-axis (shouldn't affect surface_area)
    rect = xml.load_dict({
        "type" : "rectangle",
        "to_world" : Transform4f([[1, 1, 0, 0],
                                  [0, 1, 0, 0],
                                  [0, 0, 1, 0],
                                  [0, 0, 0, 1]])
    })
    assert ek.allclose(rect.surface_area(), 2.0 * 2.0)


def test05_differentiable_surface_interaction_ray_forward(variant_gpu_autodiff_rgb):
    from mitsuba.core import xml, Ray3f, Vector3f, UInt32

    shape = xml.load_dict({'type' : 'rectangle'})

    ray = Ray3f(Vector3f(-0.3, -0.3, -10.0), Vector3f(0.0, 0.0, 1.0), 0, [])
    pi = shape.ray_intersect_preliminary(ray)

    ek.set_requires_gradient(ray.o)
    ek.set_requires_gradient(ray.d)

    # If the ray origin is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    ek.forward(ray.o.x)
    assert ek.allclose(ek.gradient(si.p), [1, 0, 0])

    # If the ray origin is shifted along the y-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    ek.forward(ray.o.y)
    assert ek.allclose(ek.gradient(si.p), [0, 1, 0])

    # If the ray origin is shifted along the x-axis, so does si.uv
    si = pi.compute_surface_interaction(ray)
    ek.forward(ray.o.x)
    assert ek.allclose(ek.gradient(si.uv), [0.5, 0])

    # If the ray origin is shifted along the z-axis, so does si.t
    si = pi.compute_surface_interaction(ray)
    ek.forward(ray.o.z)
    assert ek.allclose(ek.gradient(si.t), -1)

    # If the ray direction is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    ek.forward(ray.d.x)
    assert ek.allclose(ek.gradient(si.p), [10, 0, 0])


def test06_differentiable_surface_interaction_ray_backward(variant_gpu_autodiff_rgb):
    from mitsuba.core import xml, Ray3f, Vector3f, UInt32

    shape = xml.load_dict({'type' : 'rectangle'})

    ray = Ray3f(Vector3f(-0.3, -0.3, -10.0), Vector3f(0.0, 0.0, 1.0), 0, [])
    pi = shape.ray_intersect_preliminary(ray)

    ek.set_requires_gradient(ray.o)

    # If si.p is shifted along the x-axis, so does the ray origin
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.p.x)
    assert ek.allclose(ek.gradient(ray.o), [1, 0, 0])

    # If si.t is changed, so does the ray origin along the z-axis
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.t)
    assert ek.allclose(ek.gradient(ray.o), [0, 0, -1])