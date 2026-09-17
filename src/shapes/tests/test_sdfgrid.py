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
                    si = s.ray_intersect(ray, mi.RayFlags.Default, True)

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
                'grid': mi.TensorXf(default_sdf_grid())
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
                    si = s.ray_intersect(ray, mi.RayFlags.Default, True)

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
    si = scene.compute_surface_interaction(ray, pi, mi.RayFlags.Default | mi.RayFlags.DetachShape)

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
    si = scene.compute_surface_interaction(ray, pi, mi.RayFlags.Default | mi.RayFlags.DetachShape)

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
    si = scene.compute_surface_interaction(ray, pi, mi.RayFlags.Default)

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
    si = scene.compute_surface_interaction(ray, pi, mi.RayFlags.Default)

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
    si = scene.compute_surface_interaction(ray, pi, mi.RayFlags.Default | mi.RayFlags.FollowShape)

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
    si = scene.compute_surface_interaction(ray, pi, mi.RayFlags.Default | mi.RayFlags.FollowShape)

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
                "grid" : mi.TensorXf(sdf_grid)
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
                    si = s.ray_intersect(ray, mi.RayFlags.Default, True)

                    assert dr.allclose(si.t, 2 + y)
                    assert dr.allclose(si.n, np.array([0, 1 / np.sqrt(2), 1 / np.sqrt(2)]))
                    assert dr.allclose(si.p, ray.o - mi.Vector3f(0, 0, 2 + y))


def test09_shape_type(variant_scalar_rgb):
    sdf = mi.load_dict({ "type" : "sdfgrid",
                         "grid" : default_sdf_grid()})
    assert sdf.shape_type() == mi.ShapeType.SDFGrid.value


@pytest.mark.parametrize('shape', [(3, 5, 8), (8, 5, 3)])
def test10_rectangular_grid(variants_all_rgb, shape):
    # Plane x + 2y + 4z = 3.5 sampled on a grid with distinct dimensions
    nz, ny, nx = shape
    grid = [x / (nx - 1) + 2 * y / (ny - 1) + 4 * z / (nz - 1) - 3.5
            for z in range(nz) for y in range(ny) for x in range(nx)]
    scene = mi.load_dict({
        'type': 'scene',
        'sdf': {
            'type': 'sdfgrid',
            'grid': mi.TensorXf(grid, shape=(*shape, 1))
        }
    })
    p = mi.Point3f(0.25, 0.375, 0.625)
    n = dr.normalize(mi.Normal3f(1, 2, 4))
    for axis in range(3):
        for sign in (-1, 1):
            d = mi.Vector3f(0)
            d[axis] = sign
            ray = mi.Ray3f(p - 2 * d, d)
            si = scene.ray_intersect(ray)
            assert dr.all(si.is_valid())
            assert dr.allclose(si.t, 2, atol=1e-5)
            assert dr.allclose(si.n, n, atol=1e-5)
            assert dr.allclose(si.sh_frame.n, n, atol=1e-5)
            assert dr.all(scene.ray_test(ray))


def test11_no_cracks(variants_all_rgb):
    """
    Rays that cross the surface within rounding distance of a voxel face used
    to pass through it. The two adjacent voxels evaluated the SDF on their
    shared face with different rounding and could disagree about its sign, and
    the tight voxel bounding boxes left no margin for the ray-box test of the
    acceleration structure. This test aims rays at points where the trilinear
    surface of a sphere grid crosses a grid plane.
    """
    pytest.importorskip("numpy")
    import numpy as np

    res, n = 64, 2000
    t = np.linspace(0, 1, res)
    z, y, x = np.meshgrid(t, t, t, indexing='ij')
    f = (np.sqrt((x - 0.5)**2 + (y - 0.5)**2 + (z - 0.5)**2) - 0.45).astype(np.float32)
    scene = mi.load_dict({
        'type': 'scene',
        'sdf': {
            'type': 'sdfgrid',
            'grid': mi.TensorXf(f.reshape(res, res, res, 1))
        }
    })

    rng = np.random.default_rng(0)
    pts = []
    for axis in range(3):
        # g[i, k, j]: grid plane 'i' along 'axis', remaining axes in order
        g = np.moveaxis(f.astype(np.float64), 2 - axis, 0)
        i = rng.integers(1, res - 1, n)
        a = rng.uniform(0, res - 1, n)
        j = np.minimum(a.astype(int), res - 2)
        v = (a - j)[:, None]
        i2, j2 = i[:, None], j[:, None]
        k = np.arange(res - 1)[None, :]

        # Within each cell of the plane, the interpolant is linear along 'k'
        f00, f10 = g[i2, k, j2], g[i2, k, j2 + 1]
        f01, f11 = g[i2, k + 1, j2], g[i2, k + 1, j2 + 1]
        c0 = (1 - v) * f00 + v * f10
        c1 = (1 - v) * (f01 - f00) + v * (f11 - f10)
        with np.errstate(divide='ignore', invalid='ignore'):
            w = -c0 / c1
        ok = (w >= 0) & (w <= 1)
        first = np.argmax(ok, axis=1)
        sel = np.nonzero(ok[np.arange(n), first])[0]

        p = np.empty((len(sel), 3))
        other = [ax for ax in range(3) if ax != axis]
        p[:, axis] = i[sel] / (res - 1)
        p[:, other[0]] = a[sel] / (res - 1)
        p[:, other[1]] = (first[sel] + w[sel, first[sel]]) / (res - 1)
        pts.append(p)
    p = np.concatenate(pts)

    # Random directions entering the sphere at 'p', excluding grazing angles
    d = rng.normal(size=p.shape)
    d /= np.linalg.norm(d, axis=1, keepdims=True)
    normal = (p - 0.5) / np.linalg.norm(p - 0.5, axis=1, keepdims=True)
    cos = np.sum(d * normal, axis=1)
    d[cos > 0] *= -1
    keep = np.abs(cos) > 0.1
    d, p = d[keep], p[keep]
    o = (p - 1.5 * d).astype(np.float32)
    d = d.astype(np.float32)

    if mi.variant().startswith('scalar'):
        t, occluded = [], []
        for oi, di in zip(o.tolist(), d.tolist()):
            ray = mi.Ray3f(mi.Point3f(*oi), mi.Vector3f(*di))
            t.append(scene.ray_intersect_preliminary(ray).t)
            ray.maxt = 1.51
            occluded.append(scene.ray_test(ray))
    else:
        ray = mi.Ray3f(mi.Point3f(np.ascontiguousarray(o.T)),
                       mi.Vector3f(np.ascontiguousarray(d.T)))
        t = scene.ray_intersect_preliminary(ray).t
        ray.maxt = 1.51
        occluded = scene.ray_test(ray)

    leaks = np.abs(np.array(t) - 1.5) > 1e-3
    assert leaks.sum() == 0, f'{leaks.sum()} of {len(o)} rays passed through the surface'
    assert np.all(np.array(occluded))


def test12_occupied_voxel_count_updates(variants_all_rgb):
    # Grow to full occupancy, then shrink both the occupancy and the resolution
    scene = None
    for res, plane, expected_count, expected_x in [(8, 0.4, 49, 0.4),
                                                   (8, None, 343, 0.5 / 7),
                                                   (4, 0.75, 9, 0.75)]:
        values = [x / (res - 1) - plane if plane is not None
                  else (-1) ** (x + y + z)
                  for z in range(res) for y in range(res) for x in range(res)]
        grid = mi.TensorXf(values, shape=(res, res, res, 1))
        if scene is None:
            scene = mi.load_dict({'type': 'scene',
                                  'sdf': {'type': 'sdfgrid', 'grid': grid}})
        else:
            params = mi.traverse(scene)
            params['sdf.grid'] = grid
            params.update()

        assert scene.shapes()[0].primitive_count() == expected_count
        ray = mi.Ray3f(mi.Point3f(-1, 0.2, 0.3), mi.Vector3f(1, 0, 0))
        pi = scene.ray_intersect_preliminary(ray)
        assert dr.all(pi.is_valid())
        assert dr.allclose(pi.t, 1 + expected_x, atol=1e-5)
        for offset in (-0.01, 0.01):
            ray.maxt = 1 + expected_x + offset
            assert dr.all(scene.ray_test(ray) == (offset > 0))


def test13_empty_grid(variants_all_rgb):
    for value in (-1, 1):
        with pytest.raises(RuntimeError, match='at least.*non-empty voxel'):
            mi.load_dict({'type': 'sdfgrid',
                          'grid': mi.TensorXf([value] * 60, shape=(3, 4, 5, 1))})


def test14_trilinear_gradient(variants_all_rgb):
    # A trilinear polynomial with a varying normal and all mixed terms,
    # sampled on a rectangular grid under nonuniform scaling
    nx, ny, nz = 8, 5, 4
    values = [z + x * y + 0.25 * y * z + 0.5 * z * x + 0.125 * x * y * z - 0.6
              for z in [i / (nz - 1) for i in range(nz)]
              for y in [i / (ny - 1) for i in range(ny)]
              for x in [i / (nx - 1) for i in range(nx)]]
    to_world = mi.ScalarTransform4f().translate([-3, 2, 1]).scale([2, 3, 4])
    scene = mi.load_dict({'type': 'scene', 'sdf': {
        'type': 'sdfgrid', 'normals': 'analytic', 'to_world': to_world,
        'grid': mi.TensorXf(values, shape=(nz, ny, nx, 1))}})
    to_world = mi.Transform4f(to_world)

    # Include grid planes and the outer x faces
    for x, y in [(0.13, 0.2), (0.32, 0.41), (0.71, 0.78),
                 (3 / 7, 0.5), (0, 0.25), (1, 0.2)]:
        z = (0.6 - x * y) / (1 + 0.25 * y + 0.5 * x + 0.125 * x * y)
        ray = mi.Ray3f(to_world @ mi.Point3f(x, y, 1),
                       to_world @ mi.Vector3f(0, 0, -1))
        si = scene.ray_intersect(ray)
        assert dr.all(si.is_valid())
        assert dr.allclose(si.t, 1 - z, atol=1e-5)
        gradient = mi.Normal3f(y + 0.5 * z + 0.125 * y * z,
                               x + 0.25 * z + 0.125 * x * z,
                               1 + 0.25 * y + 0.5 * x + 0.125 * x * y)
        normal = dr.normalize(to_world @ gradient)
        assert dr.allclose(si.n, normal, atol=2e-5)
        assert dr.allclose(si.sh_frame.n, normal, atol=2e-5)


@pytest.mark.parametrize('source', ['grid', 'ray'])
def test15_trilinear_gradient_derivatives(variants_all_ad_rgb, source):
    theta = mi.Float(0)
    dr.enable_grad(theta)
    a = 1 + theta if source == 'grid' else mi.Float(1)
    x = 0.37 + theta if source == 'ray' else mi.Float(0.37)
    y = 0.43
    gx = mi.Float([0, 1, 0, 1, 0, 1, 0, 1])
    gy = mi.Float([0, 0, 1, 1, 0, 0, 1, 1])
    gz = mi.Float([0, 0, 0, 0, 1, 1, 1, 1])
    values = gz + a * gx * gy + 0.25 * gy * gz + 0.5 * gz * gx + 0.125 * gx * gy * gz - 0.6
    scene = mi.load_dict({'type': 'scene', 'sdf': {
        'type': 'sdfgrid', 'normals': 'analytic',
        'grid': mi.TensorXf(values, shape=(2, 2, 2, 1))}})

    ray = mi.Ray3f(mi.Point3f(x, y, 1), mi.Vector3f(0, 0, -1))
    si = scene.ray_intersect(ray)
    assert dr.all(si.is_valid())

    z = (0.6 - a * x * y) / (1 + 0.25 * y + 0.5 * x + 0.125 * x * y)
    expected_p = mi.Point3f(x, y, z)
    expected_n = dr.normalize(mi.Normal3f(a * y + 0.5 * z + 0.125 * y * z,
                                          a * x + 0.25 * z + 0.125 * x * z,
                                          1 + 0.25 * y + 0.5 * x + 0.125 * x * y))
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)
    assert dr.allclose(dr.grad(si.p), dr.grad(expected_p), atol=2e-5)
    assert dr.allclose(dr.grad(si.n), dr.grad(expected_n), atol=2e-5)


def test16_gradient_at_voxel_boundary(variants_all_rgb):
    # The field is continuous, but its x derivative jumps from 1 to 3 at the
    # shared face. The geometric normal is the one-sided normal of the
    # intersected voxel.
    values = [z + (x if x <= 0.5 else 3 * x - 1) + 0.2 * y - 0.8
              for z in (0, 1) for y in (0, 1) for x in (0, 0.5, 1)]
    scene = mi.load_dict({'type': 'scene', 'sdf': {
        'type': 'sdfgrid', 'normals': 'analytic',
        'grid': mi.TensorXf(values, shape=(2, 2, 3, 1))}})
    left = dr.normalize(mi.Normal3f(1, 0.2, 1))
    right = dr.normalize(mi.Normal3f(3, 0.2, 1))
    for x in (0.5 - 2e-7, 0.5, 0.5 + 2e-7):
        ray = mi.Ray3f(mi.Point3f(x, 0.25, 1), mi.Vector3f(0, 0, -1))
        si = scene.ray_intersect(ray)
        assert dr.all(si.is_valid())
        matches_left = dr.allclose(si.n, left, atol=1e-6)
        matches_right = dr.allclose(si.n, right, atol=1e-6)
        if x < 0.5:
            assert matches_left
        elif x > 0.5:
            assert matches_right
        else:
            assert matches_left or matches_right


def test17_smooth_normals(variants_all_rgb):
    # Smooth normals of a sphere grid are accurate and continuous across grid
    # planes, where the analytic normals of neighboring voxels differ
    pytest.importorskip("numpy")
    import numpy as np

    res = 64
    t = np.linspace(0, 1, res)
    z, y, x = np.meshgrid(t, t, t, indexing='ij')
    f = (np.sqrt((x - 0.5)**2 + (y - 0.5)**2 + (z - 0.5)**2) - 0.45).astype(np.float32)
    scene = mi.load_dict({'type': 'scene', 'sdf': {
        'type': 'sdfgrid', 'normals': 'smooth',
        'grid': mi.TensorXf(f.reshape(res, res, res, 1))}})

    def trace(o):
        if mi.variant().startswith('scalar'):
            n = [scene.ray_intersect(mi.Ray3f(mi.Point3f(*oi), mi.Vector3f(0, 0, -1))).sh_frame.n
                 for oi in o.T.tolist()]
            return np.array(n).T
        ray = mi.Ray3f(mi.Point3f(np.ascontiguousarray(o)), mi.Vector3f(0, 0, -1))
        return np.array(scene.ray_intersect(ray).sh_frame.n)

    rng = np.random.default_rng(0)
    xy = rng.uniform(0.2, 0.8, (2, 200))
    o = np.concatenate([xy, np.full((1, 200), 1.5)]).astype(np.float32)
    n = trace(o)
    p = np.concatenate([xy, 0.5 + np.sqrt(0.45**2 - np.sum((xy - 0.5)**2, axis=0))[None]])
    ref = (p - 0.5) / np.linalg.norm(p - 0.5, axis=0, keepdims=True)
    angle = np.degrees(np.arccos(np.clip(np.sum(n * ref, axis=0), -1, 1)))
    assert angle.max() < 0.1

    # Rays hitting the sphere on both sides of the grid plane x = 32 / 63
    o[0, :100] = 32 / 63 - 1e-5
    o[0, 100:] = 32 / 63 + 1e-5
    o[1, 100:] = o[1, :100]
    n = trace(o)
    assert np.linalg.norm(n[:, :100] - n[:, 100:], axis=0).max() < 2e-4


def test18_half_precision(variants_all_rgb):
    # Half precision grids give the same hits as single precision ones
    pytest.importorskip("numpy")
    import numpy as np

    res = 32
    t = np.linspace(0, 1, res)
    z, y, x = np.meshgrid(t, t, t, indexing='ij')
    f = (np.sqrt((x - 0.5)**2 + (y - 0.5)**2 + (z - 0.5)**2) - 0.45).astype(np.float32)
    grid = mi.TensorXf(f.reshape(res, res, res, 1))

    scene_f32 = mi.load_dict({'type': 'scene', 'sdf': {'type': 'sdfgrid', 'grid': grid}})
    scene_f16 = mi.load_dict({'type': 'scene', 'sdf': {'type': 'sdfgrid', 'grid': mi.TensorXf16(grid)}})
    assert type(mi.traverse(scene_f16)['sdf.grid']) is mi.TensorXf16

    rng = np.random.default_rng(0)
    for i in range(50):
        o = rng.normal(size=3)
        o = 0.5 + 1.5 * o / np.linalg.norm(o)
        d = rng.uniform(0, 1, 3) - o
        ray = mi.Ray3f(mi.Point3f(*o.tolist()), mi.Vector3f(*(d / np.linalg.norm(d)).tolist()))
        si_f32 = scene_f32.ray_intersect(ray)
        si_f16 = scene_f16.ray_intersect(ray)
        assert dr.all(si_f32.is_valid() == si_f16.is_valid())
        if dr.all(si_f32.is_valid()):
            assert dr.allclose(si_f32.t, si_f16.t, atol=1e-3)
            assert dr.allclose(si_f32.sh_frame.n, si_f16.sh_frame.n, atol=1e-3)


def test19_half_precision_gradients(variants_all_ad_rgb):
    # A half precision grid remains differentiable: shifting the SDF of a
    # diagonal plane moves the hit point along the ray
    grid = mi.TensorXf16([-1, -1, 0, 0, 0, 0, 1, 1], shape=(2, 2, 2, 1))
    scene = mi.load_dict({'type': 'scene', 'sdf': {'type': 'sdfgrid', 'grid': grid}})
    params = mi.traverse(scene)

    theta = mi.Float16(0)
    dr.enable_grad(theta)
    params['sdf.grid'] = params['sdf.grid'] - theta
    params.update()

    ray = mi.Ray3f(mi.Point3f(0.5, 0.5, 2), mi.Vector3f(0, 0, -1))
    si = scene.ray_intersect(ray)
    dr.forward(theta)
    # SDF gradient along the ray direction is -1, so dt/dtheta = -1
    assert dr.allclose(dr.grad(si.t), -1, atol=1e-2)
