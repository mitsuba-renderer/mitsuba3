import pytest
import drjit as dr
import mitsuba as mi

def default_sdf_grid():
    return mi.TensorXf([0, 0, 1, 0, 0, 1, 0, 0], shape=(2, 2, 2, 1))

def test01_create(variant_scalar_rgb):
    for normal_method in ["analytic", "smooth"]:
        with pytest.raises(RuntimeError) as e:
            s = mi.load_dict({
                "type" : "sdfgrid",
                "normals" : normal_method
            })

        s = mi.load_dict({
            "type" : "sdfgrid",
            "normals" : normal_method,
            "grid": default_sdf_grid()
        })
        assert s is not None


def test02_bbox(variant_scalar_rgb):
    sy = 2.5
    for sx in [1, 2, 4]:
        for translate in [mi.ScalarVector3f([1.3, -3.0, 5]),
                          mi.ScalarVector3f([-10000, 3.0, 31])]:
            s = mi.load_dict({
                "type" : "sdfgrid",
                "to_world" : mi.ScalarTransform4f().translate(translate).scale((sx, sy, 1.0)),
                "grid": default_sdf_grid()
            })

            b = s.bbox()

            assert b.valid()
            assert dr.allclose(b.center(), translate + mi.ScalarVector3f([0.5 * sx, 0.5 * sy, 0.5]))
            assert dr.allclose(b.min, translate)
            assert dr.allclose(b.max, translate +[sx, sy, 1.0])


def test03_parameters_changed(variant_scalar_rgb):
    pytest.importorskip("numpy")
    import numpy as np

    # Diagonal plane
    sdf_grid = np.array([
        -np.sqrt(2)/2, -np.sqrt(2)/2, # z = 0, y = 0
        0, 0, # z = 0, y = 1
        0, 0, # z = 1, y = 0
        np.sqrt(2)/2, np.sqrt(2)/2 # z = 1, y = 1
    ]).reshape((2, 2, 2, 1))

    s = mi.load_dict({
        "type" : "sdfgrid",
        "grid": default_sdf_grid()
    })

    params = mi.traverse(s)
    params['grid'] = mi.TensorXf(sdf_grid)
    params.update()

    assert True


def test04_ray_intersect(variants_all_ad_rgb):
    pytest.importorskip("numpy")
    import numpy as np

    # Diagonal plane
    sdf_grid = np.array([
        -np.sqrt(2)/2, -np.sqrt(2)/2, # z = 0, y = 0
        0, 0, # z = 0, y = 1
        0, 0, # z = 1, y = 0
        np.sqrt(2)/2, np.sqrt(2)/2 # z = 1, y = 1
    ]).reshape((2, 2, 2, 1))

    for translate in [mi.ScalarVector3f([0.0, 0.0, 0]),
                      mi.ScalarVector3f([-0.5, 0.5, 0.2])]:
        s = mi.load_dict({
            "type" : "scene",
            "sdf": {
                "type" : "sdfgrid",
                "to_world" : mi.ScalarTransform4f().translate(translate),
                "grid": default_sdf_grid()
            }
        })

        params = mi.traverse(s)
        params['sdf.grid'] = mi.TensorXf(sdf_grid)
        params.update()

        n = 10
        for x in dr.linspace(mi.Float, -2, 2, n):
            for y in dr.linspace(mi.Float, -2, 2, n):
                origin = translate + mi.Vector3f(x, y, 3)
                direction = mi.Vector3f(0, 0, -1)
                ray = mi.Ray3f(o=origin, d=direction)

                si_found = s.ray_test(ray)
                assert si_found == (x >= 0 and x <= 1 and y >= 0 and y<= 1)

                if si_found[0]:
                    si = s.ray_intersect(ray, mi.RayFlags.All, True)

                    assert dr.allclose(si.t, 2 + y)
                    assert dr.allclose(si.n, mi.Normal3f(0, 1 / dr.sqrt(2), 1 / dr.sqrt(2)))
                    assert dr.allclose(si.p, ray.o - mi.Vector3f(0, 0, 2 + y))


def test05_ray_intersect_instancing(variants_all_ad_rgb):
    pytest.importorskip("numpy")
    import numpy as np

    # Diagonal plane
    sdf_grid = np.array([
        -np.sqrt(2)/2, -np.sqrt(2)/2, # z = 0, y = 0
        0, 0, # z = 0, y = 1
        0, 0, # z = 1, y = 0
        np.sqrt(2)/2, np.sqrt(2)/2 # z = 1, y = 1
    ]).reshape((2, 2, 2, 1))

    instance_translations = [mi.ScalarVector3f([0.0, 0.0, 0]), mi.ScalarVector3f([8.0, 0.0, 0.0])]

    s = mi.load_dict({
        "type" : "scene",
        'shape_group': {
            'type': 'shapegroup',
            'sdf': {
                'type': 'sdfgrid',
                'grid': default_sdf_grid()
            }
        },
        'first_sdf': {
            'type': 'instance',
            'to_world': mi.ScalarTransform4f().translate(instance_translations[0]),
            'shapegroup': {
                'type': 'ref',
                'id': 'shape_group'
            }
        },
        'second_sdf': {
            'type': 'instance',
            'to_world': mi.ScalarTransform4f().translate(instance_translations[1]),
            'shapegroup': {
                'type': 'ref',
                'id': 'shape_group'
            }
        }
    })

    params = mi.traverse(s)
    params['shape_group.sdf.grid'] = mi.TensorXf(sdf_grid)
    params.update()

    n = 10
    for translate in instance_translations:
        for x in dr.linspace(mi.Float, -2, 2, n):
            for y in dr.linspace(mi.Float, -2, 2, n):
                origin = translate + mi.Vector3f(x, y, 3)
                direction = mi.Vector3f(0, 0, -1)
                ray = mi.Ray3f(o=origin, d=direction)
                si_found = s.ray_test(ray)
                assert si_found == (x >= 0 and x <= 1 and y >= 0 and y<= 1)

                if si_found[0]:
                    si = s.ray_intersect(ray, mi.RayFlags.All, True)

                    assert dr.allclose(si.t, 2 + y)
                    assert dr.allclose(si.n, np.array([0, 1 / np.sqrt(2), 1 / np.sqrt(2)]))
                    assert dr.allclose(si.p, ray.o - mi.Vector3f(0, 0, 2 + y))


def test07_differentiable_surface_interaction_ray_forward_follow_shape(variants_all_ad_rgb):
    pytest.importorskip("numpy")
    import numpy as np

    scene = mi.load_dict({
        "type" : "scene",
        "sdf" : {
            "type" : "sdfgrid",
            "normals" : "analytic",
            "grid": default_sdf_grid()
        }
    })
    params = mi.traverse(scene)

    # Diagonal plane
    sdf_grid = np.array([
        -np.sqrt(2)/2, -np.sqrt(2)/2, # z = 0, y = 0
        0, 0, # z = 0, y = 1
        0, 0, # z = 1, y = 0
        np.sqrt(2)/2, np.sqrt(2)/2 # z = 1, y = 1
    ]).reshape((2, 2, 2, 1))

    params['sdf.grid'] = mi.TensorXf(sdf_grid)
    params.update()

    # Test 00: With DetachShape and no moving rays, the output shouldn't produce
    #          any gradients (differentiating `to_world`).

    ray = mi.Ray3f(mi.Vector3f(0.5, 0.5, 4), mi.Vector3f(0, 0, -1))

    theta = mi.Float(0)
    dr.enable_grad(theta)
    params['sdf.to_world'] = mi.Transform4f().translate(mi.Vector3f(theta))
    params.update()
    pi = scene.ray_intersect_preliminary(ray)
    si = pi.compute_surface_interaction(ray, mi.RayFlags.All | mi.RayFlags.DetachShape)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.t), 0.0)
    assert dr.allclose(dr.grad(si.p), 0.0)
    assert dr.allclose(dr.grad(si.n), 0.0)

    # Test 01: With DetachShape and no moving rays, the output shouldn't produce
    #          any gradients (differentiating `grid`).

    ray = mi.Ray3f(mi.Vector3f(0.5, 0.5, 2), mi.Vector3f(0, 0, -1))

    theta = dr.zeros(mi.TensorXf, shape=(2,2,2,1))
    dr.enable_grad(theta)
    params['sdf.grid'] = params['sdf.grid'] + theta
    params['sdf.to_world'] = dr.detach(params['sdf.to_world'])
    params.update()
    pi = scene.ray_intersect_preliminary(ray)
    si = pi.compute_surface_interaction(ray, mi.RayFlags.All | mi.RayFlags.DetachShape)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.t), 0.0)
    assert dr.allclose(dr.grad(si.p), 0.0)
    assert dr.allclose(dr.grad(si.n), 0.0)

    # Test 02: When the plane is moving upwards, the point will move back along
    #          the ray. The normal isn't changing but the point is
    #          (differentiating `to_world`).

    ray = mi.Ray3f(mi.Vector3f(0.5, 0.5, 2), mi.Vector3f(0, 0, -1))

    theta = mi.Float(0)
    dr.enable_grad(theta)

    params['sdf.grid'] = dr.detach(params['sdf.grid'])
    params['sdf.to_world'] = mi.Transform4f().translate([0, theta, 0])
    params.update()
    pi = scene.ray_intersect_preliminary(ray)
    si = pi.compute_surface_interaction(ray, mi.RayFlags.All)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.t), -1)
    assert dr.allclose(dr.grad(si.p), [0, 0, 1])
    assert dr.allclose(dr.grad(si.n), 0, atol=1e-7)

    # Test 03: When the plane is moving upwards, the point will move back along
    #          the ray. The normal isn't changing but the point is
    #          (differentiating `grid`).

    theta = dr.zeros(mi.TensorXf, shape=(2,2,2,1))
    dr.enable_grad(theta)

    grid = params['sdf.grid']
    offset = (dr.sqrt(2) * theta) / 2 # Shift by theta in the y direction
    grid = grid - offset
    params['sdf.grid'] = grid
    params['sdf.to_world'] = dr.detach(params['sdf.to_world'])
    params.update()

    ray = mi.Ray3f(mi.Vector3f(0.5, 0.5, 2), mi.Vector3f(0, 0, -1))
    pi = scene.ray_intersect_preliminary(ray)
    si = pi.compute_surface_interaction(ray, mi.RayFlags.All)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.t), -1)
    assert dr.allclose(dr.grad(si.p), [0, 0, 1])
    assert dr.allclose(dr.grad(si.n), 0, atol=1e-7)

    # Test 04: With FollowShape, any intersection point with a translating plane
    #          should move according to the translation. The normal and the
    #          UVs should be static (differentiating `to_world`).

    theta = mi.Float(0)
    dr.enable_grad(theta)

    params['sdf.grid'] = dr.detach(params['sdf.grid'])
    params['sdf.to_world'] = mi.Transform4f().translate([0, theta, 0])
    params.update()

    ray = mi.Ray3f(mi.Vector3f(0.5, 0.5, 2), mi.Vector3f(0, 0, -1))
    pi = scene.ray_intersect_preliminary(ray)
    si = pi.compute_surface_interaction(ray, mi.RayFlags.All | mi.RayFlags.FollowShape)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.p), [0, 1, 0])
    assert dr.allclose(dr.grad(si.n), 0, atol=1e-7)

    # Test 05: With FollowShape, any intersection point with a translating plane
    #          should move according to the translation. The normal and the
    #          UVs should be static (differentiating `grid`).

    theta = dr.zeros(mi.TensorXf, shape=(2,2,2,1))
    dr.enable_grad(theta)

    params['sdf.to_world'] = dr.detach(params['sdf.to_world'])
    grid = params['sdf.grid']
    offset = (dr.sqrt(2) * theta) / 2 # Shift by theta in the y direction
    grid = grid - offset
    params['sdf.grid'] = grid
    params.update()

    ray = mi.Ray3f(mi.Vector3f(0.5, 0.5, 2), mi.Vector3f(0, 0, -1))
    pi = scene.ray_intersect_preliminary(ray)
    si = pi.compute_surface_interaction(ray, mi.RayFlags.All | mi.RayFlags.FollowShape)

    dr.forward(theta)

    assert dr.allclose(dr.grad(si.p), [0, 0.5, 0.5]) # Direction of surface normal
    assert dr.allclose(dr.grad(si.n), 0, atol=1e-7)


def test08_load_tensor(variants_all_ad_rgb):
    pytest.importorskip("numpy")
    import numpy as np

    # Diagonal plane
    sdf_grid = np.array([
        -np.sqrt(2)/2, -np.sqrt(2)/2, # z = 0, y = 0
        0, 0, # z = 0, y = 1
        0, 0, # z = 1, y = 0
        np.sqrt(2)/2, np.sqrt(2)/2 # z = 1, y = 1
    ]).reshape((2, 2, 2, 1))

    for translate in [mi.ScalarVector3f([0.0, 0.0, 0]),
                      mi.ScalarVector3f([-0.5, 0.5, 0.2])]:
        s = mi.load_dict({
            "type" : "scene",
            "sdf": {
                "type" : "sdfgrid",
                "to_world" : mi.ScalarTransform4f().translate(translate),
                "grid" : sdf_grid
            }
        })

        n = 10
        for x in dr.linspace(mi.Float, -2, 2, n):
            for y in dr.linspace(mi.Float, -2, 2, n):
                origin = translate + mi.Vector3f(x, y, 3)
                direction = mi.Vector3f(0, 0, -1)
                ray = mi.Ray3f(o=origin, d=direction)

                si_found = s.ray_test(ray)
                assert si_found == (x >= 0 and x <= 1 and y >= 0 and y<= 1)

                if si_found[0]:
                    si = s.ray_intersect(ray, mi.RayFlags.All, True)

                    assert dr.allclose(si.t, 2 + y)
                    assert dr.allclose(si.n, np.array([0, 1 / np.sqrt(2), 1 / np.sqrt(2)]))
                    assert dr.allclose(si.p, ray.o - mi.Vector3f(0, 0, 2 + y))


def test09_shape_type(variant_scalar_rgb):
    sdf = mi.load_dict({ "type" : "sdfgrid",
                         "grid" : default_sdf_grid()})
    assert sdf.shape_type() == mi.ShapeType.SDFGrid.value
