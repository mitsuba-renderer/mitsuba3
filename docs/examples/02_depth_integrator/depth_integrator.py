import os
import numpy as np

import mitsuba
from mitsuba.packet_rgb.core import Bitmap, Struct, Thread
from mitsuba.packet_rgb.core.xml import load_file
from mitsuba.packet_rgb.render import ImageBlock


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
sensor = scene.sensors()[0]
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

block = ImageBlock(film.crop_size(), 5, filter=film.reconstruction_filter(), border=False)
block.clear() # Clear ImageBlock before accumulating into it
result = np.stack([result, result, result], axis=1) # Imageblock expects RGB values (Array of size (n, 3))
block.put(position_sample, rays.wavelengths, result, 1)

# Write out the result from the ImageBlock
# Internally, ImageBlock stores values in XYZAW format (color XYZ, alpha value A and weight W)
xyzaw_np = block.data().reshape([film_size[1], film_size[0], 5])

# We then create a Bitmap from these values and save it out as EXR file
bmp = Bitmap(xyzaw_np, Bitmap.PixelFormat.XYZAW)
bmp.convert(Bitmap.PixelFormat.Y, Struct.Type.Float32, srgb_gamma=False).write('depth.exr')


