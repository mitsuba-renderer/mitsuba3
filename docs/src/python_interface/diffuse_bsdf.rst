Writing a diffuse bsdf
===========================

In this example, we will implement a simple diffuse BSDF in python using a similar approach as in
the :ref:`integrator-direct`, extending the :code:`BSDF` base class. The code is very similar to the
diffuse BSDF implemented in C++ (in :code:`src/bsdf/diffuse.cpp`).

The code for this example can be found in :code:`docs/examples/04_diffuse_bsdf/diffuse_bsdf.py`.

The BSDF class need to implement the following 3 methods: :code:`sample`, :code:`eval` and :code:`pdf`:

.. literalinclude:: ../../examples/04_diffuse_bsdf/diffuse_bsdf.py
   :language: python
   :lines: 14-64

After defining this new BSDF class, we have to register it as a plugin.
This allows Mitsuba to instantiate this new BSDF when loading a scene:

.. literalinclude:: ../../examples/04_diffuse_bsdf/diffuse_bsdf.py
   :language: python
   :lines: 66

The :code:`register_bsdf` function takes the name of our new plugin and a function to construct new instances.
After that, we can use our new BSDF in a scene by specifying

.. code-block:: xml

    <BSDF type="mydiffusebsdf"/>

The scene is then rendered by calling the standard :code:`render` function:

.. literalinclude:: ../../examples/04_diffuse_bsdf/diffuse_bsdf.py
   :language: python
   :lines: 71-80
