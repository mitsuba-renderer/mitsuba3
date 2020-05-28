.. _sec-rendering-scene:

Rendering a scene
=================

In the previous section, we learned how to load a scene from an XML file. Once a scene has been
loaded, it can be rendered as follows:

.. code-block:: python

    ...

    # Load the scene for an XML file
    scene = load_file(filename)

    # Get the scene's sensor (if many, can pick one by specifying the index)
    sensor = scene.sensors()[0]

    # Call the scene's integrator to render the loaded scene with the desired sensor
    scene.integrator().render(scene, sensor)

After rendering, it is possible to write out the rendered data as an HDR OpenEXR file like this:

.. code-block:: python

    # The rendered data is stored in the film
    film = sensor.film()

    # Write out data as high dynamic range OpenEXR file
    film.set_destination_file('/path/to/output.exr')
    film.develop()

One can also write out a gamma tone-mapped JPEG file of the same rendering
using the :py:class:`mitsuba.core.Bitmap` class:

.. code-block:: python

    # Write out a tone-mapped JPG of the same rendering
    from mitsuba.core import Bitmap, Struct
    img = film.bitmap(raw=True).convert(Bitmap.PixelFormat.RGB, Struct.Type.UInt8, srgb_gamma=True)
    img.write('/path/to/output.jpg')

The ``raw=True`` argument in :code:`film.bitmap()` specifies that we are
interested in the raw film contents to be able to perform a conversion into the
desired output format ourselves.

See :py:meth:`mitsuba.core.Bitmap.convert` for more information regarding the bitmap convertion routine.

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
    :file:`docs/examples/01_render_scene/render_scene.py`


.. _sec-rendering-scene-custom:

Custom rendering pipeline in Python
------------------------------------

In the following section, we show how to use the Python bindings to write a
simple depth map renderer, including ray generation and pixel value splatting,
purely in Python. While this is of course much more work than simply calling
the integrator's ``render()``, this fine-grained level of control can be useful
in certain applications. Please also refer to the related section on
:ref:`developing custom plugins in Python <sec-custom-plugins>`.

Similar to before, we import a number of modules and load the scene from disk:

.. literalinclude:: ../../examples/02_depth_integrator/depth_integrator.py
   :language: python
   :lines: 1-21

In this example we use the packet variant of Mitsuba. This means all calls to
Mitsuba functions will be vectorized and we avoid expensive for-loops in
Python. The same code will work for `gpu` variants of the renderer as well.

Instead of calling the scene's existing integrator as before, we will now
manually trace rays through each pixel of the image:

.. literalinclude:: ../../examples/02_depth_integrator/depth_integrator.py
   :language: python
   :lines: 23-57

After computing the surface intersections for all the rays, we then extract the depth values

.. literalinclude:: ../../examples/02_depth_integrator/depth_integrator.py
   :language: python
   :lines: 59-64

We then splat these depth values to an :code:`ImageBlock`, which is an image data structure that
handles averaging over samples and accounts for the pixel filter. The :code:`ImageBlock` is then
converted to a :code:`Bitmap` object and the resulting image saved to disk.

.. literalinclude:: ../../examples/02_depth_integrator/depth_integrator.py
   :language: python
   :lines: 66-84

.. note:: The code for this example can be found in :code:`docs/examples/02_depth_integrator/depth_integrator.py`
