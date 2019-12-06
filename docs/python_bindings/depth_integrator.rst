Writing a depth integrator
==========================

In this example, we show how to use the Python bindings to write a simple depth map integrator.
While this being one of the simplest possible integrators to write, it can serve as a starting point to write more complex integrators.
The code for this example can be found in :code:`docs/examples/02_depth_integrator/depth_integrator.py`.


Similar to before, we import a number of modules and load the scene from disk:

.. code-block:: python
    :name: rendering-a-scene

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


Now, instead of calling the scene's existing integrator as before, we instead manually trace rays through each pixel of the image:

.. code-block:: python
    :name: rendering-a-scene

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
    # The sensor object implements a method to sample rays for each pixel.
    # (it could potentially account for depth of field, motion blur, etc.)
    rays, weights = sensor.sample_ray_differential(
        time=0,
        sample1=np.random.rand(n_rays).astype(np.float32),
        sample2=position_sample * scale,
        sample3=np.random.rand(n_rays, 2).astype(np.float32))

    # Intersect all rays with the scene geometry
    surface_interaction = scene.ray_intersect(rays)


After computing the surface intersections for all the rays, we then extract the depth values and splat them to an `ImageBlock`,
which is an image data structure that handles averaging over samples and accounts for the pixel filter.

.. code-block:: python
    :name: rendering-a-scene


    # Given intersection, compute the final pixel values as the depth "t"
    # of the sampled surface interaction
    result = surface_interaction.t
    result[~surface_interaction.is_valid()] = 0 # set values to zero if no intersection occured

    # Accumulate values into an image block
    rfilter = film.reconstruction_filter()
    block = ImageBlock(Bitmap.EXYZAW, film.crop_size(), warn=False, filter=rfilter, border=False)

    # ImageBlock takes four values. This is usually needed to represent color,
    # but in the case of the depth integrator we can simply pass the same value four times.
    result = np.stack([result, result, result, result], axis=1)
    block.put(position_sample, rays.wavelengths, result, 1)

    # Write out the result from the ImageBlock as an EXR file
    block.bitmap().convert(Bitmap.EY, Struct.EFloat32, srgb_gamma=False).write('depth.exr')

