import mitsuba as mi
import drjit as dr
import os
from mitsuba.scalar_rgb import ScalarTransform4f as T

mi.set_variant('cuda_ad_rgb')
mi.set_log_level(mi.LogLevel.Info)

generate_ref = True
generate_path = False
generate_prb = False
generate_prb_2 = False
generate_prb_basic = False
generate_prb_basic2 = True
with_disc = False
folder_out = 'lens_test'
if not os.path.exists(folder_out):
    os.makedirs(folder_out)

res = 128
max_depth = 4

scene_description = {
    'type': 'scene',
    'integrator': {
        'type': 'path',
        'max_depth': max_depth,
    },
    'rough_glass': {
        'type': 'roughdielectric',
        'distribution': 'beckmann',
        'alpha': 0.05,
        'int_ior': 1.001,
        'ext_ior': 1.1,
    },
    'glass1': {
        'type': 'obj',
        'filename': '../resources/data/common/meshes/rectangle.obj',
        'face_normals': True,
        'to_world': T().translate([0, 0, 1]) @ T().scale(4.0),
        'bsdf': {
            'type': 'ref',
            'id': 'rough_glass',
        }
    },
    'glass2': {
        'type': 'obj',
        'filename': '../resources/data/common/meshes/rectangle.obj',
        'face_normals': True,
        'to_world': T().translate([0, 0, 0]) @ T().scale(4.0),
        'bsdf': {
            'type': 'ref',
            'id': 'rough_glass',
        }
    },
    'glass3': {
        'type': 'obj',
        'filename': '../resources/data/common/meshes/rectangle.obj',
        'face_normals': True,
        'to_world': T().translate([0, 0, -1]) @ T().scale(4.0),
        'bsdf': {
            'type': 'ref',
            'id': 'rough_glass',
        }
    },
    'light': {
        'type': 'obj',
        'filename': '../resources/data/common/meshes/rectangle.obj',
        'face_normals': True,
        'to_world': T().translate([0, 0, -2]) @ T().scale(0.7),  # 0.7
        'emitter': {
            'type': 'area',
            'radiance': {
                'type': 'bitmap',
                'filename': 'gradient_lowres.jpg',
                'wrap_mode': 'clamp',
            }
        }
    },
    # 'envmap': {
    #     'type': 'envmap',
    #     'filename': '../resources/data/common/textures/museum.exr',
    # },
    'sensor': {
        'type': 'perspective',
        'to_world': T().look_at(origin=[0, 0, 2], target=[0, 0, 0], up=[0, 1, 0]),
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

# del scene_description['glass1']
# del scene_description['glass2']
# del scene_description['glass3']

scene = mi.load_dict(scene_description)

primal = mi.render(scene, spp=1024)
mi.Bitmap(primal).write(f'{folder_out}/primal.exr')

params = mi.traverse(scene)
key = 'glass2.vertex_positions'
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

    image_fd = (image_2 - image_1) / epsilon / N

    mi.Bitmap(image_1 / N).write('primal_fd_1.exr')
    mi.Bitmap(image_2 / N).write('primal_fd_2.exr')

    mi.util.write_bitmap(f"{folder_out}/lens_fd.exr", image_fd)


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
    mi.Bitmap(disc_fwd).write(f'{folder_out}/lens_disc_fwd.exr')

if generate_path:
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    path_fwd = path_integrator.render_forward(scene, params=params, spp=2048)
    if with_disc:
        path_fwd += disc_fwd
    mi.Bitmap(path_fwd).write(f'{folder_out}/lens_path_fwd.exr')

if generate_prb :
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    prb_fwd = prb_integrator.render_forward(scene, params=params, spp=1024)
    mi.Bitmap(prb_fwd).write(f'{folder_out}/lens_prb_fwd.exr')

if generate_prb_basic:
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    path_basic_fwd = prb_basic_integrator.render_forward(scene, params=params, spp=2 ** 16)
    if with_disc:
        path_basic_fwd += disc_fwd
    mi.Bitmap(path_basic_fwd).write(f'{folder_out}/lens_prb_basic_fwd.exr')

if generate_prb_basic2:
    repeat = 1
    prb_basic2_fwd = dr.zeros(mi.TensorXf, (res[1], res[0], 3))
    for i in range(repeat):
        update(0)
        theta = mi.Float(0)
        dr.enable_grad(theta)
        update(theta)
        dr.forward(theta, flags=dr.ADFlag.ClearEdges)

        prb_basic2_fwd += prb_basic2_integrator.render_forward(
            scene, params=params, spp=2 ** 16, seed=i
        )
        dr.eval(prb_basic2_fwd)
        print(f'{(i + 1)}/{repeat}', end='\r')
    prb_basic2_fwd /= repeat
    if with_disc:
        prb_basic2_fwd += disc_fwd
    mi.Bitmap(prb_basic2_fwd).write(f'{folder_out}/lens_prb_basic2_fwd.exr')

if generate_prb_2:
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    prb_2_fwd = prb_2_integrator.render_forward(scene, params=params, spp=2 ** 16)
    if with_disc:
        prb_2_fwd += disc_fwd
    mi.Bitmap(prb_2_fwd).write(f'{folder_out}/lens_prb_2_fwd.exr')
