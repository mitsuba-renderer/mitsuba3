#!/usr/bin/env python
# coding: utf-8

# In[1]:


import mitsuba as mi
import drjit as dr


# In[2]:


mi.set_variant('cuda_ad_rgb')


# In[3]:


#from mitsuba.scalar_rgb import ScalarTransform4f as T
#
#res = 128
#
#scene_description = {
#    'type': 'scene',
#    'plane': {
#        'type': 'obj',
#        'filename': '/home/nicolas/rgl/mitsuba3/resources/data/common/meshes/rectangle.obj',
#        'face_normals': True,
#    },
#    'occluder': {
#        'type': 'obj',
#        'filename': '/home/nicolas/rgl/mitsuba3/resources/data/common/meshes/rectangle.obj',
#        'face_normals': True,
#        'to_world': T().translate([-1, 0, 0.5]) @ T().rotate([0, 1, 0], 90) @ T().scale(1.0),
#    },
#    'light': {
#        'type': 'sphere',
#        'to_world': T().translate([-4, 0, 6]) @ T().scale(0.5),
#        'emitter': {
#            'type': 'area',
#            'radiance': {'type': 'rgb', 'value': [15.0, 15.0, 15.0]}
#        }
#    },
#    'sensor': {
#        'type': 'perspective',
#        'to_world': T().look_at(origin=[0, 0, 4], target=[0, 0, 0], up=[0, 1, 0]),
#        'fov': 50,
#        'film': {
#            'type': 'hdrfilm',
#            'rfilter': { 'type': 'box' },
#            'width': res,
#            'height': res,
#            'sample_border': True,
#            'pixel_format': 'rgb',
#            'component_format': 'float32',
#        }
#    }
#}
#
#prb_integrator = mi.load_dict({
#    'type': 'prb',
#    'max_depth': 3
#})
#
#scene = mi.load_dict(scene_description)


# In[4]:


#primal = mi.render(scene, integrator=prb_integrator, spp=512)
#mi.Bitmap(primal).write('primal.exr')


# In[5]:


from mitsuba.scalar_rgb import ScalarTransform4f as T

res = 128

scene_description = {
    'type': 'scene',
    'plane': {
        'type': 'obj',
        'filename': '/home/nroussel/rgl/mitsuba3/resources/data/common/meshes/rectangle.obj',
        'face_normals': True,
        'emitter': {
            'type': 'area',
            'radiance': {
                'type': 'bitmap',
                'filename': 'gradient.jpg',
                'wrap_mode': 'clamp',
            }
        }
    },
    'integrator': {
        'type': 'path',
        'max_depth': 2,
    },
    'occluder': {
        'type': 'obj',
        'filename': '/home/nroussel/rgl/mitsuba3/resources/data/common/meshes/rectangle.obj',
        'face_normals': True,
        'to_world': T().translate([-0.99, 0, 0.5]) @ T().rotate([0, 1, 0], 90) @ T().scale(1.0),
    },
    #'light': {
    #    'type': 'sphere',
    #    'to_world': T().translate([-4, 0, 6]) @ T().scale(0.5),
    #    'emitter': {
    #        'type': 'area',
    #        'radiance': {'type': 'rgb', 'value': [15.0, 15.0, 15.0]}
    #    }
    #},
    'sensor': {
        'type': 'perspective',
        'to_world': T().look_at(origin=[0, 0, 4], target=[0, 0, 0], up=[0, 1, 0]),
        'fov': 50,
        'film': {
            'type': 'hdrfilm',
            'rfilter': { 'type': 'box' },
            'width': res,
            'height': res,
            'sample_border': True,
            'pixel_format': 'rgb',
            'component_format': 'float32',
            #'crop_offset_x': 98,
            #'crop_offset_y': 29,
            #'crop_width': 1,
            #'crop_height': 1,
        }
    }
}

scene = mi.load_dict(scene_description)


# In[6]:


primal = mi.render(scene, spp=512)
mi.Bitmap(primal).write('primal.exr')

params = mi.traverse(scene)
key = 'plane.vertex_positions'
initial_params = dr.unravel(mi.Vector3f, params[key])


# In[7]:


def update(theta):
    params[key] = dr.ravel(initial_params + mi.Vector3f(theta, 0.0, 0.0))
    params.update()
    dr.eval()


# In[8]:


#res = scene.sensors()[0].film().size()
#image_1 = dr.zeros(mi.TensorXf, (res[1], res[0], 3))
#image_2 = dr.zeros(mi.TensorXf, (res[1], res[0], 3))
#
#epsilon = 1e-3
#N = 10
#for it in range(N):
#    theta = mi.Float(-0.5 * epsilon)
#    update(theta)
#    image_1 += mi.render(scene, spp=46000 * 4, seed=it)
#    dr.eval(image_1)
#
#    theta = mi.Float(0.5 * epsilon)
#    update(theta)
#    image_2 += mi.render(scene, spp=46000 * 4, seed=it)
#    dr.eval(image_2)
#    
#    print(f"{it+1}/{N}", end='\r')
#    
#mi.Bitmap(image_1 / N).write('fd_1.exr')
#mi.Bitmap(image_2 / N).write('fd_2.exr')
#
#image_fd = (image_2 - image_1) / epsilon / N 
#
#mi.util.write_bitmap("fd.exr", image_fd)


# In[9]:


#dr.set_flag(dr.JitFlag.Debug, True)
#dr.set_flag(dr.JitFlag.SymbolicLoops, False)

update(0)
theta = mi.Float(0)
dr.enable_grad(theta)
update(theta)
dr.forward(theta, flags=dr.ADFlag.ClearEdges)

path_integrator = mi.load_dict({
    'type': 'path',
    'max_depth': 2
})

prb_integrator = mi.load_dict({
    'type': 'prb',
    'max_depth': 2
})

prb_basic_integrator = mi.load_dict({
    'type': 'prb_basic',
    'max_depth': 2
})

prb_projective_integrator = mi.load_dict({
    'type': 'prb_projective',
    'max_depth': 2,
    #'guiding': 'none',
    #'sppp': 100,
    #'sppc': 4096,
    #'sppi': 2048,
    'sppp': 0,
    'sppc': 0,
    'sppi': 2048,
})

direct_projective_integrator = mi.load_dict({
    'type': 'direct_projective',
    #'guiding': 'none',
    'sppp': 100,
    'sppc': 4096,
    'sppi': 2048,
    #'sppp': 0,
    #'sppc': 0,
    #'sppi': 2048,
})

#grad_img_cont = path_integrator.render_forward(scene, params=params, spp=512, seed=1)
#grad_img_cont = prb_basic_integrator.render_forward(scene, params=params, spp=4096, seed=11)
#grad_img_cont = prb_integrator.render_forward(scene, params=params, spp=4096, seed=19)

update(0)
theta = mi.Float(0)
dr.enable_grad(theta)
update(theta)
dr.forward(theta, flags=dr.ADFlag.ClearEdges)

grad_img_disc = direct_projective_integrator.render_forward(scene, params=params, seed=1)
#grad_img_disc = prb_projective_integrator.render_forward(scene, params=params)

#grad_img = grad_img_cont 
grad_img = grad_img_disc
#grad_img = grad_img_cont + grad_img_disc

print(dr.max(dr.abs(grad_img), axis=None))
mi.util.write_bitmap("fwd.exr", grad_img)
#mi.util.write_bitmap("prb_projective_disc.exr", grad_img)


# In[10]:


import numpy as np
import drjit as dr
import mitsuba as mi
mi.set_variant('cuda_ad_rgb')
tmp = dr.repeat(dr.linspace(mi.Float, 0, 1, 512), 3).numpy()
tmp = tmp.reshape(512, 3)[np.newaxis, ...]
tmp = np.repeat(tmp, 512, axis=0)
tmp.shape
mi.util.write_bitmap('gradient.jpg', tmp)


# In[ ]:





# In[ ]:




