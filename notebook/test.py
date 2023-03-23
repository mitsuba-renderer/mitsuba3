import os
import sys
import mitsuba as mi
import drjit as dr
from matplotlib import pyplot as plt
import numpy as np
mi.set_variant('cuda_ad_rgb')
from mitsuba.python.util import traverse

def mse(image, image_ref):
    return dr.mean(dr.sqr(image - image_ref))

# Reference image and ref texture
image_ref = mi.Bitmap('figures/mi_ref.exr')
ref_tex = mi.load_dict({
        'type': 'bitmap',
        'filename': 'figures/check16.png',
        'filter_type': 'bilinear',
        'wrap_mode': 'repeat',
        'to_uv': mi.ScalarTransform4f.scale([16, 16, 1])
    })
ref_param = traverse(ref_tex)
m_texture = ref_param['data']
dr.detach(m_texture)

sensor = mi.load_string('<sensor version="3.0.0" type="perspective" id="sensor">  \
        <float name="fov" value="35"/> <float name="near_clip" value="0.10000000149011612"/> \
		<float name="far_clip" value="1000.0"/> \
        <transform name="to_world"> \
            <lookat target="0, 2, 0" origin="0, 2, -50" up="0, 1, 0"/> \
            <rotate y="1" angle="0"/> \
        </transform> \
        <sampler type="independent"> \
            <integer name="sample_count" value="8"/> \
        </sampler> \
        <film type="hdrfilm"> \
            <integer name="width"  value="1024"/> \
            <integer name="height" value="1024"/> \
            <rfilter type="gaussian"/> \
            <string name="pixel_format" value="rgb"/> \
        </film> \
    </sensor>')

# Light
emitter = mi.load_string('\
    <emitter version = "3.0.0" type="directional"> \
        <rgb name="irradiance" value="2"/> \
        <vector name="direction" value="0, -1, -1"/> \
    </emitter> ')

# BSDF
ctx = mi.BSDFContext()

bsdf = mi.load_dict({      
    'type': 'diffuse',
    'reflectance': {
        'type': 'mipmap',
        'filename': 'figures/rcheck16.png',
        'filter_type': 'bilinear',
        'wrap_mode': 'repeat',
        'mipmap_filter_type': 'trilinear',
        'to_uv': mi.ScalarTransform4f.scale([16, 16, 1])
    }
})

# Obj
floor = mi.load_dict({
    'type': 'obj',
    'filename': 'figures/meshes/Plane.obj', 
    'bsdf': bsdf
})

# Scene
scene = mi.load_dict({
    "type" : "scene",
    'integrator': { 'type': 'direct' },
    "sensor": sensor,
    "emitter": emitter,
    "obj": floor,
})

params = mi.traverse(scene)
print (params)

key = 'obj.bsdf.reflectance.data'
test = 'obj.bsdf.reflectance.test'
dr.set_label(params[key], 'myTexture')

dr.enable_grad(params[key])
mi.traverse(scene)
params.update()

#################################################################
''' AD TEST'''
dr.set_flag(dr.JitFlag.VCallRecord, True)

graph = dr.graphviz_ad()
graph.render('mipmap.dot')

image = mi.render(scene, params, spp=4)
mi.Bitmap(image).write('figures/mi_r.exr')

iteration_count = 1
errors = []

# Evaluate the objective function from the current rendered image
loss = mse(image, image_ref)

# Backpropagate through the rendering process
dr.backward(loss)

grad = dr.grad(params[key])
grad *= 1e7

# Track the difference between the current color and the true value
err_ref = dr.mean(dr.sqr(m_texture- params[key]))
# print(f"Iteration {it:02d}: parameter error = {err_ref[0]:6f}", end='\r')
errors.append(err_ref)

print('\nOptimization complete.\n')
print (grad.array)
mi.Bitmap(grad).write('figures/grad_ewa.exr')
mi.util.convert_to_bitmap(grad)


''' FORWARD AD '''
# dr.set_grad(params[key], 1)
# grad = dr.grad(params[key])
# print (grad)

# dr.forward(params[key])
# print (dr.grad_enabled(image))
# grad = dr.grad(image)
# grad *= 1e6

# a = grad.numpy()
# print (a[a<0])
# mi.Bitmap(grad)

# graph = dr.graphviz_ad()
# graph.render('mipmap.dot')