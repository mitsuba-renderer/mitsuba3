import mitsuba
import pytest
import enoki as ek

from mitsuba.python.test.util import fresolver_append_path

@fresolver_append_path
def make_sphere_mesh_scene():
    return mitsuba.core.load_dict({
        'type' : 'scene',
        'mesh' : {
            'type' : 'obj',
            'filename' : 'resources/data/common/meshes/sphere.obj'
        }
    })

@fresolver_append_path
def make_rectangle_mesh_scene():
    return mitsuba.core.load_dict({
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
    from mitsuba.core import load_dict, Float, Point3f, Vector3f, Ray3f, Transform4f
    from mitsuba.python.util import traverse
    from mitsuba.python.ad import reparameterize_ray

    num_rays = 32
    exponent = 3.0
    kappa = 1e6

    ray = Ray3f(ray_o, ray_d, 0.0, [])

    if shape == 'rectangle':
        scene = make_rectangle_mesh_scene()
    else:
        scene = make_sphere_mesh_scene()

    rng = mitsuba.core.PCG32()

    params = traverse(scene)
    key = 'mesh.vertex_positions'
    params.keep([key])

    init_vertex_pos = ek.unravel(Point3f, params[key])

    theta = Float(0.0)
    ek.enable_grad(theta)
    trans = Vector3f([1.0, 0.0, 0.0])
    transform = Transform4f.translate(theta * trans)
    positions_new = transform @ init_vertex_pos
    params[key] = ek.ravel(positions_new)
    params.update()
    ek.forward(theta, ek.ADFlag.ClearEdges)

    ek.set_label(theta, 'theta')
    ek.set_label(params, 'params')

    d, det = reparameterize_ray(
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

    ek.set_label(d, 'd')
    ek.set_label(det, 'det')

    # print(ek.graphviz_str(Float(1)))

    ek.enqueue(ek.ADMode.Forward, params)
    ek.traverse(mitsuba.core.Float, ek.ADMode.Forward)

    grad_d = ek.grad(d)

    # Compute attached ray direction if the shape moves along the translation axis
    si_p = scene.ray_intersect(ray).p
    new_d = ek.normalize((si_p + trans - ray.o))

    # Gradient should match direction difference along the translation axis
    assert ek.allclose(ek.dot(new_d - ray.d, trans), ek.dot(grad_d, trans), atol=1e-2)

    # Other dimensions should be insignificant
    assert ek.all(ek.dot(grad_d, 1-trans) < 1e-4)


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
    from mitsuba.core import Float, Ray3f, Vector3f
    from mitsuba.python.util import traverse
    from mitsuba.python.ad import reparameterize_ray

    # ek.set_flag(ek.JitFlag.LoopRecord, False)

    grad_direction = Vector3f(1, 0, 0)
    grad_detergence = 0.0
    n_passes = 4
    num_rays = 16
    kappa = 1e6
    exponent = 3.0

    scene = make_rectangle_mesh_scene()

    rng = mitsuba.core.PCG32()

    params = traverse(scene)
    key = 'mesh.vertex_positions'
    params.keep([key])

    ray = Ray3f(ray_o, ray_d, 0.0, [])
    ek.set_label(ray, 'ray')

    res_grad = 0.0
    for i in range(n_passes):
        ek.enable_grad(params[key])
        ek.set_grad(params[key], 0.0)
        ek.set_label(params[key], key)
        params.set_dirty(key)
        params.update()
        ek.eval()

        d, det = reparameterize_ray(
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

        ek.set_label(d, 'd')
        ek.set_label(det, 'det')

        # print(ek.graphviz_str(Float(1)))

        ek.set_grad(d, grad_direction)
        ek.set_grad(det, grad_detergence)
        ek.enqueue(ek.ADMode.Backward, d, det)
        ek.traverse(Float, ek.ADMode.Backward, ek.ADFlag.ClearVertices)

        res_grad += ek.unravel(Vector3f, ek.grad(params[key]))

    assert ek.allclose(res_grad / float(n_passes), ref, atol=atol)


if __name__ == '__main__':
    """
    Helper script to plot shape parameter gradients
    """

    mitsuba.set_variant('llvm_ad_rgb')

    from mitsuba.core import load_dict, Float,  Point3f, Vector3f, Transform4f, ScalarTransform4f, Bitmap, Struct, Color3f
    from mitsuba.python.util import traverse
    from mitsuba.python.ad import reparameterize_ray
    from mitsuba.python.ad.integrators.integrator import sample_sensor_rays

    ek.set_flag(ek.JitFlag.LoopRecord, False)

    res = 128
    spp = 2048
    # scene = make_sphere_mesh_scene()
    scene = make_rectangle_mesh_scene()

    camera = load_dict({
        "type" : "perspective",
        "near_clip": 0.1,
        "far_clip": 1000.0,
        "to_world" : ScalarTransform4f.look_at(origin=[0, 0, -5],
                                               target=[0, 0, 0],
                                               up=[0, 1, 0]),
        "film" : {
            "type" : "hdrfilm",
            "rfilter" : { "type" : "box"},
            "width" : res,
            "height" : res,
        }
    })
    rng = mitsuba.core.PCG32(size=res*res*spp)

    rays, weight, pos, pos_idx, _ = sample_sensor_rays(camera)

    params = traverse(scene)
    key = 'mesh.vertex_positions'
    params.keep([key])

    trans = Vector3f([1.0, 0.0, 0.0])
    init_vertex_pos = ek.unravel(Point3f, params[key])

    theta = Float(0.0)
    ek.enable_grad(theta)
    ek.set_label(theta, 'theta')

    transform = Transform4f.translate(theta * trans)
    positions_new = transform @ init_vertex_pos
    params[key] = ek.ravel(positions_new)
    params.update()
    ek.eval()

    ek.forward(theta, ek.ADFlag.ClearEdges)

    num_rays = 64
    exponent = 3.0
    kappa = 1e5

    d, det = reparameterize_ray(
        scene=scene,
        rng=rng,
        ray=ray,
        params=params,
        active=True,
        num_rays=num_rays,
        kappa=kappa,
        exponent=exponent
    )

    ek.set_label(d, 'd')
    ek.set_label(det, 'det')

    # print(ek.graphviz_str(Float(1)))

    ek.enqueue(ek.ADMode.Forward, params)
    ek.traverse(Float, ek.ADMode.Forward, ek.ADFlag.ClearEdges | ek.ADFlag.ClearInterior)

    grad_d = ek.grad(d)
    grad_det = ek.grad(det)

    block = mitsuba.render.ImageBlock(
        [res, res],
        [0, 0],
        channel_count=5,
        filter=camera.film().rfilter(),
        border=False
    )
    block.put(pos, rays.wavelengths, grad_d, 1)
    bmp_grad_d = Bitmap(block.data(), Bitmap.PixelFormat.RGBAW)
    bmp_grad_d = bmp_grad_d.convert(Bitmap.PixelFormat.RGB, Struct.Type.Float32, srgb_gamma=False)
    bmp_grad_d.write('output_grad_d.exr')

    block.clear()
    block.put(pos, rays.wavelengths, Color3f(grad_det), 1)
    bmp_grad_det = Bitmap(block.data(), Bitmap.PixelFormat.RGBAW)
    bmp_grad_det = bmp_grad_det.convert(Bitmap.PixelFormat.Y, Struct.Type.Float32, srgb_gamma=False)
    bmp_grad_det.write('output_grad_det.exr')
