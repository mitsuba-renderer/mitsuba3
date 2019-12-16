import os
import numpy as np

import mitsuba

from mitsuba.packet_rgb.core  import Bitmap, Struct, Thread
from mitsuba.packet_rgb.core .xml import load_file


SCENE_DIR = '../../../resources/data/scenes/'
filename = os.path.join(SCENE_DIR, 'cbox/cbox.xml')

# Append the directory containing the scene to the "file resolver".
# This is needed since the scene specifies meshes and textures
# using relative paths.
directory_name = os.path.dirname(filename)
Thread.thread().file_resolver().append(directory_name)

# Load the actual scene
scene = load_file(filename)

# Call the scene's integrator to render the loaded scene

scene.integrator().render(scene, scene.sensors()[0])

# After rendering, the rendered data is stored in the film
film = scene.sensors()[0].film()

# Write out rendering as high dynamic range OpenEXR file (after converting to linear RGB space)
film.bitmap().convert(Bitmap.ERGB, Struct.EFloat32, srgb_gamma=False).write('cbox.exr')

# Write out a tonemapped JPG of the same rendering
film.bitmap().convert(Bitmap.ERGB, Struct.EUInt8, srgb_gamma=True).write('cbox.jpg')

# Get the pixel values as a numpy array for further processing
image_np = np.array(film.bitmap().convert(Bitmap.ERGB, Struct.EFloat32, srgb_gamma=False))
print(image_np.shape)