import mitsuba as mi
import drjit as dr
import os
from mitsuba.scalar_rgb import ScalarTransform4f as T
import time

mi.set_variant('cuda_ad_rgb')
# dr.set_flag(dr.JitFlag.ShaderExecutionReordering, False)
#dr.set_flag(dr.JitFlag.SymbolicCalls, False)
#dr.set_flag(dr.JitFlag.SymbolicLoops, False)


EXP_NAME = 'lens'
SHAPE = 'plane'
# EXP_NAME = 'cbox'
# SHAPE = 'cube'
assert EXP_NAME in ['lens', 'cbox'], "Invalid experiment name."

if EXP_NAME == 'lens':
    assert SHAPE in ['sphere', 'plane'], "Invalid shape name."
    MATE = ""

    generate_ref = True
    generate_path = False
    generate_prb = True
    generate_prb_2 = False
    generate_prb_basic = True 
    generate_direct_py = True
    generate_direct_projective = True
    generate_prb_basic2 = False
    generate_prb_threepoint = False
    generate_prb_projective_fix = True
    with_disc = False

    folder_out = f'output/{EXP_NAME}_{SHAPE}'
    is_mesh = True
    max_depth = 4
elif EXP_NAME == 'cbox':
    assert SHAPE in ['sphere', 'cube'], "Invalid shape name."

    MATE = 'principled'
    assert MATE in ['diffuse', 'principled'], "Invalid material type."

    generate_ref = True
    generate_path = False
    generate_prb = False
    generate_prb_2 = True
    generate_prb_basic = False
    generate_prb_basic2 = False
    generate_prb_threepoint = False
    generate_prb_projective_fix = True
    with_disc = True

    folder_out = f'output/{EXP_NAME}_{SHAPE}_{MATE}'
    is_mesh = True
    max_depth = 3

if not os.path.exists(folder_out):
    os.makedirs(folder_out)

print(f"Experiment: {EXP_NAME}, Shape: {SHAPE}, Material: {MATE}")

res = 128
spp_grad = 2 ** 16

if EXP_NAME == 'cbox':
    scene_description = {
        'type': 'scene',
        'integrator': {
            'type': 'path',
            'max_depth': max_depth
        },
        # -------------------- Sensor --------------------
        'sensor': {
            'type': 'perspective',
            'fov_axis': 'smaller',
            'near_clip': 0.001,
            'far_clip': 100.0,
            'focus_distance': 1000,
            'fov': 39.3077,
            'to_world': T().look_at(
                origin=[0, 0, 3.90],
                target=[0, 0, 0],
                up=[0, 1, 0]
            ),
            'sampler': {
                'type': 'independent',
                'sample_count': 64
            },
            'film': {
                'type': 'hdrfilm',
                'width' : res,
                'height': res,
                'rfilter': {
                    'type': 'box',
                },
                'pixel_format': 'rgb',
                'component_format': 'float32',
            }
        },
        # -------------------- BSDFs --------------------
        'white': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [0.885809, 0.698859, 0.666422],
            }
        },
        'green': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [0.105421, 0.37798, 0.076425],
            }
        },
        'red': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [0.570068, 0.0430135, 0.0443706],
            }
        },
        # -------------------- Light --------------------
        'light': {
            'type': 'rectangle',
            'to_world': T().translate([0.0, 0.99, 0.01]).rotate([1, 0, 0], 90).scale([0.23, 0.19, 0.19]),
            'bsdf': {
                'type': 'ref',
                'id': 'white'
            },
            'emitter': {
                'type': 'area',
                'radiance': {
                    'type': 'rgb',
                    'value': [18.387, 13.9873, 6.75357],
                }
            }
        },
        # -------------------- Shapes --------------------
        'floor': {
            'type': 'rectangle',
            'to_world': T().translate([0.0, -1.0, 0.0]).rotate([1, 0, 0], -90),
            'bsdf': {
                'type': 'ref',
                'id':  'white'
            }
        },
        'ceiling': {
            'type': 'rectangle',
            'to_world': T().translate([0.0, 1.0, 0.0]).rotate([1, 0, 0], 90),
            'bsdf': {
                'type': 'ref',
                'id':  'white'
            }
        },
        'back': {
            'type': 'rectangle',
            'to_world': T().translate([0.0, 0.0, -1.0]),
            'bsdf': {
                'type': 'ref',
                'id':  'white'
            }
        },
        'green-wall': {
            'type': 'rectangle',
            'to_world': T().translate([1.0, 0.0, 0.0]).rotate([0, 1, 0], -90),
            'bsdf': {
                'type': 'ref',
                'id':  'green'
            }
        },
        'red-wall': {
            'type': 'rectangle',
            'to_world': T().translate([-1.0, 0.0, 0.0]).rotate([0, 1, 0], 90),
            'bsdf': {
                'type': 'ref',
                'id':  'red'
            }
        },
        'shape_bsdf': {
                'type': 'roughconductor',
                'alpha': 0.5,
            },
        'shape': {
            'type': 'obj',
            'filename': 'sphere.obj',
            'bsdf': {
                'type': 'ref',
                'id': 'shape_bsdf',
            },
            'to_world': T().translate([0.0, 0.0, 0.0]).scale([0.4, 0.4, 0.4]),
        }
    }
    if SHAPE == 'cube':
        scene_description['shape'] = {
            'type': 'cube',
            'bsdf': {
                'type': 'ref',
                'id': 'shape_bsdf',
            },
            'to_world': T().rotate([0.5, 0.5, 0.5], 50).scale([0.4, 0.4, 0.4]),
        }
    if MATE == 'principled':
        scene_description['white'] = {
            'type': 'principled',
            'base_color': {
                'type': 'rgb',
                'value': [0.885809, 0.698859, 0.666422],
            },
            'metallic': 0.7,
            'roughness': 0.1,
        }
        scene_description['green'] = {
            'type': 'principled',
            'base_color': {
                'type': 'rgb',
                'value': [0.105421, 0.37798, 0.076425],
            },
            'metallic': 0.7,
            'roughness': 0.1,
        }
        scene_description['red'] = {
            'type': 'principled',
            'base_color': {
                'type': 'rgb',
                'value': [0.570068, 0.0430135, 0.0443706],
            },
            'metallic': 0.7,
            'roughness': 0.1,
        }
elif EXP_NAME == 'lens':
    scene_description = {
        'type': 'scene',
        'integrator': {
            'type': 'path',
            'max_depth': max_depth,
        },
        'rough_glass': {
            'type': 'roughdielectric',
            'distribution': 'beckmann',
            'alpha': 0.8,
            'int_ior': 1.001,
            'ext_ior': 1.1,
        },
        # 'glass1': {
        #     'type': 'obj',
        #     'filename': 'resources/data/common/meshes/rectangle.obj',
        #     'face_normals': True,
        #     'to_world': T().translate([0, 0, 1]) @ T().scale(4.0),
        #     'bsdf': {
        #         'type': 'ref',
        #         'id': 'rough_glass',
        #     }
        # },
        # 'glass2': {
        #     'type': 'obj',
        #     'filename': 'resources/data/common/meshes/rectangle.obj',
        #     'face_normals': True,
        #     'to_world': T().translate([0, 0, 0]) @ T().scale(4.0),
        #     'bsdf': {
        #         'type': 'ref',
        #         'id': 'rough_glass',
        #     }
        # },
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
                    'filename': '../resources/data/common/textures/gradient_bi.jpg',
                    'wrap_mode': 'clamp',
                }
            }
        },
        'sensor': {
            'type': 'perspective',
            'to_world': T().look_at(origin=[0, 0, 2], target=[0, 0, 0], up=[0, 1, 0]),
            'fov': 40,
            'film': {
                'type': 'hdrfilm',
                'rfilter': { 'type': 'box' },
                'width': res,
                'height': res,
                #'crop_offset_x': 49,
                #'crop_offset_y': 61,
                #'crop_width': 2,
                #'crop_height': 2,
                'sample_border': True,
                'pixel_format': 'rgb',
                'component_format': 'float32',
            }
        }
    }
    if SHAPE == 'sphere':
        assert False

class kernel_timer:
    def __init__(self):
        self.start_time = time.time()
        self.disc_time = 0.0
        self.disc_kernels_time = 0.0

    def start(self, name):
        self.name = name
        dr.kernel_history_clear()
        self.start_time = time.time()

    def stop(self, is_disc=False, with_disc=False):
        assert not (is_disc and with_disc)
        dr.eval()
        dr.sync_thread()
        hist = dr.kernel_history()
        kernel_time = sum(
            item.get('execution_time', 0.0) for item in hist)
        elapsed_time = time.time() - self.start_time
        if with_disc:
            elapsed_time += self.disc_time
            kernel_time += self.disc_kernels_time
        print(f"{self.name} took {elapsed_time:.2f} seconds," +
              f" {kernel_time / 1e3:.2f} seconds in kernels.")

        if is_disc:
            self.disc_time = elapsed_time
            self.disc_kernels_time = kernel_time


scene = mi.load_dict(scene_description, optimize=False)

primal = mi.render(scene, spp=8196)
mi.Bitmap(primal).write(f'{folder_out}/primal.exr')

params = mi.traverse(scene)
if not is_mesh:
    key = 'shape.to_world'
    initial_params = mi.Transform4f(params[key])
else:
    key1 = 'light.vertex_positions'
    initial_params1 = dr.unravel(mi.Vector3f, params[key1])
    key2 = 'glass3.vertex_positions'
    initial_params2 = dr.unravel(mi.Vector3f, params[key2])

def update(theta):
    if is_mesh:
        params[key1] = dr.ravel(initial_params1 + mi.Vector3f(theta))
        params[key2] = dr.ravel(initial_params2 + mi.Vector3f(-theta))
    else:
        params[key] = mi.Transform4f().translate(mi.Vector3f(0.0, 0.0, theta)) @ initial_params
    params.update()
    dr.eval()

res = scene.sensors()[0].film().size()
image_1 = dr.zeros(mi.TensorXf, (res[1], res[0], 3))
image_2 = dr.zeros(mi.TensorXf, (res[1], res[0], 3))

if generate_ref:
    epsilon = 1e-5
    N = 1 if EXP_NAME == 'lens' else 8
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

    mi.util.write_bitmap(f"{folder_out}/fd.exr", image_fd)


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

direct_py_integrator = mi.load_dict({
    'type': 'direct_py'
})

direct_projective_integrator = mi.load_dict({
    'type': 'direct_projective',
    'sppi': 0,
    'sppc': 128,
    'sppp': 0,
})

prb_2_integrator = mi.load_dict({
    'type': 'prb',
    'max_depth': max_depth
})

prb_basic2_integrator = mi.load_dict({
    'type': 'prb_basic',
    'max_depth': max_depth
})

prb_threepoint = mi.load_dict({
    'type': 'prb',
    'max_depth': max_depth,
})

sppc = 0
sppp = 512
sppi = 2048
prb_projective_fix = mi.load_dict({
    'type': 'prb_projective',
    'max_depth': max_depth,
    'sppi': 0,
    'sppc': 128,
    'sppp': 0,
})
prb_projective_fix.sample_border_warning = False

projective_integrator = mi.load_dict({
    'type': 'prb_projective',
    'max_depth': max_depth,
    'sppi': sppi,
    'sppc': sppc,
    'sppp': sppp,
})
# projective_integrator.scale_mass = 0.5
projective_integrator.sample_border_warning = False

dr.set_flag(dr.JitFlag.KernelHistory, True)
timer = kernel_timer()

if with_disc:
    timer.start("disc_fwd")
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    disc_fwd = projective_integrator.render_forward(
        scene, params=params
    )
    mi.Bitmap(disc_fwd).write(f'{folder_out}/disc_fwd.exr')
    timer.stop(is_disc=True)

if generate_path:
    timer.start("path_fwd")
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    path_fwd = path_integrator.render_forward(
        scene, params=params, spp=spp_grad
    )
    if with_disc:
        path_fwd += disc_fwd
    mi.Bitmap(path_fwd).write(f'{folder_out}/path_fwd.exr')
    timer.stop(with_disc=with_disc)

if generate_prb:
    timer.start("prb_fwd")
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    prb_fwd = prb_integrator.render_forward(
        scene, params=params, spp=spp_grad
    )
    mi.Bitmap(prb_fwd).write(f'{folder_out}/prb_fwd.exr')
    timer.stop()

if generate_direct_py:
    timer.start("direct_py_fwd")
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    direct_py_fwd = direct_py_integrator.render_forward(
        scene, params=params, spp=spp_grad
    )
    mi.Bitmap(direct_py_fwd).write(f'{folder_out}/direct_py_fwd.exr')
    timer.stop()

    direct_py_primal = direct_py_integrator.render(scene, spp=spp_grad)
    mi.Bitmap(direct_py_primal).write(f'{folder_out}/direct_py_primal.exr')
    direct_primal = mi.load_dict({'type': 'direct', 'bsdf_samples': 0, 'emitter_samples': 1}).render(scene, spp=spp_grad)
    mi.Bitmap(direct_primal).write(f'{folder_out}/direct_emitter_sampling_primal.exr')

    direct_primal = mi.load_dict({'type': 'direct', 'bsdf_samples': 1, 'emitter_samples': 0}).render(scene, spp=spp_grad)
    mi.Bitmap(direct_primal).write(f'{folder_out}/direct_bsdf_sampling_primal.exr')

if generate_direct_projective:
    timer.start("direct_projective_fwd")
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    direct_projective_fwd = direct_projective_integrator.render_forward(
        scene, params=params, spp=spp_grad
    )
    mi.Bitmap(direct_projective_fwd).write(f'{folder_out}/direct_projective_fwd.exr')
    timer.stop()

if generate_prb_basic:
    timer.start("prb_basic_fwd")
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    path_basic_fwd = prb_basic_integrator.render_forward(
        scene, params=params, spp=spp_grad
    )
    if with_disc:
        path_basic_fwd += disc_fwd
    mi.Bitmap(path_basic_fwd).write(f'{folder_out}/prb_basic_fwd.exr')
    timer.stop(with_disc=with_disc)

if generate_prb_basic2:
    timer.start("prb_basic2_fwd")
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    prb_basic2_fwd = prb_basic2_integrator.render_forward(
        scene, params=params, spp=spp_grad
    )
    if with_disc:
        prb_basic2_fwd += disc_fwd
    mi.Bitmap(prb_basic2_fwd).write(f'{folder_out}/prb_basic_2_fwd.exr')
    timer.stop(with_disc=with_disc)

if generate_prb_2:
    timer.start("prb_2_fwd")
    repeat = 1
    prb_2_fwd = dr.zeros(mi.TensorXf, (res[1], res[0], 3))
    for it in range(repeat):
        update(0)
        theta = mi.Float(0)
        dr.enable_grad(theta)
        update(theta)
        dr.forward(theta, flags=dr.ADFlag.ClearEdges)

        prb_2_fwd += prb_2_integrator.render_forward(
            scene, params=params, spp=spp_grad, seed=it
        )
        dr.eval(prb_2_fwd)
    prb_2_fwd /= repeat
    if with_disc:
        prb_2_fwd += disc_fwd
    dr.eval(prb_2_fwd)
    mi.Bitmap(prb_2_fwd).write(f'{folder_out}/prb_2.exr')
    timer.stop(with_disc=with_disc)

if generate_prb_threepoint:
    timer.start("prb_threepoint_fwd")
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    threepoint_fwd = prb_threepoint.render_forward(
        scene, params=params, spp=spp_grad
    )
    mi.Bitmap(threepoint_fwd).write(f'{folder_out}/threepoint_fwd.exr')
    dr.eval(threepoint_fwd)
    timer.stop(with_disc=False)

if generate_prb_projective_fix:
    timer.start("prb_projective_fix_fwd")
    update(0)
    theta = mi.Float(0)
    dr.enable_grad(theta)
    update(theta)
    dr.forward(theta, flags=dr.ADFlag.ClearEdges)

    prb_threepoint_fwd = prb_projective_fix.render_forward(
        scene, params=params, spp=spp_grad
    )
    dr.eval(prb_threepoint_fwd)
    mi.Bitmap(prb_threepoint_fwd).write(f'{folder_out}/prb_projective_fix_fwd.exr')
    timer.stop(with_disc=False)

