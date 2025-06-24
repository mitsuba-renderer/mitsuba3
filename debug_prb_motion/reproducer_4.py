import mitsuba as mi
import drjit as dr
from mitsuba.scalar_rgb import ScalarTransform4f as T

mi.set_variant('cuda_ad_rgb')
mi.set_log_level(mi.LogLevel.Info)

generate_ref = True
generate_path = False
generate_prb = False
generate_prb_2 = True
generate_prb_basic = False
generate_prb_basic2 = False
with_disc = False

res = 128
max_depth = 3

scene_description = {
    'type': 'scene',
    'integrator': {
        'type': 'path',
        'max_depth': max_depth,
    },
    'glass': {
        'type': 'obj',
        'filename': '../resources/data/common/meshes/rectangle.obj',
        'face_normals': True,
        'to_world': T().translate([0, 0.2, 3]),
        'bsdf': {
            'type': 'roughdielectric',
            'distribution': 'beckmann',
            'alpha': 0.01,
            'int_ior': 1.001,
            'ext_ior': 1.1,
        }
    },
    'plane': {
        'type': 'obj',
        # If diffuse, works;
        # If rough conductor, does not work --> si_next BSDF not differtiated
        'bsdf': {
            'type': 'roughconductor',
            'distribution': 'ggx',
            'alpha': 0.3,
        },
        'filename': '../resources/data/common/meshes/rectangle.obj',
        'face_normals': True,
        'to_world':  T().rotate([1, 0, 0], 45) @ T().rotate([0, 0, 1], 90) @ T().scale(1),
    },
    'light': {
        'type': 'obj',
        'filename': '../resources/data/common/meshes/rectangle.obj',
        'face_normals': True,
        'to_world': T().translate([0, -1.5, 0]) @ T().rotate([0, 1, 0], -90) @ T().rotate([1, 0, 0], -90) @ T().scale(0.2),
        'emitter': {
            'type': 'area',
            'radiance': {
                'type': 'rgb',
                'value': [100.0, 100.0, 100.0],
            }
        }
    },
    'sensor': {
        'type': 'perspective',
        'to_world': T().look_at(origin=[0, 0.2, 4], target=[0, 0.2, 0], up=[0, 1, 0]),
        'fov': 40,
        'film': {
            'type': 'hdrfilm',
            'rfilter': { 'type': 'box' },
            'width': res,
            'height': res,
            'sample_border': True,
            'pixel_format': 'rgb',
            'component_format': 'float32',
        }
    }
}
scene = mi.load_dict(scene_description)

primal = mi.render(scene, spp=512)
mi.Bitmap(primal).write('primal.exr')

params = mi.traverse(scene)
key = 'glass.vertex_positions'
initial_params = dr.unravel(mi.Vector3f, params[key])

def update(theta):
    params[key] = dr.ravel(initial_params + mi.Vector3f(0.0, 0.0, theta))
    params.update()
    dr.eval()

res = scene.sensors()[0].film().size()
image_1 = dr.zeros(mi.TensorXf, (res[1], res[0], 3))
image_2 = dr.zeros(mi.TensorXf, (res[1], res[0], 3))

if generate_ref:
    epsilon = 1e-3
    N = 1
    for it in range(N):
        theta = mi.Float(-0.5 * epsilon)
        update(theta)
        image_1 += mi.render(scene, spp=46000 * 4, seed=it)
        dr.eval(image_1)

        theta = mi.Float(0.5 * epsilon)
        update(theta)
        image_2 += mi.render(scene, spp=46000 * 4, seed=it)
        dr.eval(image_2)

        print(f"{it+1}/{N}", end='\r')

    mi.Bitmap(image_1 / N).write('fd_1.exr')
    mi.Bitmap(image_2 / N).write('fd_2.exr')

    image_fd = (image_2 - image_1) / epsilon / N

    mi.util.write_bitmap("fd.exr", image_fd)


path_integrator = mi.load_dict({
    'type': 'path',
    'max_depth': max_depth
})

prb_integrator = mi.load_dict({
    'type': 'prb',
    'max_depth': max_depth
})

prb_basic_integrator = mi.load_dict({
    'type': 'prb_basic',
    'max_depth': max_depth
})

prb_2_integrator = mi.load_dict({
    'type': 'prb_2',
    'max_depth': max_depth
})

prb_basic2_integrator = mi.load_dict({
    'type': 'prb_basic_2',
    'max_depth': max_depth
})


direct_projective_integrator = mi.load_dict({
    'type': 'direct_projective',
    'sppi': 2048,
    'sppc': 0,
    'sppp': 512,
})

if with_disc:
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    disc_fwd = direct_projective_integrator.render_forward(scene, params=params)
    mi.Bitmap(disc_fwd).write('disc_fwd.exr')

if generate_path:
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    path_fwd = path_integrator.render_forward(scene, params=params, spp=2048)
    if with_disc:
        path_fwd += disc_fwd
    mi.Bitmap(path_fwd).write('path_fwd.exr')

if generate_prb :
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    prb_fwd = prb_integrator.render_forward(scene, params=params, spp=1024)
    mi.Bitmap(prb_fwd).write('prb_fwd.exr')

if generate_prb_basic:
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    path_basic_fwd = prb_basic_integrator.render_forward(scene, params=params, spp=2 ** 16)
    if with_disc:
        path_basic_fwd += disc_fwd
    mi.Bitmap(path_basic_fwd).write('prb_basic_fwd.exr')

if generate_prb_basic2:
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    prb_basic2_fwd = prb_basic2_integrator.render_forward(scene, params=params, spp=2 ** 16)
    if with_disc:
        prb_basic2_fwd += disc_fwd
    mi.Bitmap(prb_basic2_fwd).write('prb_basic_fwd.exr')

if generate_prb_2:
    repeat = 8
    prb_2_fwd = dr.zeros(mi.TensorXf, (res[1], res[0], 3))
    for it in range(repeat):
        update(0)
        theta = mi.Float(0)
        dr.enable_grad(theta)
        update(theta)
        dr.forward(theta, flags=dr.ADFlag.ClearEdges)

        prb_2_fwd += prb_2_integrator.render_forward(scene, params=params, spp=2 ** 16, seed=it)
        dr.eval(prb_2_fwd)
    prb_2_fwd /= repeat
    if with_disc:
        prb_2_fwd += disc_fwd
    mi.Bitmap(prb_2_fwd).write('prb_2.exr')
