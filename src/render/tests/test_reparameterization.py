import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path

@fresolver_append_path
def make_sphere_mesh_scene():
    return mi.load_dict({
        'type' : 'scene',
        'mesh' : {
            'type' : 'obj',
            'filename' : 'resources/data/common/meshes/sphere.obj'
        }
    })

@fresolver_append_path
def make_rectangle_mesh_scene():
    return mi.load_dict({
        'type' : 'scene',
        'mesh' : {
            'type' : 'obj',
            'filename' : 'resources/data/common/meshes/rectangle.obj',
            'face_normals': True
        }
    })


@pytest.mark.parametrize("shape, ray_o, ray_d", [
    ('rectangle', [0, 1, -5], [0, 0, 1]),       # Target one side of the rectangle
    ('rectangle', [0, 0.0, -5], [0, 0, 1]),     # Target center of the rectangle
    ('rectangle', [0.99, -0.99, -5], [0, 0, 1]) # Target one corner of the rectangle
])
def test01_reparameterization_forward(variants_all_ad_rgb, shape, ray_o, ray_d):
    num_rays = 32
    exponent = 3.0
    kappa = 1e6

    ray = mi.Ray3f(ray_o, ray_d, 0.0, [])

    if shape == 'rectangle':
        scene = make_rectangle_mesh_scene()
    else:
        scene = make_sphere_mesh_scene()

    rng = mi.PCG32()

    params = mi.traverse(scene)
    key = 'mesh.vertex_positions'
    params.keep([key])

    init_vertex_pos = dr.unravel(mi.Point3f, params[key])

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    trans = mi.Vector3f([1.0, 0.0, 0.0])
    transform = mi.Transform4f.translate(theta * trans)
    positions_new = transform @ init_vertex_pos
    params[key] = dr.ravel(positions_new)
    params.update()
    dr.forward(theta, dr.ADFlag.ClearEdges)

    dr.set_label(theta, 'theta')
    dr.set_label(params, 'params')

    d, det = mi.ad.reparameterize_ray(
        scene=scene,
        rng=rng,
        ray=ray,
        params=params,
        active=True,
        num_rays=num_rays,
        kappa=kappa,
        exponent=exponent
    )

    assert d == ray.d
    assert det == 1.0

    dr.set_label(d, 'd')
    dr.set_label(det, 'det')

    # print(dr.graphviz_str(Float(1)))

    dr.enqueue(dr.ADMode.Forward, params)
    dr.traverse(mi.Float, dr.ADMode.Forward)

    grad_d = dr.grad(d)

    # Compute attached ray direction if the shape moves along the translation axis
    si_p = scene.ray_intersect(ray).p
    new_d = dr.normalize((si_p + trans - ray.o))

    # Gradient should match direction difference along the translation axis
    assert dr.allclose(dr.dot(new_d - ray.d, trans), dr.dot(grad_d, trans), atol=1e-2)

    # Other dimensions should be insignificant
    assert dr.all(dr.dot(grad_d, 1-trans) < 1e-4)


@pytest.mark.parametrize("ray_o, ray_d, ref, atol", [
    # Hit the center of the triangle, only the vertices of the diagonal should move
    ([0, 0, -1], [0, 0, 1], [[0.5, 0.5, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]], 1e-3),
    # Hit corner of the triangle, only that vertex should move
    ([1, 1, -1], [0, 0, 1], [[0, 0, 0, 1], [0, 0, 0, 0], [0, 0, 0, 0]], 1e-1),
    # Hit side of the triangle, only those vertices should move
    ([1, 0, -1], [0, 0, 1], [[0.5, 0, 0, 0.5], [0, 0, 0, 0], [0, 0, 0, 0]], 1e-2),
])
@pytest.mark.parametrize("unroll", [False, True])
def test02_reparameterization_backward_direction_gradient(variants_all_ad_rgb, ray_o, ray_d, ref, atol, unroll):
    # dr.set_flag(dr.JitFlag.LoopRecord, False)

    grad_direction = mi.Vector3f(1, 0, 0)
    grad_detergence = 0.0
    n_passes = 4
    num_rays = 16
    kappa = 1e6
    exponent = 3.0

    scene = make_rectangle_mesh_scene()

    rng = mi.PCG32()

    params = mi.traverse(scene)
    key = 'mesh.vertex_positions'
    params.keep([key])

    ray = mi.Ray3f(ray_o, ray_d, 0.0, [])
    dr.set_label(ray, 'ray')

    res_grad = 0.0
    for i in range(n_passes):
        dr.enable_grad(params[key])
        dr.set_grad(params[key], 0.0)
        dr.set_label(params[key], key)
        params.set_dirty(key)
        params.update()
        dr.eval()

        d, det = mi.ad.reparameterize_ray(
            scene=scene,
            rng=rng,
            ray=ray,
            params=params,
            active=True,
            num_rays=num_rays,
            kappa=kappa,
            exponent=exponent,
            unroll=unroll,
        )

        dr.set_label(d, 'd')
        dr.set_label(det, 'det')

        # print(dr.graphviz_str(Float(1)))

        dr.set_grad(d, grad_direction)
        dr.set_grad(det, grad_detergence)
        dr.enqueue(dr.ADMode.Backward, d, det)
        dr.traverse(mi.Float, dr.ADMode.Backward, dr.ADFlag.ClearVertices)

        res_grad += dr.unravel(mi.Vector3f, dr.grad(params[key]))

    assert dr.allclose(res_grad / float(n_passes), ref, atol=atol)


if __name__ == '__main__':
    """
    Helper script to plot shape parameter gradients
    """

    mi.set_variant('llvm_ad_rgb')

    from mitsuba.ad.integrators.integrator import sample_sensor_rays

    dr.set_flag(dr.JitFlag.LoopRecord, False)

    res = 128
    spp = 2048
    # scene = make_sphere_mesh_scene()
    scene = make_rectangle_mesh_scene()

    camera = mi.load_dict({
        "type" : "perspective",
        "near_clip": 0.1,
        "far_clip": 1000.0,
        "to_world" : mi.ScalarTransform4f.look_at(origin=[0, 0, -5],
                                               target=[0, 0, 0],
                                               up=[0, 1, 0]),
        "film" : {
            "type" : "hdrfilm",
            "rfilter" : { "type" : "box"},
            "width" : res,
            "height" : res,
        }
    })
    rng = mi.PCG32(size=res*res*spp)

    rays, weight, pos, pos_idx, _ = sample_sensor_rays(camera)

    params = mi.traverse(scene)
    key = 'mesh.vertex_positions'
    params.keep([key])

    trans = mi.Vector3f([1.0, 0.0, 0.0])
    init_vertex_pos = dr.unravel(mi.Point3f, params[key])

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    dr.set_label(theta, 'theta')

    transform = mi.Transform4f.translate(theta * trans)
    positions_new = transform @ init_vertex_pos
    params[key] = dr.ravel(positions_new)
    params.update()
    dr.eval()

    dr.forward(theta, dr.ADFlag.ClearEdges)

    num_rays = 64
    exponent = 3.0
    kappa = 1e5

    d, det = mi.ad.reparameterize_ray(
        scene=scene,
        rng=rng,
        ray=ray,
        params=params,
        active=True,
        num_rays=num_rays,
        kappa=kappa,
        exponent=exponent
    )

    dr.set_label(d, 'd')
    dr.set_label(det, 'det')

    # print(dr.graphviz_str(Float(1)))

    dr.enqueue(dr.ADMode.Forward, params)
    dr.traverse(mi.Float, dr.ADMode.Forward, dr.ADFlag.ClearEdges | dr.ADFlag.ClearInterior)

    grad_d = dr.grad(d)
    grad_det = dr.grad(det)

    block = mi.ImageBlock(
        [res, res],
        [0, 0],
        channel_count=5,
        filter=camera.film().rfilter(),
        border=False
    )
    block.put(pos, rays.wavelengths, grad_d, 1)
    bmp_grad_d = mi.Bitmap(block.data(), mi.Bitmap.PixelFormat.RGBAW)
    bmp_grad_d = bmp_grad_d.convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.Float32, srgb_gamma=False)
    bmp_grad_d.write('output_grad_d.exr')

    block.clear()
    block.put(pos, rays.wavelengths, mi.Color3f(grad_det), 1)
    bmp_grad_det = mi.Bitmap(block.data(), mi.Bitmap.PixelFormat.RGBAW)
    bmp_grad_det = bmp_grad_det.convert(mi.Bitmap.PixelFormat.Y, mi.Struct.Type.Float32, srgb_gamma=False)
    bmp_grad_det.write('output_grad_det.exr')
