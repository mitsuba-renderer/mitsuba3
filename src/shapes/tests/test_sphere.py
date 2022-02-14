import mitsuba
import pytest
import drjit as dr
from drjit.scalar import ArrayXf as Float
from mitsuba.scalar_rgb.test.util import fresolver_append_path


def test01_create(variant_scalar_rgb):
    from mitsuba.core import load_dict, ScalarTransform4f

    s = load_dict({"type" : "sphere"})
    assert s is not None
    assert s.primitive_count() == 1
    assert dr.allclose(s.surface_area(), 4 * dr.Pi)

    # Test transforms order in constructor

    rot = ScalarTransform4f.rotate([1.0, 0.0, 0.0], 35)

    s1 = load_dict({
        "type" : "sphere",
        "radius" : 2.0,
        "center" : [1, 0, 0],
        "to_world" : rot
    })

    s2 = load_dict({
        "type" : "sphere",
        "to_world" : rot * ScalarTransform4f.translate([1, 0, 0]) * ScalarTransform4f.scale(2)
    })

    assert str(s1) == str(s2)


def test02_bbox(variant_scalar_rgb):
    from mitsuba.core import load_dict

    for r in [1, 2, 4]:
        s = load_dict({
            "type" : "sphere",
            "radius" : r
        })
        b = s.bbox()

        assert b.valid()
        assert dr.allclose(b.center(), [0, 0, 0])
        assert dr.all(b.min == -r)
        assert dr.all(b.max == r)
        assert dr.allclose(b.extents(), [2 * r] * 3)


def test03_ray_intersect_transform(variant_scalar_rgb):
    from mitsuba.core import load_dict, Ray3f, Transform4f

    for r in [1, 3]:
        s = load_dict({
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
                    or dr.abs(x_coord ** 2 + y_coord ** 2 - r * r) < 1e-8

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
                        assert dr.allclose(du, [1, 0], atol=2e-2)
                    if si_v.is_valid():
                        dv = (si_v.uv - si.uv) / eps
                        assert dr.allclose(dv, [0, 1], atol=2e-2)


def test04_ray_intersect_vec(variant_scalar_rgb):
    from mitsuba.python.test.util import check_vectorization

    def kernel(o):
        from mitsuba.core import load_dict, ScalarTransform4f, Ray3f

        scene = load_dict({
            "type" : "scene",
            "foo" : {
                "type" : "sphere",
                "to_world" : ScalarTransform4f.scale((0.5, 0.5, 0.5))
            }
        })

        o = 2.0 * o - 1.0
        o.z = 5.0

        t = scene.ray_intersect(Ray3f(o, [0, 0, -1])).t
        dr.eval(t)
        return t

    check_vectorization(kernel, arg_dims = [3], atol=1e-5)


def test05_sample_direct(variant_scalar_rgb):
    from mitsuba.core import load_dict, Ray3f
    from mitsuba.render import Interaction3f

    sphere = load_dict({"type" : "sphere"})

    def sample_cone(sample, cos_theta_max):
        cos_theta = (1 - sample[1]) + sample[1] * cos_theta_max
        sin_theta = dr.sqrt(1 - cos_theta * cos_theta)
        phi = 2 * dr.Pi * sample[0]
        s, c = dr.sin(phi), dr.cos(phi)
        return [c * sin_theta, s * sin_theta, cos_theta]

    it = dr.zero(Interaction3f)
    it.p = [0, 0, -3]
    it.t = 0
    sin_cone_angle = 1.0 / it.p[2]
    cos_cone_angle = dr.sqrt(1 - sin_cone_angle**2)

    for xi_1 in dr.linspace(Float, 0, 1, 10):
        for xi_2 in dr.linspace(Float, 1e-3, 1 - 1e-3, 10):
            sample = sphere.sample_direction(it, [xi_2, 1 - xi_1])
            d = sample_cone([xi_1, xi_2], cos_cone_angle)
            its = sphere.ray_intersect(Ray3f(it.p, d))
            assert dr.allclose(d, sample.d, atol=1e-5, rtol=1e-5)
            assert dr.allclose(its.t, sample.dist, atol=1e-5, rtol=1e-5)
            assert dr.allclose(its.p, sample.p, atol=1e-5, rtol=1e-5)


def test06_differentiable_surface_interaction_ray_forward(variants_all_ad_rgb):
    from mitsuba.core import load_dict, Ray3f, Vector3f

    shape = load_dict({'type' : 'sphere'})

    ray = Ray3f(Vector3f(0.0, -10.0, 0.0), Vector3f(0.0, 1.0, 0.0))
    pi = shape.ray_intersect_preliminary(ray)

    dr.enable_grad(ray.o)
    dr.enable_grad(ray.d)

    # If the ray origin is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    si.p *= 1.0
    dr.forward(ray.o.x)
    assert dr.allclose(dr.grad(si.p), [1, 0, 0])

    # If the ray origin is shifted along the z-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    si.p *= 1.0
    dr.forward(ray.o.z)
    assert dr.allclose(dr.grad(si.p), [0, 0, 1])

    # If the ray origin is shifted along the y-axis, so does si.t
    si = pi.compute_surface_interaction(ray)
    si.t *= 1.0
    dr.forward(ray.o.y)
    assert dr.allclose(dr.grad(si.t), -1)

    # If the ray direction is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    si.p *= 1.0
    dr.forward(ray.d.x)
    assert dr.allclose(dr.grad(si.p), [9, 0, 0])

    # If the ray origin is shifted tangent to the sphere (azimuth), so si.uv.x move by 1 / 2pi
    dr.enable_grad(ray.o)
    si = shape.ray_intersect(ray)
    si.uv *= 1.0
    dr.forward(ray.o.x)
    assert dr.allclose(dr.grad(si.uv), [1 / (2.0 * dr.Pi), 0])

    # If the ray origin is shifted tangent to the sphere (inclination), so si.uv.y move by 2 / 2pi
    dr.enable_grad(ray.o)
    si = shape.ray_intersect(ray)
    si.uv *= 1.0
    dr.forward(ray.o.z)
    assert dr.allclose(dr.grad(si.uv), [0, -2 / (2.0 * dr.Pi)])

    # # If the ray origin is shifted along the x-axis, so does si.n
    dr.enable_grad(ray.o)
    si = shape.ray_intersect(ray)
    si.n *= 1.0
    dr.forward(ray.o.x)
    assert dr.allclose(dr.grad(si.n), [1, 0, 0])

    # # If the ray origin is shifted along the z-axis, so does si.n
    dr.enable_grad(ray.o)
    si = shape.ray_intersect(ray)
    si.n *= 1.0
    dr.forward(ray.o.z)
    assert dr.allclose(dr.grad(si.n), [0, 0, 1])


def test07_differentiable_surface_interaction_ray_backward(variants_all_ad_rgb):
    from mitsuba.core import load_dict, Ray3f, Vector3f

    shape = load_dict({'type' : 'sphere'})

    ray = Ray3f(Vector3f(0.0, 0.0, -10.0), Vector3f(0.0, 0.0, 1.0))
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


def test08_si_singularity(variants_all_rgb):
    from mitsuba.core import load_dict, Ray3f

    scene = load_dict({"type" : "scene", 's': { 'type': 'sphere' }})
    ray = Ray3f([0, 0, -1], [0, 0, 1])

    si = scene.ray_intersect(ray)

    print(f"si: {si}")

    assert dr.allclose(si.dp_du, [0, 0, 0])
    assert dr.allclose(si.dp_dv, [dr.Pi, 0, 0])
    assert dr.allclose(si.sh_frame.s, [1, 0, 0])
    assert dr.allclose(si.sh_frame.t, [0, -1, 0])
    assert dr.allclose(si.sh_frame.n, [0, 0, -1])
