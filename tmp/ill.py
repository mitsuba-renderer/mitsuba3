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

    generate_ref = False
    generate_path = False
    generate_prb = False
    generate_prb_2 = False
    generate_prb_basic = True 
    generate_direct_py = True
    generate_prb_basic2 = False
    generate_prb_threepoint = False
    generate_prb_projective_fix = False
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
            'hide_emitters': True,
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
            'to_world': T().translate([0, 0, -1]) @ T().scale(0.6),
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
        'light2': {
            'type': 'constant',
            'radiance': 0.1
        },
        'sensor': {
            'type': 'perspective',
            'to_world': T().look_at(origin=[2, 2, 1], target=[0, 0, -2], up=[0, 1, 0]),
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
                'pixel_format': 'rgba',
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
scene.sensors()[0].film().bitmap().write(f'{folder_out}/setup_real.exr')


