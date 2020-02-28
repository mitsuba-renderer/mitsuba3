Rendering a scene
=================

In the previous section, we learned how to load a scene from an XML file. Once a scene has been
loaded, it can be rendered as follows:

.. code-block:: python

    import os
    import mitsuba
    mitsuba.set_variant('scalar_rgb')
    from mitsuba.core import Bitmap, hread
    from mitsuba.core.xml import load_file

    # Absolute or relative path to the XML file
    filename = 'path/to/my/scene.xml'

    # Add the scene directory to the FileResolver's search path
    Thread.thread().file_resolver().append(os.path.dirname(filename))

    # Load the scene for an XML file
    scene = load_file(filename)

    # Get the scene's sensor (if many, can pick one by specifying the index)
    sensor = scene.sensors()[0]

    # Call the scene's integrator to render the loaded scene with the desired sensor
    scene.integrator().render(scene, sensor)

After rendering, it is possible to write out the rendered data as an HDR OpenEXR file like this:

.. code-block:: python

    # The rendered data is stored in the film
    film = scene.sensors()[0].film()

    # Write out data as high dynamic range OpenEXR file
    film.set_destination_file('/path/to/output.exr')
    film.develop()

One can also write out a gamma tone-mapped JPEG file of the same rendering
using the :py:class:`mitsuba.core.Bitmap` class:

.. code-block:: python

    # Write out a tone-mapped JPG of the same rendering
    from mitsuba.core import Struct
    img = film.bitmap(raw=True).convert(Bitmap.PixelFormat.RGB, Struct.Type.UInt8, srgb_gamma=True)
    img.write('/path/to/output.jpg')

The ``raw=True`` argument in :code:`film.bitmap()` specifies that we are
interested in the raw film contents to be able to perform a conversion into the
desired output format ourselves.

The data stored in the ``Bitmap`` object can also be cast into a NumPy array for further processing
in Python:

.. code-block:: python

    # Get linear pixel values as a NumPy array for further processing
    img = img.convert(Bitmap.PixelFormat.RGB, Struct.Type.Float32, srgb_gamma=False)
    import numpy as np
    image_np = np.array(img)
    print(image_np.shape)

.. note::

    The full Python script of this tutorial can be found in the file:
    :file:`docs/examples/01_render_scene/render_scene.py`.
