import os
import numpy as np

from mitsuba.core import Bitmap, Struct, Thread
from mitsuba.core.xml import load_file

from mitsuba.core import Bitmap
from mitsuba.render import RadianceSample3fX, ImageBlock

SCENE_DIR = '../../../resources/data/scenes/'
filename = os.path.join(SCENE_DIR, 'cbox/cbox.xml')

# Append the directory containing the scene to the "file resolver".
# This is needed since the scene specifies meshes and textures
# using relative paths.
directory_name = os.path.dirname(filename)
Thread.thread().file_resolver().append(directory_name)

# Load the actual scene
scene = load_file(filename)

# Instead of calling the scene's integrator, we build our own small integrator
# This integrator simply computes the depth values per pixel
sensor = scene.sensor()
film = sensor.film()
film_size = film.crop_size()
spp = 32

# Sample pixel positions in the image plane
# Inside of each pixels, we randomly choose starting locations for our rays
n_rays = film_size[0] * film_size[1] * spp
pos = np.arange(n_rays) / spp
scale = np.array([1.0 / film_size[0], 1.0 / film_size[1]])
pos = np.stack([pos % int(film_size[0]), pos / int(film_size[0])], axis=1)
position_sample = pos + np.random.rand(n_rays, 2).astype(np.float32)


# Sample rays starting from the camera sensor
rays, weights = sensor.sample_ray_differential(
    time=0,
    sample1=np.random.rand(n_rays).astype(np.float32),
    sample2=position_sample * scale,
    sample3=np.random.rand(n_rays, 2).astype(np.float32))

# Intersect rays with the scene geometry
surface_interaction = scene.ray_intersect(rays)

# Given intersection, compute the final pixel values as the depth t
# of the sampled surface interaction
result = surface_interaction.t
result[~surface_interaction.is_valid()] = 0 # set values to zero if no intersection occured


# Accumulate values into an image block
rfilter = film.reconstruction_filter()
block = ImageBlock(Bitmap.EXYZAW, film.crop_size(), warn=False, filter=rfilter, border=False)
result = np.stack([result, result, result, result], axis=1) # Workaround since ImageBlock assumes 4 values
block.put(position_sample, rays.wavelengths, result, 1)

# Write out the result from the ImageBlock
block.bitmap().convert(Bitmap.EY, Struct.EFloat32, srgb_gamma=False).write('depth.exr')

