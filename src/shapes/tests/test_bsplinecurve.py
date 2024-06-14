import pytest
import drjit as dr
import mitsuba as mi

from drjit.scalar import ArrayXf as Float
from mitsuba.scalar_rgb.test.util import fresolver_append_path

@fresolver_append_path
def test01_create(variants_all_rgb):
    s = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve.txt",
    })
    assert s is not None
    assert s.primitive_count() == 1


@fresolver_append_path
def test02_create_multiple_curves(variants_all_rgb):
    s = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve_6.txt",
    })
    assert s is not None
    assert s.primitive_count() == 6


@fresolver_append_path
def test03_bbox(variants_all_rgb):
    for sx in [1, 2, 4]:
        for translate in [mi.ScalarVector3f([1.3, -3.0, 5]),
                          mi.ScalarVector3f([-10000, 3.0, 31])]:
            s = mi.load_dict({
                "type" : "bsplinecurve",
                "filename" : "resources/data/common/meshes/curve.txt",
                "to_world" : mi.ScalarTransform4f().translate(translate) @ mi.ScalarTransform4f().scale((sx, 1, 1))
            })
            b = s.bbox()

            assert b.valid()
            assert dr.allclose(b.center(), translate)
            assert dr.allclose(b.min, translate - [1, 0, 0] - [sx, 1, 1])
            assert dr.allclose(b.max, translate + [1, 0, 0] + [sx, 1, 1])


@fresolver_append_path
def test04_parameters_changed(variants_vec_rgb):
    pytest.importorskip("numpy")
    import numpy as np

    new_control_points = np.array([
        1, 0, 0, 1,
        2, 0, 0, 1,
        3, 0, 0, 1,
        4, 0, 0, 1,
    ]).reshape((4, 4))
    new_control_points = mi.Point4f(new_control_points)

    s = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve.txt",
    })

    params = mi.traverse(s)
    params['control_points'] = dr.ravel(new_control_points)
    params.update()

    params = mi.traverse(s)
    assert dr.allclose(dr.ravel(new_control_points), params['control_points'])


@fresolver_append_path
def test05_ray_intersect(variant_scalar_rgb):
    for translate in [mi.Vector3f([0.0, 0.0, 0.0]),
                      mi.Vector3f([0.0, 0.0, 0.0])]:
        s = mi.load_dict({
            "type" : "scene",
            "foo" : {
                "type" : "bsplinecurve",
                "filename" : "resources/data/common/meshes/curve.txt",
                "to_world" : mi.Transform4f().translate(translate)
            }
        })

        # grid size
        n = 10
        for x in dr.linspace(Float, -0.5, 0.5, n):
            for y in dr.linspace(Float, -1.5, 1.5, n):
                x = x - translate[0]
                y = y - translate[1]

                ray = mi.Ray3f(o=[x, y, -10], d=[0, 0, 1],
                            time=0.0, wavelengths=[])
                si_found = s.ray_test(ray)

                assert si_found == (x <= 0.3 and -0.3 <= x and y <= 1 and -1 <= y)

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


@fresolver_append_path
def test06_ray_intersect_vec(variant_scalar_rgb):

    from mitsuba.scalar_rgb.test.util import check_vectorization

    def kernel(o):
        scene = mi.load_dict({
            "type" : "scene",
            "foo" : {
                "type" : "bsplinecurve",
                "filename" : "resources/data/common/meshes/curve.txt",
            }
        })

        o = 2.0 * o - 1.0
        o.z = 5.0

        t = scene.ray_intersect(mi.Ray3f(o, [0, 0, -1])).t
        dr.eval(t)
        return t

    check_vectorization(kernel, arg_dims = [3], atol=1e-5)


# TODO: Enable OptiX once the required drivers are above v531
@fresolver_append_path
def test07_differentiable_surface_interaction_ray_forward_follow_shape(variant_llvm_ad_rgb):
    scene = mi.load_dict({
        "type" : "scene",
        "curve" : {
            "type" : "bsplinecurve",
            "filename" : "resources/data/common/meshes/curve.txt",
        }
    })
    params = mi.traverse(scene)
    key = 'curve.control_points'

    # Test 00: With DetachShape and no moving rays, the output shouldn't produce
    #          any gradients (differentiating positions).

    ray = mi.Ray3f(mi.Vector3f(0, 0, 2), mi.Vector3f(0, 0, -1))

    theta = mi.Float(0)
    dr.enable_grad(theta)
    params[key] = dr.ravel(
            dr.unravel(mi.Point4f, params[key]) + mi.Vector4f([theta, 0, 0, 0])
    )
    params.update()
    pi = scene.ray_intersect_preliminary(ray)
    si = pi.compute_surface_interaction(ray, mi.RayFlags.All | mi.RayFlags.DetachShape)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.t), 0.0)
    assert dr.allclose(dr.grad(si.p), 0.0)
    assert dr.allclose(dr.grad(si.n), 0.0)

    # Test 01: When the curve is inflating, the point will move back
    #          along the ray. The normal, point and UVs will not change
    #          (differentiating radii).

    ray = mi.Ray3f(mi.Vector3f(0, 0, 2), mi.Vector3f(0, 0, -1))

    theta = mi.Float(1)
    dr.enable_grad(theta)

    cp_scaling = mi.Float([0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1])
    cp_scaling *= theta
    cp_scaling += mi.Float([1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0])
    params[key] *= cp_scaling;
    params.update()

    pi = scene.ray_intersect_preliminary(ray)
    si = pi.compute_surface_interaction(ray, mi.RayFlags.All)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.t), -1)
    assert dr.allclose(dr.grad(si.p), [0, 0, 1])
    assert dr.allclose(dr.grad(si.n), 0, atol=1e-6)
    assert dr.allclose(dr.grad(si.uv), 0)

    # Test 02: With FollowShape, any intersection point with a translating curve
    #          should move according to the translation. The normal and the
    #          UVs should be static (differentiating positions).

    ray = mi.Ray3f(mi.Vector3f(0.1, 0.1, 2), mi.Vector3f(0, 0, -1))

    theta = mi.Float(0)
    dr.enable_grad(theta)

    params[key] = dr.ravel(dr.unravel(
        mi.Point4f, params[key]) + mi.Vector4f([0, theta, 0, 0])
    )
    params.update()

    pi = scene.ray_intersect_preliminary(ray)
    si = pi.compute_surface_interaction(ray, mi.RayFlags.All | mi.RayFlags.FollowShape)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.p), [0, 1, 0])
    assert dr.allclose(dr.grad(si.n), 0, atol=1e-6)
    assert dr.allclose(dr.grad(si.uv), 0, atol=1e-6)


@fresolver_append_path
def test08_eval_parameterization(variants_vec_rgb):
    scene = mi.load_dict({
        "type" : "scene",
        "curve" : {
            "type" : "bsplinecurve",
            "filename" : "resources/data/common/meshes/curve.txt",
        }
    })

    N = 10
    x = dr.linspace(mi.Float, -0.2, 0.2, N)
    y = dr.linspace(mi.Float, -0.2, 0.2, N)
    x, y, = dr.meshgrid(x, y)
    ray = mi.Ray3f(mi.Vector3f(x, y, 2), mi.Vector3f(0, 0, -1))

    pi = scene.ray_intersect_preliminary(ray)
    si = pi.compute_surface_interaction(ray, mi.RayFlags.All | mi.RayFlags.DetachShape)

    curve = scene.shapes()[0]
    eval_param_si = curve.eval_parameterization(si.uv, active=si.is_valid())

    assert dr.allclose(si.p, eval_param_si.p)
    assert dr.allclose(si.uv, eval_param_si.uv)
    assert dr.allclose(si.n, eval_param_si.n, atol=1e-6)
    assert dr.allclose(si.sh_frame.n, eval_param_si.sh_frame.n, atol=1e-6)


@fresolver_append_path
def test09_instancing(variants_vec_rgb):
    scene = mi.load_dict({
        "type" : "scene",
        "group": {
            "type" : "shapegroup",
            "curve" : {
                "type" : "bsplinecurve",
                "filename" : "resources/data/common/meshes/curve.txt",
            }
        },
        "instance1": {
            "type" : "instance",
            "to_world": mi.ScalarTransform4f().translate([0, -2, 0]),
            "shapegroup": {
                "type": "ref",
                "id": "group"
            }
        },
        "instance2": {
            "type" : "instance",
            "to_world": mi.ScalarTransform4f().translate([0, 2, 0]),
            "shapegroup": {
                "type": "ref",
                "id": "group"
            }
        }
    })

    ray1 = mi.Ray3f(o=mi.Point3f(0, -2, 2), d=mi.Vector3f(0, 0, -1))
    pi1 = scene.ray_intersect_preliminary(ray1)

    ray2 = mi.Ray3f(o=mi.Point3f(0, 2, 2), d=mi.Vector3f(0, 0, -1))
    pi2 = scene.ray_intersect_preliminary(ray2)

    assert dr.all(pi1.is_valid())
    assert dr.all(pi2.is_valid())


# TODO: Enable OptiX once the required drivers are above v531
@fresolver_append_path
def test10_backface_culling(variant_llvm_ad_rgb):
    scene =  mi.load_dict({
        "type" : "scene",
        "foo" : {
            "type" : "bsplinecurve",
            "filename" : "resources/data/common/meshes/curve.txt",
        }
    })

    # Ray inside the curve
    ray = mi.Ray3f(o=[0, 0, 0], d=[0, 0, -1])
    si = scene.ray_intersect(ray)
    assert dr.all(~si.is_valid())

    # Ray outside the curve
    ray = mi.Ray3f(o=[0, 0, 2], d=[0, 0, -1])
    si = scene.ray_intersect(ray)
    assert dr.all(si.is_valid())


@fresolver_append_path
def test12_sample_silhouette_perimeter(variants_vec_rgb):
    curve = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve.txt",
    })
    curve_ptr = mi.ShapePtr(curve)
    length = -2 * (-1 - 4 * 0.3 + 0.3) * (1 / 6)

    x = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    y = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    z = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    samples = mi.Point3f(dr.meshgrid(x, y, z))

    ss = curve.sample_silhouette(samples, mi.DiscontinuityFlags.PerimeterType)
    assert dr.allclose(ss.discontinuity_type, mi.DiscontinuityFlags.PerimeterType.value)
    assert dr.allclose(dr.abs(ss.p.x), length/2)
    assert dr.allclose(dr.norm(mi.Point2f(ss.p.y, ss.p.z)), 1)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert dr.allclose(ss.pdf, dr.inv_four_pi * dr.inv_two_pi, atol=1e-6)
    assert (dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, curve_ptr))


@fresolver_append_path
def test13_sample_silhouette_interior(variants_vec_rgb):
    curve = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve.txt",
    })
    curve_ptr = mi.ShapePtr(curve)
    length = -2 * (-1 - 4 * 0.3 + 0.3) * (1 / 6)

    x = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    y = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    z = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    samples = mi.Point3f(dr.meshgrid(x, y, z))

    ss = curve.sample_silhouette(samples, mi.DiscontinuityFlags.InteriorType)
    assert dr.allclose(ss.discontinuity_type, mi.DiscontinuityFlags.InteriorType.value)
    assert dr.all((-length/2 <= ss.p.x) & (ss.p.x <= length/2))
    assert dr.allclose(dr.norm(mi.Point2f(ss.p.y, ss.p.z)), 1)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert dr.allclose(ss.n, mi.Point3f(0, ss.p.y, ss.p.z), atol=1e-6)
    assert dr.allclose(ss.pdf, dr.inv_two_pi * (1 / length) * dr.inv_two_pi, atol=1e-2)
    assert (dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, curve_ptr))


@fresolver_append_path
def test14_sample_silhouette_bijective(variants_vec_rgb):
    curve = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve_doc.txt",
    })

    x = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    y = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    z = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    samples = mi.Point3f(dr.meshgrid(x, y, z))

    ss_perimeter = curve.sample_silhouette(samples, mi.DiscontinuityFlags.PerimeterType)
    out_perimeter  = curve.invert_silhouette_sample(ss_perimeter)
    assert dr.allclose(samples, out_perimeter, atol=1e-6)

    ss_interior = curve.sample_silhouette(samples, mi.DiscontinuityFlags.InteriorType)
    out_interior = curve.invert_silhouette_sample(ss_interior)
    assert dr.allclose(samples, out_interior, atol=1e-7)


@fresolver_append_path
def test15_discontinuity_types(variants_vec_rgb):
    curve = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve_doc.txt",
    })

    types = curve.silhouette_discontinuity_types()
    assert mi.has_flag(types, mi.DiscontinuityFlags.InteriorType)
    assert mi.has_flag(types, mi.DiscontinuityFlags.PerimeterType)


@fresolver_append_path
def test16_differential_motion(variants_vec_rgb):
    if not dr.is_diff_v(mi.Float):
        pytest.skip("Only relevant in AD-enabled variants!")

    curve = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve_doc.txt",
    })
    params = mi.traverse(curve)

    theta = mi.Point3f(0.0)
    dr.enable_grad(theta)
    key = 'control_points'
    control_points = dr.unravel(mi.Point4f, params[key])
    positions = mi.Point3f(control_points.x, control_points.y, control_points.z)
    translation = mi.Transform4f().translate([theta.x, 2 * theta.y, 3 * theta.z])
    positions = translation @ positions
    control_points = mi.Point4f(
        positions.x,
        positions.y,
        positions.z,
        control_points[3]
    )
    params[key] = dr.ravel(control_points)
    params.update()

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.prim_index = 0
    si.p = mi.Point3f(1, 0, 0) # doesn't matter
    si.uv = mi.Point2f(0.5, 0.5)

    p_diff = curve.differential_motion(si)
    dr.forward(theta)
    v = dr.grad(p_diff)

    assert dr.allclose(p_diff, si.p)
    assert dr.allclose(v, [1.0, 2.0, 3.0])


@fresolver_append_path
def test17_primitive_silhouette_projection_interior(variants_vec_rgb):
    curve = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve.txt",
    })
    curve_ptr = mi.ShapePtr(curve)
    length = -2 * (-1 - 4 * 0.3 + 0.3) * (1 / 6)

    u = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    v = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    uv = mi.Point2f(dr.meshgrid(u, v))
    si = curve.eval_parameterization(uv)

    viewpoint = mi.Point3f(0, 0, 5)

    ss = curve.primitive_silhouette_projection(
        viewpoint, si, mi.DiscontinuityFlags.InteriorType, 0.)

    valid = ss.is_valid()
    ss = dr.gather(mi.SilhouetteSample3f, ss, dr.compress(valid))

    assert dr.allclose(ss.discontinuity_type, mi.DiscontinuityFlags.InteriorType.value)
    assert dr.all((-length/2 <= ss.p.x) & (ss.p.x <= length/2))
    assert dr.allclose(dr.norm(mi.Point2f(ss.p.y, ss.p.z)), 1)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert dr.allclose(ss.n, mi.Point3f(0, ss.p.y, ss.p.z), atol=1e-6)
    assert dr.all((dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, curve_ptr)))


@fresolver_append_path
def test18_primitive_silhouette_projection_perimeter(variants_vec_rgb):
    curve = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve.txt",
    })
    curve_ptr = mi.ShapePtr(curve)
    length = -2 * (-1 - 4 * 0.3 + 0.3) * (1 / 6)

    u = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    v = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    uv = mi.Point2f(dr.meshgrid(u, v))
    si = curve.eval_parameterization(uv)

    viewpoint = mi.Point3f(0, 0, 5)

    ss = curve.primitive_silhouette_projection(
        viewpoint, si, mi.DiscontinuityFlags.PerimeterType, 0.)

    valid = ss.is_valid()
    ss = dr.gather(mi.SilhouetteSample3f, ss, dr.compress(valid))

    assert dr.allclose(ss.discontinuity_type, mi.DiscontinuityFlags.PerimeterType.value)
    assert dr.allclose(dr.abs(ss.p.x), length/2)
    assert dr.allclose(dr.norm(mi.Point2f(ss.p.y, ss.p.z)), 1)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert dr.all((dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, curve_ptr)))


@fresolver_append_path
def test19_precompute_silhouette(variants_vec_rgb):
    curve = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve.txt",
    })

    indices, weights = curve.precompute_silhouette(mi.ScalarPoint3f(0, 0, 3))

    assert len(indices) == 2
    assert len(weights) == 2
    assert indices[0] == mi.DiscontinuityFlags.PerimeterType.value
    assert indices[1] == mi.DiscontinuityFlags.InteriorType.value
    assert weights[0] == 0.5
    assert weights[1] == 0.5


@fresolver_append_path
def test20_sample_precomputed_silhouette(variants_vec_rgb):
    curve = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve.txt",
    })
    curve_ptr = mi.ShapePtr(curve)

    samples = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    viewpoint = mi.Point3f(0, 0, 5)
    length = -2 * (-1 - 4 * 0.3 + 0.3) * (1 / 6)

    ss = curve.sample_precomputed_silhouette(
        viewpoint, mi.DiscontinuityFlags.InteriorType.value, samples)
    valid = ss.is_valid()
    ss = dr.gather(mi.SilhouetteSample3f, ss, dr.compress(valid))

    assert dr.allclose(ss.discontinuity_type, mi.DiscontinuityFlags.InteriorType.value)
    assert dr.all((-length/2 <= ss.p.x) & (ss.p.x <= length/2))
    assert dr.allclose(dr.norm(mi.Point2f(ss.p.y, ss.p.z)), 1)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert dr.allclose(ss.n, mi.Point3f(0, ss.p.y, ss.p.z), atol=1e-6)
    assert dr.allclose(dr.mean(ss.pdf), 1 / (2 * length), atol=1e-2)
    assert dr.all((dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, curve_ptr)))

    ss = curve.sample_precomputed_silhouette(
        viewpoint, mi.DiscontinuityFlags.PerimeterType.value, samples)
    valid = ss.is_valid()
    ss = dr.gather(mi.SilhouetteSample3f, ss, dr.compress(valid))

    assert dr.allclose(ss.discontinuity_type, mi.DiscontinuityFlags.PerimeterType.value)
    assert dr.allclose(dr.abs(ss.p.x), length/2)
    assert dr.allclose(dr.norm(mi.Point2f(ss.p.y, ss.p.z)), 1)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert dr.allclose(ss.pdf, dr.inv_four_pi, atol=1e-6)
    assert dr.all((dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, curve_ptr)))


@fresolver_append_path
def test21_shape_type(variant_scalar_rgb):
    curve = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve.txt",
    })
    assert curve.shape_type() == mi.ShapeType.BSplineCurve.value;
