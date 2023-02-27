import pytest
import drjit as dr
import mitsuba as mi

from drjit.scalar import ArrayXf as Float

# TODO: Enable CUDA variants aswell (orthographic is broken).

def test01_create(variants_all_rgb):
    s = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve.txt",
    })
    assert s is not None
    assert s.primitive_count() == 1


def test02_create_multiple_curves(variants_all_rgb):
    s = mi.load_dict({
        "type" : "bsplinecurve",
        "filename" : "resources/data/common/meshes/curve_6.txt",
    })
    assert s is not None
    assert s.primitive_count() == 6


#def test02_bbox(variant_scalar_rgb):
#    #sy = 2.5
#    sy = 1
#    #for sx in [1, 2, 4]:
#    for sx in [1]:
#        #for translate in [mi.Vector3f([1.3, -3.0, 5]),
#        #                  mi.Vector3f([-10000, 3.0, 31])]:
#        for translate in [mi.Vector3f(0.)]:
#            s = mi.load_dict({
#                "type" : "bsplinecurve",
#                "filename" : "resources/data/common/meshes/curve.txt",
#                "to_world" : mi.Transform4f.translate(translate) @ mi.Transform4f.scale((sx, sy, 1.0))
#            })
#            b = s.bbox()
#
#            assert b.valid()
#            #assert dr.allclose(b.center(), translate)
#            assert dr.allclose(b.min, translate - [sx, sy, 0.0])
#            assert dr.allclose(b.max, translate + [sx, sy, 0.0])


def test03_parameters_changed(variants_vec_rgb):
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


#def test04_ray_intersect(variant_cuda_ad_rgb):
#    pytest.importorskip("numpy")
#    import numpy as np
#
#    # Diagonal plane
#    sdf_grid = np.array([
#        -np.sqrt(2)/2, -np.sqrt(2)/2, # z = 0, y = 0
#        0, 0, # z = 0, y = 1
#        0, 0, # z = 1, y = 0
#        np.sqrt(2)/2, np.sqrt(2)/2 # z = 1, y = 1
#    ]).reshape((2, 2, 2, 1))
#
#    for translate in [mi.ScalarVector3f([0.0, 0.0, 0]),
#                      mi.ScalarVector3f([-0.5, 0.5, 0.2])]:
#        s = mi.load_dict({
#            "type" : "scene",
#            "sdf": {
#                "type" : "sdfgrid",
#                "to_world" : mi.ScalarTransform4f.translate(translate)
#            }
#        })
#
#        params = mi.traverse(s)
#        params['sdf.grid'] = mi.TensorXf(sdf_grid)
#        params.update()
#
#        n = 10
#        for x in dr.linspace(mi.Float, -2, 2, n):
#            for y in dr.linspace(mi.Float, -2, 2, n):
#                origin = translate + mi.Vector3f(x, y, 3)
#                direction = mi.Vector3f(0, 0, -1)
#                ray = mi.Ray3f(o=origin, d=direction)
#
#                si_found = s.ray_test(ray)
#                assert si_found == (x >= 0 and x <= 1 and y >= 0 and y<= 1)
#
#                if si_found[0]:
#                    si = s.ray_intersect(ray, mi.RayFlags.All, True)
#
#                    assert dr.allclose(si.t, 2 + y)
#                    assert dr.allclose(si.n, [0, 1 / np.sqrt(2), 1 / np.sqrt(2)])
#                    assert dr.allclose(si.p, ray.o - mi.Vector3f(0, 0, 2 + y))


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

    dr.set_flag(dr.JitFlag.VCallRecord, False) # FIXME: remove

    ray = mi.Ray3f(mi.Vector3f(0, 0, 2), mi.Vector3f(0, 0, -1))

    theta = mi.Float(1)
    dr.enable_grad(theta)

    radii = dr.gather(mi.Float, params[key], dr.arange(mi.UInt32, 4) * 4 + 3)
    dr.scatter(params[key], radii * theta, dr.arange(mi.UInt32, 4) * 4 + 3)
    dr.eval(params[key])
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


def test08_eval_parameterization(variant_llvm_ad_rgb):
    scene = mi.load_dict({
        "type" : "scene",
        "curve" : {
            "type" : "bsplinecurve",
            "filename" : "resources/data/common/meshes/curve.txt",
        }
    })

    N = 1
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
