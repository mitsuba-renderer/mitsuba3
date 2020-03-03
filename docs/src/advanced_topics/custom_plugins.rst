.. _sec-custom-plugins:

Custom plugins in Python
========================

In Mitsuba 2, the Python bindings provide a mechanism to implement custom plugins directly in Python.
This can be achieved by extending one of the plugin's base class (e.g. `BSDF`, `Emitter`) and
overwriting the class methods (e.g. ``sample()``, ``eval()``, ...). This mechanism leverages the
`trampoline feature <https://pybind11.readthedocs.io/en/stable/advanced/classes.html#overriding-virtual-functions-in-python>`_ of *pybind11*.

The custom Python plugin can then be registered to the XML parser by calling one of the
``register_<type>(name, constructor)`` Python function as described later in the examples. By doing
so, it is possible to refer to this new plugin in the XML scene description file as with any other
C++ plugins.

.. note:: This mechanism currently only supports custom Python plugins for `BSDF`, `Emitter` and
          `Integrator`.

The rest of this section discusses various examples using this mechanism:


BSDF example
------------

In this example, we will implement a simple diffuse BSDF in Python by extending the :code:`BSDF`
base class. The code is very similar to the diffuse BSDF implemented in C++
(in :code:`src/bsdf/diffuse.cpp`).

The BSDF class need to implement the following 3 methods: :code:`sample`, :code:`eval` and :code:`pdf`:

.. literalinclude:: ../../examples/04_diffuse_bsdf/diffuse_bsdf.py
   :language: python
   :lines: 14-64

After defining this new BSDF class, we have to register it as a plugin.
This allows Mitsuba to instantiate this new BSDF when loading a scene:

.. literalinclude:: ../../examples/04_diffuse_bsdf/diffuse_bsdf.py
   :language: python
   :lines: 66

The :code:`register_bsdf` function takes the name of our new plugin and a function to construct new
instances. After that, we can use our new BSDF in a XML scene file by specifying

.. code-block:: xml

    <BSDF type="mydiffusebsdf"/>

The scene can then rendered by calling the standard :code:`render` function:

.. literalinclude:: ../../examples/04_diffuse_bsdf/diffuse_bsdf.py
   :language: python
   :lines: 68-77

.. note:: The code for this example can be found in :code:`docs/examples/04_diffuse_bsdf/diffuse_bsdf.py`


Integrator example
------------------

In this example, we will implement custom direct illumination integrator using the same mechanism.
The resulting image will have realistic shadows and shading, but no global illumination.

The main rendering routing can be implemented in around 30 lines of code:

.. literalinclude:: ../../examples/03_direct_integrator/direct_integrator.py
   :language: python
   :lines: 22-55

The code is very similar to the direct illumination integrator implemented in C++
(in :code:`src/integrators/direct.cpp`).
The function takes the current scene, sampler and array of rays as arguments.
The :code:`active` argument specifies which lanes are active.

Similar to before, we first intersect the rays with the scene.
We then first evaluate the radiance of directly visible emitters.
Then, we explicitly sample positions on emitters and evaluate their contributions.
Additionally, we sample a ray direction according to the BSDF and evaluate whether these rays also hit some emitters.
These different contributions are then combined using multiple importance sampling to reduce variance.

This function will be invoked for an array of different rays, hence each ray can then potentially hit a surface with a different BSDF.
Therefore, :code:`bsdf = si.bsdf(rays)` will be an array of different BSDF pointers.
To then call member functions of these different BSDFs, we have to invoke the according vectorized functions, e.g.

.. literalinclude:: ../../examples/03_direct_integrator/direct_integrator.py
   :language: python
   :lines: 38-39

This ensures that implementation provided by the different BSDF models will be called.
Other than that, the code and used interfaces are nearly identical to the C++ version.
Please refer to the documentation of the C++ types for details on the different functions and objects.

When implementing the depth integrator, we did a lot of manual work to correctly sample rays from the camera and splatting samples into the film.
While this can be very useful for certain applications, it is also a bit tedious.
In many cases, we simply want to implement a custom integrator and not bother with how camera rays are exactly generated.
In this example, we will therefore use a more elegant mechanism, which allows to simply extend the :code:`SamplingIntegrator` base class.
This class is the base class of all Monte Carlo integrators, e.g. the path tracer.
By using this class to define our integrator, we can then rely on the existing machinery in Mitsuba to correctly sample camera rays and splat pixel values.

Since we already defined a function :code:`integrator_sample` with the right interface, defining a new integrator becomes very simple:

.. literalinclude:: ../../examples/03_direct_integrator/direct_integrator.py
   :language: python
   :lines: 58-70

This integrator not only returns the color, but also additionally renders out the depth of the scene.
When using this integrator, Mitsuba will output a multichannel EXR file with a separate depth channel.
The method :code:`aov_names` is used to name the different arbitrary output variables (AOVs).
The same mechanism could be used to output for example surface normals.
If no AOVs are needed, you can just return an empty list instead.

After defining this new integrator class, we have to register it as a plugin.
This allows Mitsuba to instantiate this new integrator when loading a scene:

.. literalinclude:: ../../examples/03_direct_integrator/direct_integrator.py
   :language: python
   :lines: 74

The :code:`register_integrator` function takes the name of our new plugin and a function to construct new instances.
After that, we can use our new integrator in a scene by specifying

.. code-block:: xml

    <integrator type="mydirectintegrator"/>

The scene is then rendered by calling the standard :code:`render` function:

.. literalinclude:: ../../examples/03_direct_integrator/direct_integrator.py
   :language: python
   :lines: 76-85

.. note:: The code for this example can be found in :code:`docs/examples/03_direct_integrator/direct_integrator.py`
