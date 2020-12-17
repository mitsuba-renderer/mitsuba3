# Simple inverse rendering example: render a cornell box reference image, then
# then replace one of the scene parameters and try to recover it using
# differentiable rendering and gradient-based optimization.

import enoki as ek
import mitsuba
mitsuba.set_variant('cuda_autodiff_rgb')

from mitsuba.core import Float, Thread
from mitsuba.core.xml import load_file
from mitsuba.python.util import traverse
from mitsuba.python.autodiff import render, write_bitmap, Adam
import time


# Load example scene
scene = load_file('bunny/bunny.xml')

# Find differentiable scene parameters
params = traverse(scene)

# Make a backup copy
param_res = params['my_envmap.resolution']
param_ref = Float(params['my_envmap.data'])

# Discard all parameters except for one we want to differentiate
params.keep(['my_envmap.data'])

# Render a reference image (no derivatives used yet)
image_ref = render(scene, spp=16)
crop_size = scene.sensors()[0].film().crop_size()
write_bitmap('out_ref.png', image_ref, crop_size)

# Change to a uniform white lighting environment
params['my_envmap.data'] = ek.full(Float, 1.0, len(param_ref))
params.update()

# Construct an Adam optimizer that will adjust the parameters 'params'
opt = Adam(params, lr=.02)

time_a = time.time()

iterations = 100
for it in range(iterations):
    # Perform a differentiable rendering of the scene
    image = render(scene, optimizer=opt, unbiased=True, spp=1)
    write_bitmap('out_%03i.png' % it, image, crop_size)
    write_bitmap('envmap_%03i.png' % it, params['my_envmap.data'],
                 (param_res[1], param_res[0]))

    # Objective: MSE between 'image' and 'image_ref'
    ob_val = ek.hsum(ek.sqr(image - image_ref)) / len(image)

    # Back-propagate errors to input parameters
    ek.backward(ob_val)

    # Optimizer: take a gradient step
    opt.step()

    # Compare iterate against ground-truth value
    err_ref = ek.hsum(ek.sqr(param_ref - params['my_envmap.data']))
    print('Iteration %03i: error=%g' % (it, err_ref[0]), end='\r')

time_b = time.time()

print()
print('%f ms per iteration' % (((time_b - time_a) * 1000) / iterations))
