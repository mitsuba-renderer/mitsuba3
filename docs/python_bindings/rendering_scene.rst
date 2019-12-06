Rendering a scene
=================

The simplest possible application of the Python bindings is to simply render an existing scene described in an XML file.
All examples used in the following can be found under :code:`docs/examples` in the Mitsuba 2 repository.
Any file paths used in these code examples, assume the example code is run from the folder it is in (e.g. :code:`docs/examples/01_render_scene/render_scene.py` for this current example).

The following code describes how to render a scene and write out the resulting image either as EXR image or as tonemapped JPG.

.. code-block:: python
    :name: rendering-a-scene

    import os
    import numpy as np
    from mitsuba.core import Bitmap, Struct, Thread
    from mitsuba.core.xml import load_file

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
    scene.integrator().render(scene)

    # After rendering, the rendered data is stored in the film
    film = scene.sensor().film()

    # Write out rendering as high dynamic range OpenEXR file (after converting to linear RGB space)
    film.bitmap().convert(Bitmap.ERGB, Struct.EFloat32, srgb_gamma=False).write('cbox.exr')

    # Write out a tonemapped JPG of the same rendering
    film.bitmap().convert(Bitmap.ERGB, Struct.EUInt8, srgb_gamma=True).write('cbox.jpg')

    # Get the pixel values as a numpy array for further processing
    image_np = np.array(film.bitmap().convert(Bitmap.ERGB, Struct.EFloat32, srgb_gamma=False))
    print(image_np.shape)


When loading a scene, we can also optionally pass in arguments to the scene.
If we want to for example parametrize the number of samples per pixel, we could add the following lines to our Cornell box scene file:

.. code-block:: xml
    :name: cbox-xml

    <default name="spp" value="8"/>
    ...
    <sampler type="independent">
        <integer name="sample_count" value="$spp"/>
    </sampler>

To then set this parameter, we pass it when loading the scene:

.. code-block:: python
    :name: cbox-python-paramer

    scene = load_file(filename, [('spp' ,'1024')])


