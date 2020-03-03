# Simple forward differentiation example: show how a perturbation
# of a single scene parameter changes the rendered image.

import enoki as ek
import mitsuba
mitsuba.set_variant('gpu_autodiff_rgb')

from mitsuba.core import Thread, Float
from mitsuba.core.xml import load_file
from mitsuba.python.util import traverse
from mitsuba.python.autodiff import render, write_bitmap

# Load the Cornell Box
Thread.thread().file_resolver().append('cbox')
scene = load_file('cbox/cbox.xml')

# Find differentiable scene parameters
params = traverse(scene)

# Keep track of derivatives with respect to one parameter
param_0 = params['red.reflectance.value']
ek.set_requires_gradient(param_0)

# Differentiable simulation
image = render(scene, spp=32)

# Assign the gradient [1, 1, 1] to the 'red.reflectance.value' input
ek.set_gradient(param_0, [1, 1, 1], backward=False)

# Forward-propagate previously assigned gradients
Float.forward()

# The gradients have been propagated to the output image
image_grad = ek.gradient(image)

# .. write them to a PNG file
crop_size = scene.sensors()[0].film().crop_size()
fname = 'out.png'
write_bitmap(fname, image_grad, crop_size)
print('Wrote forward differentiation image to: {}'.format(fname))
