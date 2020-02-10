Writing a depth integrator
==========================

In this example, we show how to use the Python bindings to write a simple depth map integrator.
While this being one of the simplest possible integrators to write, it can serve as a starting point to write more complex integrators.
The code for this example can be found in :code:`docs/examples/02_depth_integrator/depth_integrator.py`.


Similar to before, we import a number of modules and load the scene from disk:

.. literalinclude:: ../../examples/02_depth_integrator/depth_integrator.py
   :language: python
   :lines: 1-23

In this example we use the packet version of Mitsuba. This means all calls to Mitsuba functions will be vectorized and we avoid expensive for-loops in Python.

Instead of calling the scene's existing integrator as before, we will now manually trace rays through each pixel of the image:

.. literalinclude:: ../../examples/02_depth_integrator/depth_integrator.py
   :language: python
   :lines: 25-55

After computing the surface intersections for all the rays, we then extract the depth values

.. literalinclude:: ../../examples/02_depth_integrator/depth_integrator.py
   :language: python
   :lines: 57-62

We then splat these depth values to an :code:`ImageBlock`, which is an image data structure that
handles averaging over samples and accounts for the pixel filter. The :code:`ImageBlock` is then
converted to a :code:`Bitmap` object and the resulting image saved to disk.

.. literalinclude:: ../../examples/02_depth_integrator/depth_integrator.py
   :language: python
   :lines: 64-82