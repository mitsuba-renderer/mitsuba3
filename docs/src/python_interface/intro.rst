Introduction
==============

Mitsuba 2 has extensive Python bindings. All plugins come with Python bindings, which means that it
is possible to write vectorized or differentiable integrators entirely in Python.
The bindings can not only be used to write integrators, but also for more specialized applications,
where the output is not necessarily an image.
The user can access scene intersection routines, BSDF evaluations and emitters entirely from Python.

We provide several examples of how the Python bindings can be used.
In this part of the documentation, we focus entirely on the use of the bindings for forward
rendering applications. A seperate group of tutorials describes how to use Mitsuba 2 for
differentiable rendering.

Moreover, the large automated test suite written in Python is an excellent source of examples.


Import Python bindings
----------------------

Mitsuba 2 can be compiled to a great variety of different variants (e.g. :code:`'scalar_rgb'`,
:code:`'gpu_autodiff_spectral_polarized'`, etc.) that each have their own Python bindings in
addition to generic/non-templated code that lives in yet another module. Writing various different
prefixes many times in import statements such as

.. code-block:: python

    from mitsuba.render_gpu_autodiff_spectral_polarized_ext import Integrator
    from mitsuba.core_ext import FileStream

can get rather tiring. For this reason, Mitsuba uses *virtual* Python modules that dynamically
resolve import statements to the right destination. The desired Mitsuba variant should be specified
via this function. The above example then simplifies to

.. code-block:: python

    import mitsuba

    mitsuba.set_variant('gpu_autodiff_spectral_polarized')

    from mitsuba.render import Integrator
    from mitsuba.core import FileStream

The variant name can be changed at any time and will only apply to future imports. The variant name
is a per-thread property, hence multiple independent threads can execute code in separate variants.
Other helper functions are provided in Python in order to interact with the available variants:

.. code-block:: python

    # Returns the list of names of the different variants available
    mitsuba.variants() # e.g. ['scalar_rgb', 'gpu_spectral']

    # Returns the name of the current variant
    mitsuba.variant() # e.g. 'scalar_rgb'


Python API documentation
------------------------

The Python bindings exports comprehensive Python-style docstrings, hence it is possible to get
information on classes, function, or entire namespaces within using the :code:`help` Python function
after having set the desired variant:

.. code-block:: python

    import mitsuba
    mitsuba.set_variant('scalar_rgb')
    help(mitsuba.render.BSDF)

    # Output:
    # class Bitmap(Object)
    # |  General-purpose bitmap class with read and write support for several
    # |  common file formats.
    # |
    # |  This class handles loading of PNG, JPEG, BMP, TGA, as well as OpenEXR
    # |  files, and it supports writing of PNG, JPEG and OpenEXR files.
    # |
    # |  PNG and OpenEXR files are optionally annotated with string-valued
    # |  metadata, and the gamma setting can be stored as well. Please see the
    # |  class methods and enumerations for further detail.
    # |
    # |  Method resolution order:
    # |      Bitmap
    # |      Object
    # |      pybind11_builtins.pybind11_object
    # |      builtins.object
    # |
    # |  Methods defined here:
    # |
    # |  __eq__(...)
    # |      __eq__(self: mitsuba.core.Bitmap, arg0: mitsuba.core.Bitmap) -> bool
    # ...




Enoki aliases
-------------

In order to let the user write generic code in Python that is valid for the different variants of
the renderer, :code:`mitsuba.core` provides aliases for basic types like :code:`Float`,
:code:`UInt32`, etc, as well as for |enoki| types such as :code:`Vector2f`, :code:`Vector3f`,
:code:`Vector2i`, :code:`Point3f`, etc.

.. code-block:: python

    import mitsuba

    mitsuba.set_variant('packet_rgb')
    from mitsuba.core import Float
    # Float = enoki.dynamic.Float (aka DynamicArray<Packet<float>)
    from mitsuba.core import Vector2f
    # Vector2f = enoki.dynamic.Vector2f (aka Array<DynamicArray<Packet<float>, 2>)

    mitsuba.set_variant('gpu_rgb')
    from mitsuba.core import Float
    # Float = enoki.cuda.Float (aka CUDAArray<float>)
    from mitsuba.core import Vector2f
    # Vector2f = enoki.cuda.Vector2f (aka Array<CUDAArray<float>, 2>)

In the following Python snippet, we show how one can use those aliases to write generic
code that can run on the CPU or the GPU, depending on the choosen variant.

.. code-block:: python

    import enoki as ek
    import mitsuba

    # Choose the variant
    mitsuba.set_variant('packet_rgb') # valid code with other variants, e.g. 'gpu_rgb'

    from mitsuba.core import Float, UInt64, Vector2f, PCG32

    # Generate 1000^2 samples in the unit square
    sample_count = 1000
    rng = PCG32(initseq=ek.arange(UInt64, sample_count))
    samples = Vector2f(rng.next_float32(), rng.next_float32())

    # Project the 2D grid onto a unit sphere
    pos = mitsuba.core.warp.square_to_uniform_sphere(samples)


Numpy integration
-----------------

The |enoki| Python bindings rely on `implicit conversion
<https://pybind11.readthedocs.io/en/stable/advanced/classes.html#implicit-conversions>`_ and the
`buffer protocol
<https://pybind11.readthedocs.io/en/stable/advanced/pycpp/numpy.html#buffer-protocol>`_ to
automatically cast |numpy| arrays into the right |enoki| type. This allows the users to
directly pass |numpy| arrays to Mitsuba functions as in the following example:

.. code-block:: python

    import numpy as np
    import mitsuba

    # Choose the variant
    mitsuba.set_variant("packet_rgb") # valid code with other variants, e.g. 'gpu_rgb'

    # Generate 1000^2 samples in the unit square
    sample_count = 1000
    samples = np.random.random((sample_count, 2))

    # Project the 2D grid onto a unit sphere
    pos = mitsuba.core.warp.square_to_uniform_sphere(samples)


Submodules
----------

The Mitsuba Python bindings are split into different Python submodules, following the folder
structure of the C++ codebase.

.. list-table::
    :widths: 30 70
    :header-rows: 1

    * - Submodule name
      - Description
    * - :code:`mitsuba.core`
      - Contains the Python bindings for the classes and functions of the
        :monosp:`mitsuba/libcore` C++ library.
    * - :code:`mitsuba.render`
      - Contains the Python bindings for the classes and functions of the
        :monosp:`mitsuba/librender` C++ library.
    * - :code:`mitsuba.python`
      - Provides classes and functions only related to the Python part of the framework.

.. todo:: re-write those descriptions

