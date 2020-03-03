# Simple inverse rendering example: render a cornell box reference image, 
# then replace one of the scene parameters and try to recover it using
# differentiable rendering and gradient-based optimization. (PyTorch)

import enoki as ek
import mitsuba
mitsuba.set_variant('gpu_autodiff_rgb')

from mitsuba.core import Thread, Vector3f
from mitsuba.core.xml import load_file
from mitsuba.python.util import traverse
from mitsuba.python.autodiff import render_torch, write_bitmap
import torch
import time

Thread.thread().file_resolver().append('cbox')
scene = load_file('cbox/cbox.xml')

# Find differentiable scene parameters
params = traverse(scene)

# Discard all parameters except for one we want to differentiate
params.keep(['red.reflectance.value'])

# Print the current value and keep a backup copy
param_ref = params['red.reflectance.value'].torch()
print(param_ref)

# Render a reference image (no derivatives used yet)
image_ref = render_torch(scene, spp=8)
crop_size = scene.sensors()[0].film().crop_size()
write_bitmap('out_ref.png', image_ref, crop_size)

# Change the left wall into a bright white surface
params['red.reflectance.value'] = [.9, .9, .9]
params.update()

# Which parameters should be exposed to the PyTorch optimizer?
params_torch = params.torch()

# Construct a PyTorch Adam optimizer that will adjust 'params_torch'
opt = torch.optim.Adam(params_torch.values(), lr=.2)
objective = torch.nn.MSELoss()

time_a = time.time()

iterations = 100
for it in range(iterations):
    # Zero out gradients before each iteration
    opt.zero_grad()

    # Perform a differentiable rendering of the scene
    image = render_torch(scene, params=params, unbiased=True,
                         spp=1, **params_torch)

    write_bitmap('out_%03i.png' % it, image, crop_size)

    # Objective: MSE between 'image' and 'image_ref'
    ob_val = objective(image, image_ref)

    # Back-propagate errors to input parameters
    ob_val.backward()

    # Optimizer: take a gradient step
    opt.step()

    # Compare iterate against ground-truth value
    err_ref = objective(params_torch['red.reflectance.value'], param_ref)
    print('Iteration %03i: error=%g' % (it, err_ref), end='\r')

time_b = time.time()

print()
print('%f ms per iteration' % (((time_b - time_a) * 1000) / iterations))
