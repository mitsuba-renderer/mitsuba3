import pytest
import drjit as dr
import mitsuba as mi

from drjit.scalar import ArrayXf as Float


def test01_create(variant_scalar_rgb):
    s = mi.load_dict({"type" : "disk"})
    assert s is not None
    assert s.primitive_count() == 1
    assert dr.allclose(s.surface_area(), dr.pi)


def test02_bbox(variant_scalar_rgb):
    sy = 2.5
    for sx in [1, 2, 4]:
        for translate in [mi.Vector3f([1.3, -3.0, 5]),
                          mi.Vector3f([-10000, 3.0, 31])]:
            s = mi.load_dict({
                "type" : "disk",
                "to_world" : mi.Transform4f().translate(translate) @ mi.Transform4f().scale((sx, sy, 1.0))
            })
            b = s.bbox()

            assert dr.allclose(s.surface_area(), sx * sy * dr.pi)

            assert b.valid()
            assert dr.allclose(b.center(), translate)
            assert dr.allclose(b.min, translate - [sx, sy, 0.0])
            assert dr.allclose(b.max, translate + [sx, sy, 0.0])


def test03_ray_intersect(variant_scalar_rgb):
    for r in [1, 3, 5]:
        for translate in [mi.Vector3f([0.0, 0.0, 0.0]),
                          mi.Vector3f([1.0, -5.0, 0.0])]:
            s = mi.load_dict({
                "type" : "scene",
                "foo" : {
                    "type" : "disk",
                    "to_world" : mi.Transform4f().translate(translate) @ mi.Transform4f().scale((r, r, 1.0))
                }
            })

            # grid size
            n = 10
            for x in dr.linspace(Float, -1, 1, n):
                for y in dr.linspace(Float, -1, 1, n):
                    x = 1.1 * r * (x - translate[0])
                    y = 1.1 * r * (y - translate[1])

                    ray = mi.Ray3f(o=[x, y, -10], d=[0, 0, 1],
                                time=0.0, wavelengths=[])
                    si_found = s.ray_test(ray)

                    assert si_found == (x**2 + y**2 <= r*r)

                    if si_found:
                        ray = mi.Ray3f(o=[x, y, -10], d=[0, 0, 1],
                                    time=0.0, wavelengths=[])

                        si = s.ray_intersect(ray, mi.RayFlags.All | mi.RayFlags.dNSdUV, True)
                        ray_u = mi.Ray3f(ray)
                        ray_v = mi.Ray3f(ray)
                        eps = 1e-4
                        ray_u.o += si.dp_du * eps
                        ray_v.o += si.dp_dv * eps
                        si_u = s.ray_intersect(ray_u)
                        si_v = s.ray_intersect(ray_v)

                        if si_u.is_valid():
                            dp_du = (si_u.p - si.p) / eps
                            dn_du = (si_u.n - si.n) / eps
                            assert dr.allclose(dp_du, si.dp_du, atol=2e-2)
                            assert dr.allclose(dn_du, si.dn_du, atol=2e-2)
                        if si_v.is_valid():
                            dp_dv = (si_v.p - si.p) / eps
                            dn_dv = (si_v.n - si.n) / eps
                            assert dr.allclose(dp_dv, si.dp_dv, atol=2e-2)
                            assert dr.allclose(dn_dv, si.dn_dv, atol=2e-2)


def test04_ray_intersect_vec(variant_scalar_rgb):
    from mitsuba.scalar_rgb.test.util import check_vectorization

    def kernel(o):
        scene = mi.load_dict({
            "type" : "scene",
            "foo" : {
                "type" : "disk",
                "to_world" : mi.ScalarTransform4f().scale((2.0, 0.5, 1.0))
            }
        })

        o = 2.0 * o - 1.0
        o.z = 5.0

        t = scene.ray_intersect(mi.Ray3f(o, [0, 0, -1])).t
        dr.eval(t)
        return t

    check_vectorization(kernel, arg_dims = [3], atol=1e-5)


def test05_differentiable_surface_interaction_ray_forward(variants_all_ad_rgb):
    shape = mi.load_dict({'type' : 'disk'})

    ray = mi.Ray3f(mi.Vector3f(0.1, -0.2, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
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

    # If the ray origin is shifted toward the center of the disk, so does si.uv.x
    ray = mi.Ray3f(mi.Vector3f(0.9999999, 0.0, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    dr.enable_grad(ray.o)
    si = shape.ray_intersect(ray)
    si.uv *= 1.0
    dr.forward(ray.o.x)
    assert dr.allclose(dr.grad(si.uv), [1, 0])

    # If the ray origin is shifted tangent to the disk, si.uv.y moves by 1 / (2pi)
    si = shape.ray_intersect(ray)
    si.uv *= 1.0
    dr.forward(ray.o.y)
    assert dr.allclose(dr.grad(si.uv), [0, 0.5 / dr.pi], atol=1e-5)

    # If the ray origin is shifted tangent to the disk, si.dp_dv will also have a component is x
    si = shape.ray_intersect(ray)
    si.dp_dv *= 1.0
    dr.forward(ray.o.y)
    assert dr.allclose(dr.grad(si.dp_dv), [-1, 0, 0])


def test06_differentiable_surface_interaction_ray_backward(variants_all_ad_rgb):
    shape = mi.load_dict({'type' : 'disk'})

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


def test07_differentiable_surface_interaction_ray_forward_follow_shape(variants_all_ad_rgb):
    shape = mi.load_dict({'type' : 'disk'})
    params = mi.traverse(shape)

    # Test 00: With DetachShape and no moving rays, the output shouldn't produce
    #          any gradients.

    ray = mi.Ray3f(mi.Vector3f(0.1, 0.1, -2), mi.Vector3f(0, 0, 1))

    theta = mi.Float(0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().scale(1 + theta)
    params.update()
    si = shape.ray_intersect(ray, mi.RayFlags.All | mi.RayFlags.DetachShape)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.t), 0.0)
    assert dr.allclose(dr.grad(si.p), 0.0)
    assert dr.allclose(dr.grad(si.n), 0.0)
    assert dr.allclose(dr.grad(si.uv), 0.0)

    # Test 01: When the disk is inflating, the point will not move along the
    #          ray. The normal isn't changing but the UVs are.

    ray = mi.Ray3f(mi.Vector3f(0.1, 0.1, -2), mi.Vector3f(0, 0, 1))

    theta = mi.Float(0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().scale(1 + theta)
    params.update()
    si = shape.ray_intersect(ray, mi.RayFlags.All)

    dr.forward(theta)

    r = dr.norm(mi.Point2f(0.1, 0.1))
    d_r = -r
    d_v = 0
    d_uv = mi.Vector2f(d_r, d_v)

    assert dr.allclose(dr.grad(si.t), 0)
    assert dr.allclose(dr.grad(si.p), 0)
    assert dr.allclose(dr.grad(si.n), 0)
    assert dr.allclose(dr.grad(si.uv), d_uv)

    # Test 02: With FollowShape, any intersection point with translating disk
    #          should move according to the translation. The normal and the
    #          UVs should be static.

    ray = mi.Ray3f(mi.Vector3f(0.1, 0.1, -2.0), mi.Vector3f(0.0, 0.0, 1.0))

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().translate([theta, 0.0, 0.0])
    params.update()
    si = shape.ray_intersect(ray, mi.RayFlags.All | mi.RayFlags.FollowShape)

    dr.forward(theta, dr.ADFlag.ClearNone)

    assert dr.allclose(dr.grad(si.p), [1.0, 0.0, 0.0])
    assert dr.allclose(dr.grad(si.n), 0.0)
    assert dr.allclose(dr.grad(si.uv), 0.0)

    # Test 03: With FollowShape, an off-center intersection point with a
    #          rotating disk and its normal should follow the rotation speed
    #          along the tangent direction. The UVs should be static.

    ray = mi.Ray3f(mi.Vector3f(0.1, 0.1, -2.0), mi.Vector3f(0.0, 0.0, 1.0))

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().rotate([0, 0, 1], 90 * theta)
    params.update()
    si = shape.ray_intersect(ray, mi.RayFlags.All | mi.RayFlags.FollowShape)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.p), [-dr.pi * 0.1 / 2, dr.pi * 0.1 / 2, 0.0])
    assert dr.allclose(dr.grad(si.n), 0.0)
    assert dr.allclose(dr.grad(si.uv), 0.0)

    # Test 04: Without FollowShape, a disk that is only rotating shouldn't
    #          produce any gradients for the intersection point and normal, but
    #          for the UVs.

    ray = mi.Ray3f(mi.Vector3f(0.1, 0.1, -2.0), mi.Vector3f(0.0, 0.0, 1.0))

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().rotate([0, 0, 1], 90 * theta)
    params.update()
    si = shape.ray_intersect(ray, mi.RayFlags.All)

    dr.forward(theta)

    v = dr.atan2(0.1, 0.1) * dr.inv_two_pi
    d_r = 0
    d_v = -2 * v
    d_uv = mi.Vector2f(d_r, d_v)

    assert dr.allclose(dr.grad(si.p), 0.0)
    assert dr.allclose(dr.grad(si.n), 0.0)
    assert dr.allclose(dr.grad(si.uv), d_uv)


def test08_eval_parameterization(variants_all_ad_rgb):
    shape = mi.load_dict({'type' : 'disk'})
    transform = mi.Transform4f().scale(0.2).translate([0.1, 0.2, 0.3]).rotate([1.0, 0, 0], 45)

    si_before = shape.eval_parameterization(mi.Point2f(0.3, 0.6))

    params = mi.traverse(shape)
    params['to_world'] = transform
    params.update()

    si_after = shape.eval_parameterization(mi.Point2f(0.3, 0.6))
    assert dr.allclose(si_before.uv, si_after.uv)


def test09_sample_silhouette_wrong_type(variants_all_rgb):
    disk = mi.load_dict({ 'type': 'disk' })
    ss = disk.sample_silhouette([0.1, 0.2, 0.3],
                                  mi.DiscontinuityFlags.InteriorType)

    assert ss.discontinuity_type == mi.DiscontinuityFlags.Empty.value


def test10_sample_silhouette_perimeter(variants_vec_rgb):
    disk = mi.load_dict({ 'type': 'disk' })
    disk_ptr = mi.ShapePtr(disk)

    x = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    y = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    z = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    samples = mi.Point3f(dr.meshgrid(x, y, z))

    ss = disk.sample_silhouette(samples, mi.DiscontinuityFlags.PerimeterType)
    assert dr.allclose(ss.discontinuity_type, mi.DiscontinuityFlags.PerimeterType.value)
    assert dr.all((ss.p.z == 0) | (ss.p.z == 1))
    assert dr.allclose(dr.norm(mi.Point2f(ss.p.x, ss.p.y)), 1)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert dr.allclose(ss.pdf, dr.inv_two_pi * dr.inv_four_pi, atol=1e-6)
    assert (dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, disk_ptr))


def test11_sample_silhouette_bijective(variants_vec_rgb):
    disk = mi.load_dict({ 'type': 'disk' })

    x = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    y = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    z = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    samples = mi.Point3f(dr.meshgrid(x, y, z))

    ss = disk.sample_silhouette(samples, mi.DiscontinuityFlags.PerimeterType)
    out = disk.invert_silhouette_sample(ss)

    assert dr.allclose(samples, out, atol=1e-6)


def test12_discontinuity_types(variants_vec_rgb):
    disk = mi.load_dict({ 'type': 'disk' })

    types = disk.silhouette_discontinuity_types()
    assert not mi.has_flag(types, mi.DiscontinuityFlags.InteriorType)
    assert mi.has_flag(types, mi.DiscontinuityFlags.PerimeterType)


def test13_differential_motion(variants_vec_rgb):
    if not dr.is_diff_v(mi.Float):
        pytest.skip("Only relevant in AD-enabled variants!")

    disk = mi.load_dict({ 'type': 'disk' })
    params = mi.traverse(disk)

    theta = mi.Point3f(0.0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().translate(
        [theta.x, 2 * theta.y, 3 * theta.z])
    params.update()

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.prim_index = 0
    si.p = mi.Point3f(1, 0, 0) # doesn't matter
    si.uv = mi.Point2f(0.5, 0.5)

    p_diff = disk.differential_motion(si)
    dr.forward(theta)
    v = dr.grad(p_diff)

    assert dr.allclose(p_diff, si.p)
    assert dr.allclose(v, [1.0, 2.0, 3.0])


def test14_primitive_silhouette_projection(variants_vec_rgb):
    disk = mi.load_dict({ 'type': 'disk' })
    disk_ptr = mi.ShapePtr(disk)

    u = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    v = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    uv = mi.Point2f(dr.meshgrid(u, v))
    si = disk.eval_parameterization(uv)

    viewpoint = mi.Point3f(0, 0, 5)

    ss = disk.primitive_silhouette_projection(viewpoint, si, mi.DiscontinuityFlags.PerimeterType, 0.)

    assert dr.allclose(ss.discontinuity_type, mi.DiscontinuityFlags.PerimeterType.value)
    assert dr.all(ss.p.z == 0)
    assert dr.allclose(dr.norm(mi.Point2f(ss.p.x, ss.p.y)), 1)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert (dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, disk_ptr))


def test15_precompute_silhouette(variants_vec_rgb):
    disk = mi.load_dict({ 'type': 'disk' })

    indices, weights = disk.precompute_silhouette(mi.ScalarPoint3f(0, 0, 3))

    assert len(weights) == 1
    assert indices[0] == mi.DiscontinuityFlags.PerimeterType.value
    assert weights[0] == 1


def test16_sample_precomputed_silhouette(variants_vec_rgb):
    disk = mi.load_dict({ 'type': 'disk' })
    disk_ptr = mi.ShapePtr(disk)

    samples = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    viewpoint = mi.ScalarPoint3f(0, 0, 5)

    ss = disk.sample_precomputed_silhouette(viewpoint, 0, samples)

    assert dr.allclose(ss.discontinuity_type, mi.DiscontinuityFlags.PerimeterType.value)
    assert dr.all((ss.p.z == 0) | (ss.p.z == 1))
    assert dr.allclose(dr.norm(mi.Point2f(ss.p.x, ss.p.y)), 1)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert dr.allclose(ss.pdf, dr.inv_two_pi, atol=1e-6)
    assert (dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, disk_ptr))


def test17_shape_type(variant_scalar_rgb):
    disk = mi.load_dict({ 'type': 'disk' })
    assert disk.shape_type() == mi.ShapeType.Disk.value;
