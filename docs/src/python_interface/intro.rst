.. _sec-python:

Introduction
==============

Mitsuba 2 provides extremely fine-grained Python bindings to essentially every
function in the system. This makes it possible to import the renderer into a
Jupyter notebook and develop new algorithms interactively while visualizing
their behavior using plots. 

Ray intersections, BSDF evaluations and emitters, and numerous other system
components can be queried and sampled through an efficient vectorized
interface, enabling the design of non-traditional applications that do not
necessarily produce a rendering as output. The bindings blur the boundaries
between C++ and the Python world: it is for instance possible to to implement a
new Mitsuba plugin (e.g. a BSDF) using Python and use it to render an image,
where most rendering code runs in C++. Several Mitsuba variants heavily rely on
JIT compilation, and such Python extensions are jointly JIT-compiled along with
other C++ portions of the system.

Mitsuba's automated test suite is entirely written in Python, which ensures the
completeness and correctness of the Python bindings and also serves as an
excellent source of Python example code.

This remainder of this section provides a number of examples on using the
Python API and points out additional sources of information. For now, we focus
on the use of the bindings for "forward" rendering applications. A separate set
of tutorials describes applications to differentiable and inverse rendering.

Importing the renderer
----------------------

Mitsuba 2 ships with many different system variants, each of which provides its
own set of Python bindings that are necessarily different from others due to
the replacement of types in function signatures. 

To import Mitsuba into Python, you will first have to import the central
:code:`mitsuba` module and set the desired variant. Following this, specific
classes can be imported.

.. code-block:: python

    import mitsuba

    mitsuba.set_variant('gpu_autodiff_spectral')

    # set_variant() call must precede the following two lines
    from mitsuba.core import FileStream
    from mitsuba.render import Integrator

Behind the scenes, Mitsuba exposes a *virtual* Python module that dynamically
resolves import statements to the right destination (as specified via
``set_variant()``). The variant name can be changed at any time and will then
apply to future imports. The variant name is a per-thread property, hence
multiple independent threads can execute code in separate variants.

Other helper functions are provided in Python in order to interact with the
available variants:

.. code-block:: python

    # Returns the list of available system variants
    mitsuba.variants() # e.g. ['scalar_rgb', 'gpu_autodiff_spectral']

    # Returns the name of the current variant
    mitsuba.variant() # e.g. 'gpu_autodiff_spectral'


API documentation
-----------------

Extensive API documentation is available as part of this document. See the
:ref:`sec-api` section for details.

The Python bindings furthermore export docstrings, making it is possible to
obtain information on classes, function via the ``help()`` function:

.. code-block:: python

    import mitsuba
    mitsuba.set_variant('scalar_rgb')
    help(mitsuba.core.Bitmap)

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

    # Generate 1000^2 samples in the unit square using Numpy
    sample_count = 1000
    samples = np.random.random((sample_count, 2))

    # Project the 2D grid onto a unit sphere (implicit conversion to enoki type)
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
