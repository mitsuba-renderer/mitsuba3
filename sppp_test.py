# This is not a mitsuba test file yet

import mitsuba as mi
import drjit as dr
import os

mi.set_variant('cuda_ad_rgb')

T = mi.ScalarTransform4f

reso = 99
folder = 'output'
if not os.path.exists(folder):
    os.makedirs(folder)

deg = 60

scene_dict = {
    'type': 'scene',
    'shape': {
        'type': 'disk',
        'to_world': T().translate([0, 0, 0]) @ T().rotate([0, 1, 0], deg),
        'emitter': {
            'type': 'area',
            'radiance': {
                'type': 'rgb',
                'value': 2.0
            }
        }
    },
    'light': {
        'type': 'constant',
        'radiance': {
            'type': 'rgb',
            'value': 1.0,
        }
    },
    'integrator': {
        'type': 'direct_projective',
        'max_depth': 1,
        'sppc': 0,
        'sppi': 0,
    },
    'sensor': {
        'type': 'perspective',
        'to_world': T().look_at(origin=[0, 0, 1], target=[0, 0, 0], up=[0, 1, 0]),
        'fov': 120,
        'near_clip': 0.01,
        # 'type': 'orthographic',
        'film': {
            'type': 'hdrfilm',
            'rfilter': {'type': 'box'},
            'width': reso,
            'height': reso,
            'sample_border': True,
            'pixel_format': 'rgb',
            'component_format': 'float32',
        }
    }
}

scene = mi.load_dict(scene_dict)
params = mi.traverse(scene)
key = 'shape.to_world'
value_init = mi.Transform4f(params[key])

def update_scene(theta):
    params[key] = mi.Transform4f().translate([theta, 0, 0]) @ value_init
    params.update()

eps = 1e-3

# Render FD
repeat_fd = 2
spp_fd = 8192 * 32
image_fd = dr.zeros(mi.TensorXf, (reso, reso, 3))
for i in range(repeat_fd):
    update_scene(eps)
    img1 = mi.render(scene, params=params, spp= spp_fd, seed=i)
    update_scene(-eps)
    img2 = mi.render(scene, params=params, spp= spp_fd, seed=i)

    image_fd += img1 - img2
    dr.eval(image_fd)
image_fd /= (2 * eps * repeat_fd)
mi.Bitmap(image_fd).write(f'{folder}/image_fd_{deg}.exr')

# Render AD
repeat_ad = 1
spp_ad = 8192 * 4
# spp_ad = 64
image_ad = dr.zeros(mi.TensorXf, (reso, reso, 3))
for i in range(repeat_ad):

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    update_scene(theta)
    dr.forward(theta, dr.ADFlag.ClearEdges)

    image = mi.render(scene, seed=i, spp=spp_ad, params=params)
    dr.forward_to(image)
    image_ad += dr.grad(image)
    dr.eval(image_ad)
image_ad /= repeat_ad
mi.Bitmap(image_ad).write(f'{folder}/image_ad_{deg}.exr')

print(f"Rendered images with {deg} degrees.")
