import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float
from mitsuba.python.test.util import fresolver_append_path


def test01_create(variant_scalar_rgb):
    from mitsuba.core import xml, ScalarTransform4f

    s = xml.load_dict({"type" : "sphere"})
    assert s is not None
    assert s.primitive_count() == 1
    assert ek.allclose(s.surface_area(), 4 * ek.pi)

    # Test transforms order in constructor

    rot = ScalarTransform4f.rotate([1.0, 0.0, 0.0], 35)

    s1 = xml.load_dict({
        "type" : "sphere",
        "radius" : 2.0,
        "center" : [1, 0, 0],
        "to_world" : rot
    })

    s2 = xml.load_dict({
        "type" : "sphere",
        "to_world" : rot * ScalarTransform4f.translate([1, 0, 0]) * ScalarTransform4f.scale(2)
    })

    assert str(s1) == str(s2)


def test02_bbox(variant_scalar_rgb):
    from mitsuba.core import xml

    for r in [1, 2, 4]:
        s = xml.load_dict({
            "type" : "sphere",
            "radius" : r
        })
        b = s.bbox()

        assert b.valid()
        assert ek.allclose(b.center(), [0, 0, 0])
        assert ek.all(b.min == -r)
        assert ek.all(b.max == r)
        assert ek.allclose(b.extents(), [2 * r] * 3)


def test03_ray_intersect_transform(variant_scalar_rgb):
    from mitsuba.core import xml, Ray3f, Transform4f

    for r in [1, 3]:
        s = xml.load_dict({
            "type" : "sphere",
            "radius" : r,
            "to_world": Transform4f.translate([0, 1, 0]) * Transform4f.rotate([0, 1, 0], 30.0)
        })

        # grid size
        n = 21
        inv_n = 1.0 / n

        for x in range(n):
            for y in range(n):
                x_coord = r * (2 * (x * inv_n) - 1)
                y_coord = r * (2 * (y * inv_n) - 1)

                ray = Ray3f(o=[x_coord, y_coord + 1, -8], d=[0.0, 0.0, 1.0],
                            time=0.0, wavelengths=[])
                si_found = s.ray_test(ray)

                assert si_found == (x_coord ** 2 + y_coord ** 2 <= r * r) \
                    or ek.abs(x_coord ** 2 + y_coord ** 2 - r * r) < 1e-8

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


def test04_sample_direct(variant_scalar_rgb):
    from mitsuba.core import xml, Ray3f
    from mitsuba.render import Interaction3f

    sphere = xml.load_dict({"type" : "sphere"})

    def sample_cone(sample, cos_theta_max):
        cos_theta = (1 - sample[1]) + sample[1] * cos_theta_max
        sin_theta = ek.sqrt(1 - cos_theta * cos_theta)
        phi = 2 * ek.pi * sample[0]
        s, c = ek.sin(phi), ek.cos(phi)
        return [c * sin_theta, s * sin_theta, cos_theta]

    it = Interaction3f.zero()
    it.p = [0, 0, -3]
    it.t = 0
    sin_cone_angle = 1.0 / it.p[2]
    cos_cone_angle = ek.sqrt(1 - sin_cone_angle**2)

    for xi_1 in ek.linspace(Float, 0, 1, 10):
        for xi_2 in ek.linspace(Float, 1e-3, 1 - 1e-3, 10):
            sample = sphere.sample_direction(it, [xi_2, 1 - xi_1])
            d = sample_cone([xi_1, xi_2], cos_cone_angle)
            its = sphere.ray_intersect(Ray3f(it.p, d, 0, []))
            assert ek.allclose(d, sample.d, atol=1e-5, rtol=1e-5)
            assert ek.allclose(its.t, sample.dist, atol=1e-5, rtol=1e-5)
            assert ek.allclose(its.p, sample.p, atol=1e-5, rtol=1e-5)



def test05_differentiable_surface_interaction_ray_forward(variant_gpu_autodiff_rgb):
    from mitsuba.core import xml, Ray3f, Vector3f, UInt32

    shape = xml.load_dict({'type' : 'sphere'})

    ray = Ray3f(Vector3f(0.0, -10.0, 0.0), Vector3f(0.0, 1.0, 0.0), 0, [])
    pi = shape.ray_intersect_preliminary(ray)

    ek.set_requires_gradient(ray.o)
    ek.set_requires_gradient(ray.d)

    # If the ray origin is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    ek.forward(ray.o.x)
    assert ek.allclose(ek.gradient(si.p), [1, 0, 0])

    # If the ray origin is shifted along the z-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    ek.forward(ray.o.z)
    assert ek.allclose(ek.gradient(si.p), [0, 0, 1])

    # If the ray origin is shifted along the y-axis, so does si.t
    si = pi.compute_surface_interaction(ray)
    ek.forward(ray.o.y)
    assert ek.allclose(ek.gradient(si.t), -1)

    # If the ray direction is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    ek.forward(ray.d.x)
    assert ek.allclose(ek.gradient(si.p), [9, 0, 0])

    # If the ray origin is shifted tangent to the sphere (azimuth), so si.uv.x move by 1 / 2pi
    ek.set_requires_gradient(ray.o)
    si = shape.ray_intersect(ray)
    ek.forward(ray.o.x)
    assert ek.allclose(ek.gradient(si.uv), [1 / (2.0 * ek.pi), 0])

    # If the ray origin is shifted tangent to the sphere (inclination), so si.uv.y move by 2 / 2pi
    ek.set_requires_gradient(ray.o)
    si = shape.ray_intersect(ray)
    ek.forward(ray.o.z)
    assert ek.allclose(ek.gradient(si.uv), [0, -2 / (2.0 * ek.pi)])

    # # If the ray origin is shifted along the x-axis, so does si.n
    ek.set_requires_gradient(ray.o)
    si = shape.ray_intersect(ray)
    ek.forward(ray.o.x)
    assert ek.allclose(ek.gradient(si.n), [1, 0, 0])

    # # If the ray origin is shifted along the z-axis, so does si.n
    ek.set_requires_gradient(ray.o)
    si = shape.ray_intersect(ray)
    ek.forward(ray.o.z)
    assert ek.allclose(ek.gradient(si.n), [0, 0, 1])


def test06_differentiable_surface_interaction_ray_backward(variant_gpu_autodiff_rgb):
    from mitsuba.core import xml, Ray3f, Vector3f, UInt32

    shape = xml.load_dict({'type' : 'sphere'})

    ray = Ray3f(Vector3f(0.0, 0.0, -10.0), Vector3f(0.0, 0.0, 1.0), 0, [])
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
