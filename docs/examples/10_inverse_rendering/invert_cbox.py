# Simple inverse rendering example: render a cornell box reference image, then
# then replace one of the scene parameters and try to recover it using
# differentiable rendering and gradient-based optimization.

import enoki as ek
import mitsuba
mitsuba.set_variant('cuda_autodiff_rgb')

from mitsuba.core import Thread, Color3f
from mitsuba.core.xml import load_file
from mitsuba.python.util import traverse
from mitsuba.python.autodiff import render, write_bitmap, Adam
import time


# Load the Cornell Box
scene = load_file('cbox/cbox.xml')

# Find differentiable scene parameters
params = traverse(scene)

# Discard all parameters except for one we want to differentiate
params.keep(['red.reflectance.value'])

# Print the current value and keep a backup copy
param_ref = Color3f(params['red.reflectance.value'])
print(param_ref)

# Render a reference image (no derivatives used yet)
image_ref = render(scene, spp=8)
crop_size = scene.sensors()[0].film().crop_size()
write_bitmap('out_ref.png', image_ref, crop_size)

# Change the left wall into a bright white surface
params['red.reflectance.value'] = [.9, .9, .9]
params.update()

# Construct an Adam optimizer that will adjust the parameters 'params'
opt = Adam(params, lr=.2)

time_a = time.time()

iterations = 100
for it in range(iterations):
    # Perform a differentiable rendering of the scene
    image = render(scene, optimizer=opt, unbiased=True, spp=1)
    write_bitmap('out_%03i.png' % it, image, crop_size)

    # Objective: MSE between 'image' and 'image_ref'
    ob_val = ek.hsum(ek.sqr(image - image_ref)) / len(image)

    # Back-propagate errors to input parameters
    ek.backward(ob_val)

    # Optimizer: take a gradient step
    opt.step()

    # Compare iterate against ground-truth value
    err_ref = ek.hsum(ek.sqr(param_ref - params['red.reflectance.value']))
    print('Iteration %03i: error=%g' % (it, err_ref[0]), end='\r')

time_b = time.time()

print()
print('%f ms per iteration' % (((time_b - time_a) * 1000) / iterations))
