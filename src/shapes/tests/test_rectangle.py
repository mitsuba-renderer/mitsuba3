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
                "to_world" : mi.Transform4f().translate(translate) @ mi.Transform4f().scale((sx, sy, 1.0))
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
            "to_world" : mi.Transform4f().scale((2.0, 0.5, 1.0))
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
                "to_world" : mi.ScalarTransform4f().scale((2.0, 0.5, 1.0))
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


def test08_differentiable_surface_interaction_ray_forward_follow_shape(variants_all_ad_rgb):
    shape = mi.load_dict({'type' : 'rectangle'})
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

    # Test 01: When the rectangle is stretched, the point will not move along
    #          the ray. The normal isn't changing but the UVs are.

    ray = mi.Ray3f(mi.Vector3f(0.1, 0.2, -2), mi.Vector3f(0, 0, 1))

    theta = mi.Float(0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().scale([1 + theta, 1 + 2 * theta, 1])
    params.update()
    si = shape.ray_intersect(ray, mi.RayFlags.All)

    dr.forward(theta)

    d_uv = mi.Point2f(-0.1 / 2, -0.2)

    assert dr.allclose(dr.grad(si.t), 0)
    assert dr.allclose(dr.grad(si.p), 0)
    assert dr.allclose(dr.grad(si.n), 0)
    assert dr.allclose(dr.grad(si.uv), d_uv)

    # Test 02: With FollowShape, any intersection point with translating rectangle
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
    #          rotating rectangle and its normal should follow the rotation speed
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

    # Test 04: Without FollowShape, a rectangle that is only rotating shouldn't
    #          produce any gradients for the intersection point and normal, but
    #          for the UVs.
    ray = mi.Ray3f(mi.Vector3f(0.1, 0.1, -2.0), mi.Vector3f(0.0, 0.0, 1.0))

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().rotate([0, 0, 1], 90 * theta)
    params.update()
    si = shape.ray_intersect(ray, mi.RayFlags.All)

    dr.forward(theta)

    d_uv = [dr.pi * 0.1 / 4, -dr.pi * 0.1 / 4]

    assert dr.allclose(dr.grad(si.p), 0.0)
    assert dr.allclose(dr.grad(si.n), 0.0)
    assert dr.allclose(dr.grad(si.uv), d_uv)


def test09_eval_parameterization(variants_all_ad_rgb):
    shape = mi.load_dict({'type' : 'rectangle'})
    transform = mi.Transform4f().scale(0.2).translate([0.1, 0.2, 0.3]).rotate([1.0, 0, 0], 45)

    si_before = shape.eval_parameterization(mi.Point2f(0.3, 0.6))

    params = mi.traverse(shape)
    params['to_world'] = transform
    params.update()

    si_after = shape.eval_parameterization(mi.Point2f(0.3, 0.6))
    assert dr.allclose(si_before.uv, si_after.uv)


def test10_sample_silhouette_wrong_type(variants_all_rgb):
    rectangle = mi.load_dict({ 'type': 'rectangle' })
    ss = rectangle.sample_silhouette([0.1, 0.2, 0.3],
                                  mi.DiscontinuityFlags.InteriorType)

    assert ss.discontinuity_type == int(mi.DiscontinuityFlags.Empty)


def test11_sample_silhouette(variants_vec_rgb):
    rectangle = mi.load_dict({ 'type': 'rectangle' })
    rectangle_ptr = mi.ShapePtr(rectangle)

    x = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    y = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    z = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    samples = mi.Point3f(dr.meshgrid(x, y, z))

    ss = rectangle.sample_silhouette(samples, mi.DiscontinuityFlags.PerimeterType)
    assert dr.allclose(ss.discontinuity_type, int(mi.DiscontinuityFlags.PerimeterType))
    assert dr.all(ss.p.z == 0)
    assert dr.all(
        (ss.p.x == -1) | (ss.p.x == 1) |
        (ss.p.y == -1) | (ss.p.y == 1)
    )
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert dr.allclose(ss.pdf, (1 / (2 * 4)) * dr.inv_four_pi)

    assert (dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, rectangle_ptr))


def test12_sample_silhouette_bijective(variants_vec_rgb):
    rectangle = mi.load_dict({ 'type': 'rectangle' })

    x = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    y = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    z = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    samples = mi.Point3f(dr.meshgrid(x, y, z))

    ss = rectangle.sample_silhouette(samples, mi.DiscontinuityFlags.PerimeterType)
    out = rectangle.invert_silhouette_sample(ss)

    assert dr.allclose(samples, out, atol=1e-7)


def test13_discontinuity_types(variants_vec_rgb):
    rectangle = mi.load_dict({ 'type': 'rectangle' })

    types = rectangle.silhouette_discontinuity_types()
    assert not mi.has_flag(types, mi.DiscontinuityFlags.InteriorType)
    assert mi.has_flag(types, mi.DiscontinuityFlags.PerimeterType)


def test14_differential_motion(variants_vec_rgb):
    if not dr.is_diff_v(mi.Float):
        pytest.skip("Only relevant in AD-enabled variants!")

    rectangle = mi.load_dict({ 'type': 'rectangle' })
    params = mi.traverse(rectangle)

    theta = mi.Point3f(0.0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().translate(
        [theta.x, 2 * theta.y, 3 * theta.z])
    params.update()

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.prim_index = 0
    si.p = mi.Point3f(1, 0, 0) # doesn't matter
    si.uv = mi.Point2f(0.5, 0.5)

    p_diff = rectangle.differential_motion(si)
    dr.forward(theta)
    v = dr.grad(p_diff)

    assert dr.allclose(p_diff, si.p)
    assert dr.allclose(v, [1.0, 2.0, 3.0])


def test15_primitive_silhouette_projection(variants_vec_rgb):
    rectangle = mi.load_dict({ 'type': 'rectangle' })
    rectangle_ptr = mi.ShapePtr(rectangle)

    u = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    v = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    uv = mi.Point2f(dr.meshgrid(u, v))
    si = rectangle.eval_parameterization(uv)

    viewpoint = mi.Point3f(0, 0, 5)

    sample = dr.linspace(mi.Float, 1e-6, 1-1e-6, dr.width(uv))
    ss = rectangle.primitive_silhouette_projection(
        viewpoint, si, mi.DiscontinuityFlags.PerimeterType, sample)

    assert dr.allclose(ss.discontinuity_type, int(mi.DiscontinuityFlags.PerimeterType))
    assert dr.all((ss.p.z == 0))
    assert dr.all(
        (ss.p.x == -1) | (ss.p.x == 1) |
        (ss.p.y == -1) | (ss.p.y == 1)
    )
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert (dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, rectangle_ptr))


def test16_precompute_silhouette(variants_vec_rgb):
    rectangle = mi.load_dict({ 'type': 'rectangle' })

    indices, weights = rectangle.precompute_silhouette(mi.ScalarPoint3f(0, 0, 3))

    assert len(weights) == 1
    assert indices[0] == int(mi.DiscontinuityFlags.PerimeterType)
    assert weights[0] == 1


def test17_sample_precomputed_silhouette(variants_vec_rgb):
    rectangle = mi.load_dict({ 'type': 'rectangle' })
    rectangle_ptr = mi.ShapePtr(rectangle)

    samples = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    viewpoint = mi.ScalarPoint3f(0, 0, 5)

    ss = rectangle.sample_precomputed_silhouette(viewpoint, 0, samples)

    assert dr.allclose(ss.discontinuity_type, int(mi.DiscontinuityFlags.PerimeterType))
    assert dr.all((ss.p.z == 0))
    assert dr.all(
        (ss.p.x == -1) | (ss.p.x == 1) |
        (ss.p.y == -1) | (ss.p.y == 1)
    )
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert dr.allclose(ss.pdf, (1 / (2 * 4)))
    assert (dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, rectangle_ptr))


def test18_shape_type(variant_scalar_rgb):
    rectangle = mi.load_dict({ 'type': 'rectangle' })
    assert rectangle.shape_type() == int(mi.ShapeType.Rectangle)
