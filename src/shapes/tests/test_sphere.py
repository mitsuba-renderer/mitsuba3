import pytest
import drjit as dr
import mitsuba as mi

from drjit.scalar import ArrayXf as Float
from mitsuba.scalar_rgb.test.util import fresolver_append_path


def test01_create(variant_scalar_rgb):
    s = mi.load_dict({"type" : "sphere"})
    assert s is not None
    assert s.primitive_count() == 1
    assert dr.allclose(s.surface_area(), 4 * dr.pi)

    T = mi.ScalarTransform4f

    # Test transforms order in constructor

    rot = T().rotate([1.0, 0.0, 0.0], 35)

    s1 = mi.load_dict({
        "type" : "sphere",
        "radius" : 2.0,
        "center" : [1, 0, 0],
        "to_world" : rot
    })

    s2 = mi.load_dict({
        "type" : "sphere",
        "to_world" : rot @ T().translate([1, 0, 0]) @ T().scale(2)
    })

    assert str(s1) == str(s2)


def test02_bbox(variant_scalar_rgb):
    for r in [1, 2, 4]:
        s = mi.load_dict({
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
    for r in [1, 3]:
        s = mi.load_dict({
            "type" : "sphere",
            "radius" : r,
            "to_world": mi.Transform4f().translate([0, 1, 0]) @ mi.Transform4f().rotate([0, 1, 0], 30.0)
        })

        # grid size
        n = 21
        inv_n = 1.0 / n

        for x in range(n):
            for y in range(n):
                x_coord = r * (2 * (x * inv_n) - 1)
                y_coord = r * (2 * (y * inv_n) - 1)

                ray = mi.Ray3f(o=[x_coord, y_coord + 1, -8], d=[0.0, 0.0, 1.0],
                               time=0.0, wavelengths=[])
                si_found = s.ray_test(ray)

                assert si_found == (x_coord ** 2 + y_coord ** 2 <= r * r) \
                    or dr.abs(x_coord ** 2 + y_coord ** 2 - r * r) < 1e-8

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


def test04_ray_intersect_vec(variant_scalar_rgb):
    from mitsuba.scalar_rgb.test.util import check_vectorization

    def kernel(o):
        scene = mi.load_dict({
            "type" : "scene",
            "foo" : {
                "type" : "sphere",
                "to_world" : mi.ScalarTransform4f().scale((0.5, 0.5, 0.5))
            }
        })

        o = 2.0 * o - 1.0
        o.z = 5.0

        t = scene.ray_intersect(mi.Ray3f(o, [0, 0, -1])).t
        dr.eval(t)
        return t

    check_vectorization(kernel, arg_dims = [3], atol=1e-5)


def test05_sample_direct(variant_scalar_rgb):
    sphere = mi.load_dict({"type" : "sphere"})

    def sample_cone(sample, cos_theta_max):
        cos_theta = (1 - sample[1]) + sample[1] * cos_theta_max
        sin_theta = dr.sqrt(1 - cos_theta * cos_theta)
        phi = 2 * dr.pi * sample[0]
        s, c = dr.sin(phi), dr.cos(phi)
        return [c * sin_theta, s * sin_theta, cos_theta]

    it = dr.zeros(mi.Interaction3f)
    it.p = [0, 0, -3]
    it.t = 0
    sin_cone_angle = 1.0 / it.p[2]
    cos_cone_angle = dr.sqrt(1 - sin_cone_angle**2)

    for xi_1 in dr.linspace(Float, 0, 1, 10):
        for xi_2 in dr.linspace(Float, 1e-3, 1 - 1e-3, 10):
            sample = sphere.sample_direction(it, [xi_2, 1 - xi_1])
            d = sample_cone([xi_1, xi_2], cos_cone_angle)
            si = sphere.ray_intersect(mi.Ray3f(it.p, d))
            assert dr.allclose(d, sample.d, atol=1e-5, rtol=1e-5)
            assert dr.allclose(si.t, sample.dist, atol=1e-5, rtol=1e-5)
            assert dr.allclose(si.p, sample.p, atol=1e-5, rtol=1e-5)


def test06_differentiable_surface_interaction_ray_forward(variants_all_ad_rgb):
    shape = mi.load_dict({'type' : 'sphere'})

    ray = mi.Ray3f(mi.Vector3f(0.0, -10.0, 0.0), mi.Vector3f(0.0, 1.0, 0.0))
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
    assert dr.allclose(dr.grad(si.uv), [1 / (2.0 * dr.pi), 0])

    # If the ray origin is shifted tangent to the sphere (inclination), so si.uv.y move by 2 / 2pi
    dr.enable_grad(ray.o)
    si = shape.ray_intersect(ray)
    si.uv *= 1.0
    dr.forward(ray.o.z)
    assert dr.allclose(dr.grad(si.uv), [0, -2 / (2.0 * dr.pi)])

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
    shape = mi.load_dict({'type' : 'sphere'})

    ray = mi.Ray3f(mi.Vector3f(0.0, 0.0, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
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


def test08_differentiable_surface_interaction_ray_forward_follow_shape(variants_all_ad_rgb):
    shape = mi.load_dict({'type' : 'sphere'})
    params = mi.traverse(shape)

    # Test 00: With DetachShape and no moving rays, the output shouldn't produce
    #          any gradients.

    ray = mi.Ray3f(mi.Vector3f(0, 0, -2), mi.Vector3f(0, 0, 1))

    theta = mi.Float(0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().scale(1 + theta)
    params.update()
    si = shape.ray_intersect(ray, mi.RayFlags.All | mi.RayFlags.DetachShape)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.p), 0.0)
    assert dr.allclose(dr.grad(si.t), 0.0)
    assert dr.allclose(dr.grad(si.n), 0.0)
    assert dr.allclose(dr.grad(si.uv), 0.0)

    # Test 01: When the sphere is inflating, the point hitting the center will
    #          move back along the ray. The normal isn't changing nor the UVs.

    ray = mi.Ray3f(mi.Vector3f(0, 0, -2), mi.Vector3f(0, 0, 1))

    theta = mi.Float(0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().scale(1 + theta)
    params.update()
    si = shape.ray_intersect(ray, mi.RayFlags.All)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.t), -1)
    assert dr.allclose(dr.grad(si.p), [0, 0, -1])
    assert dr.allclose(dr.grad(si.n), 0)
    assert dr.allclose(dr.grad(si.uv), 0)

    # Test 02: With FollowShape, an intersection point at the pole of a translating
    #          sphere should move with the pole. The normal and the UVs should be static.

    ray = mi.Ray3f(mi.Vector3f(0.0, 0.0, -2.0), mi.Vector3f(0.0, 0.0, 1.0))

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().translate([theta, 0.0, 0.0])
    params.update()
    si = shape.ray_intersect(ray, mi.RayFlags.All | mi.RayFlags.FollowShape)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.p), [1.0, 0.0, 0.0])
    assert dr.allclose(dr.grad(si.n), 0.0)
    assert dr.allclose(dr.grad(si.uv), 0.0)

    # Test 03: With FollowShape, an intersection point and normal at the pole of
    #          a rotating sphere should follow the rotation speed along the
    #          tangent direction. The UVs should be static.

    ray = mi.Ray3f(mi.Vector3f(0.0, 0.0, -2.0), mi.Vector3f(0.0, 0.0, 1.0))

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().rotate([0, 1, 0], 90 * theta)
    params.update()
    si = shape.ray_intersect(ray, mi.RayFlags.All | mi.RayFlags.FollowShape)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.p), [-dr.pi / 2.0, 0.0, 0.0])
    assert dr.allclose(dr.grad(si.n), [-dr.pi / 2.0, 0.0, 0.0])
    assert dr.allclose(dr.grad(si.uv), 0.0)

    # Test 04: Without FollowShape, a sphere that is only rotating shouldn't
    #          produce any gradients for the intersection point and normal, but
    #          for the UVs.

    ray = mi.Ray3f(mi.Vector3f(0.0, -2.0, 0.0), mi.Vector3f(0.0, 1.0, 0.0))

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().rotate([1, 0, 0], 90 * theta)
    params.update()
    si = shape.ray_intersect(ray, mi.RayFlags.All)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.p), 0.0)
    assert dr.allclose(dr.grad(si.n), 0.0)
    assert dr.allclose(dr.grad(si.uv), [0.0, -0.5])


def test09_si_singularity(variants_all_rgb):
    scene = mi.load_dict({"type" : "scene", 's': { 'type': 'sphere' }})
    ray = mi.Ray3f([0, 0, -1], [0, 0, 1])

    si = scene.ray_intersect(ray)

    assert dr.allclose(si.dp_du, [0, 0, 0])
    assert dr.allclose(si.dp_dv, [dr.pi, 0, 0])
    assert dr.allclose(si.sh_frame.s, [1, 0, 0])
    assert dr.allclose(si.sh_frame.t, [0, -1, 0])
    assert dr.allclose(si.sh_frame.n, [0, 0, -1])


def test10_si_singularity_centered(variants_all_rgb):
    scene = mi.load_dict({"type" : "scene", 's': { 'type': 'sphere' }})
    ray = mi.Ray3f([0, 0, 0], [0, 0, 1])

    si = scene.ray_intersect(ray)

    assert dr.allclose(si.dp_du, [0, 0, 0])
    assert dr.allclose(si.dp_dv, [dr.pi, 0, 0])
    assert dr.allclose(si.sh_frame.s, [1, 0, 0])
    assert dr.allclose(si.sh_frame.t, [0, 1, 0])
    assert dr.allclose(si.sh_frame.n, [0, 0, 1])


def test11_sample_silhouette_wrong_type(variants_all_rgb):
    sphere = mi.load_dict({ 'type': 'sphere' })
    ss = sphere.sample_silhouette([0.1, 0.2, 0.3],
                                  mi.DiscontinuityFlags.PerimeterType)

    assert ss.discontinuity_type == mi.DiscontinuityFlags.Empty.value


def test12_sample_silhouette(variants_vec_rgb):
    sphere = mi.load_dict({ 'type': 'sphere' })
    sphere_ptr = mi.ShapePtr(sphere)

    x = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    y = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    z = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    samples = mi.Point3f(dr.meshgrid(x, y, z))

    ss = sphere.sample_silhouette(samples, mi.DiscontinuityFlags.InteriorType)
    assert dr.allclose(ss.discontinuity_type, mi.DiscontinuityFlags.InteriorType.value)
    assert dr.allclose(dr.norm(ss.p), 1)
    assert dr.allclose(ss.p, ss.n)
    assert dr.allclose(ss.pdf, dr.inv_four_pi * dr.inv_two_pi)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert (dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, sphere_ptr))
    assert dr.allclose(ss.foreshortening, 1)


def test13_sample_silhouette_bijective(variants_vec_rgb):
    sphere = mi.load_dict({ 'type': 'sphere' })

    x = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    y = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    z = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    samples = mi.Point3f(dr.meshgrid(x, y, z))

    ss = sphere.sample_silhouette(samples, mi.DiscontinuityFlags.InteriorType)
    out = sphere.invert_silhouette_sample(ss)

    assert dr.allclose(samples, out, atol=1e-7)


def test14_discontinuity_types(variants_vec_rgb):
    sphere = mi.load_dict({ 'type': 'sphere' })

    types = sphere.silhouette_discontinuity_types()
    assert mi.has_flag(types, mi.DiscontinuityFlags.InteriorType)
    assert not mi.has_flag(types, mi.DiscontinuityFlags.PerimeterType)


def test15_differential_motion(variants_vec_rgb):
    if not dr.is_diff_v(mi.Float):
        pytest.skip("Only relevant in AD-enabled variants!")

    sphere = mi.load_dict({ 'type': 'sphere' })
    params = mi.traverse(sphere)

    theta = mi.Point3f(0.0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().translate(
        [theta.x, 2 * theta.y, 3 * theta.z])
    params.update()

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.prim_index = 0
    si.p = mi.Point3f(1, 0, 0) # doesn't matter
    si.uv = mi.Point2f(0.5, 0.5)

    p_diff = sphere.differential_motion(si)
    dr.forward(theta)
    v = dr.grad(p_diff)

    assert dr.allclose(p_diff, si.p)
    assert dr.allclose(v, [1.0, 2.0, 3.0])


def test16_primitive_silhouette_projection(variants_vec_rgb):
    sphere = mi.load_dict({ 'type': 'sphere' })
    sphere_ptr = mi.ShapePtr(sphere)

    u = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    v = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    uv = mi.Point2f(dr.meshgrid(u, v))
    si = sphere.eval_parameterization(uv)

    viewpoint = mi.Point3f(0, 0, 5)

    ss = sphere.primitive_silhouette_projection(
        viewpoint, si, mi.DiscontinuityFlags.InteriorType, 0.)

    assert dr.allclose(ss.discontinuity_type, mi.DiscontinuityFlags.InteriorType.value)
    assert dr.allclose(dr.norm(ss.p), 1)
    assert dr.allclose(ss.p, ss.n)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert (dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, sphere_ptr))


def test17_precompute_silhouette(variants_vec_rgb):
    sphere = mi.load_dict({ 'type': 'sphere' })

    indices, weights = sphere.precompute_silhouette(mi.ScalarPoint3f(0, 0, 3))

    assert len(weights) == 1
    assert indices[0] == mi.DiscontinuityFlags.InteriorType.value
    assert weights[0] == 1


def test18_sample_precomputed_silhouette(variants_vec_rgb):
    sphere = mi.load_dict({ 'type': 'sphere' })
    sphere_ptr = mi.ShapePtr(sphere)

    samples = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    viewpoint = mi.ScalarPoint3f(0, 0, 5)

    ss = sphere.sample_precomputed_silhouette(viewpoint, 0, samples)

    assert dr.allclose(ss.discontinuity_type, mi.DiscontinuityFlags.InteriorType.value)
    assert dr.allclose(dr.norm(ss.p), 1)
    assert dr.allclose(ss.p, ss.n)
    ring_radius = 1 / 5 * dr.norm(ss.p - viewpoint)
    assert dr.allclose(ss.pdf, 1 / (dr.two_pi * ring_radius))
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert (dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, sphere_ptr))


def test19_shape_type(variant_scalar_rgb):
    sphere = mi.load_dict({ 'type': 'sphere' })
    assert sphere.shape_type() == mi.ShapeType.Sphere.value;
