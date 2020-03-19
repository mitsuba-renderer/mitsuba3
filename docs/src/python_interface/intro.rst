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
``mitsuba`` module and set the desired variant. Following this, specific
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

It is also possible to set a *default* variant that is automatically activated
when importing the ``mitsuba`` Python module. This can be done by setting the
``python-default`` field in the ``mitsuba.conf`` file. Following this, it is no
longer necessary to call ``mitsuba.set_variant(..)`` anymore unless another
variant is desired.


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


Basic arithmetic types
----------------------

Mitsuba heavily relies on the `Enoki
<https://enoki.readthedocs.io/en/master/intro.html>`_ library for elementary
arithmetic types, mathematical operations, vectors, matrices, and so on. Enoki
is used both on the C++ and Python side, and we recommend that you read its
documentation before developing any Mitsuba code.

One important point to note is that elementary types like floating point
numbers, vectors, etc., depend on the current variant. Mitsuba exports aliases
to these types for convenience. For instance, consider the following snippet

.. code-block:: python

    import mitsuba
    mitsuba.set_variant('scalar_rgb')
    from mitsuba.core import Float

The imported ``Float`` type is simply a builtin Python ``float`` because the
renderer is operating in scalar mode. But more complex types would be used in
the vectorized ``packet_*`` or ``gpu_*`` backends, and these also propagate
into derived array types like vectors or matrices.

.. code-block:: python

    mitsuba.set_variant('packet_rgb')
    from mitsuba.core import Float, Vector3f
    # Float    = enoki.dynamic.Float32  (a.k.a. enoki::DynamicArray<Packet<float>>)
    # Vector3f = enoki.dynamic.Vector3f (a.k.a. enoki::Array<DynamicArray<Packet<float>, 3>>)

    mitsuba.set_variant('gpu_rgb')
    from mitsuba.core import Float, Vector3f
    # Float    = enoki.cuda.Float32  (a.k.a. enoki::CUDAArray<float>)
    # Vector3f = enoki.cuda.Vector3f (a.k.a. enoki::Array<enoki::CUDAArray<float>, 3>>)

In some cases, it may be desirable to work with *scalar* numbers and vectors even working
with a vectorized backend. Simply add the ``Scalar`` prefix before any type name in this
case.

.. code-block:: python

    mitsuba.set_variant('gpu_rgb')
    from mitsuba.core import ScalarFloat, ScalarVector3f
    # ScalarFloat    = float
    # ScalarVector3f = enoki.scalar.Vector3f (a.k.a. enoki::Array<float, 3>>)

Altogether, the following basic types are provided:

.. figtable::
    :label: table-basic-types
    :caption: This table lists Mitsuba's built-in arithmetic and array types.

    .. list-table::
        :widths: 17 30
        :header-rows: 1

        * - Type name
          - Description
        * - ``Mask``
          - Result of a comparison involving an arithmetic type like ``Float``.
        * - ``Float``
          - Default floating point type (which could be single or double precision)
        * - ``Float32``
          - Single precision floating point type
        * - ``Float64``
          - Double precision floating point type
        * - ``UInt32``
          - Unsigned 32-bit integer
        * - ``Int32``
          - Signed 32-bit integer
        * - ``UInt64``
          - Unsigned 64-bit integer
        * - ``Int64``
          - Signed 64-bit integer
        * - ``Normal3f``
          - 3D normal vector
        * - ``Color[0-4]f``
          - Color vector with floating point components of the default precision (0 to 4 dimensions).
        * - ``Vector[0-4]f``
          - Vector with floating point components of the default precision (0 to 4 dimensions)
        * - ``Point[0-4]f``
          - Point with floating point components of the default precision (0 to 4 dimensions)
        * - ``Vector[0-4]i``
          - Vector with signed 32-bit integer components (0 to 4 dimensions)
        * - ``Point[0-4]i``
          - Point with signed 32-bit integer components (0 to 4 dimensions)
        * - ``Vector[0-4]u``
          - Vector with unsigned signed 32-bit integer components (0 to 4 dimensions)
        * - ``Point[0-4]u``
          - Point with unsigned signed 32-bit integer components (0 to 4 dimensions)
        * - ``Matrix[2-4]f``
          - Matrix with floating point components of the default precision (2 to 4 dimensions)

In the following Python snippet, we show how one can use those aliases to write
generic code that can run on the CPU or the GPU, depending on the chosen
variant.

.. code-block:: python

    import enoki as ek
    import mitsuba

    # Choose the variant
    mitsuba.set_variant('packet_rgb') # also works on the GPU, e.g. with 'gpu_rgb'

    from mitsuba.core import Float, UInt64, Vector2f, PCG32

    # PCG32 is a pseudo-random number generator.
    # Configure it for returning 1000 values at a time
    rng = PCG32(initseq=ek.arange(UInt64, 1000))

    # Generate 1000 uniform random variates on [0, 1]^2
    samples = Vector2f(rng.next_float32(), rng.next_float32())

    # Warp the uniform variates into uniformly distributed points on the sphere
    pos = mitsuba.core.warp.square_to_uniform_sphere(samples)


NumPy and PyTorch integration
-----------------------------

Enoki arrays interoperate with standard Python array libraries like NumPy
PyTorch. For instance, in the previous example, we could have replaced the
assignment to the ``samples`` variable by

.. code-block:: python

    import numpy as np
    samples = np.random.random((sample_count, 2))

and the subsequent ``square_to_uniform_sphere`` call would have performed an
implicit conversion. Similarly, Enoki arrays can be cast into PyTorch or NumPy
arrays and plotted using libraries like Matplotlib.

Submodules
----------

The Mitsuba Python bindings are split into three submodules:

.. list-table::
    :widths: 30 70
    :header-rows: 1

    * - Submodule name
      - Description
    * - ``mitsuba.core``
      - Python bindings for the :monosp:`libcore` C++ library, which contains
        core functionality that is not directly related to rendering
        algorithms. (→ :ref:`sec-api-core`)
    * - ``mitsuba.render``
      - Python bindings for the :monosp:`librender` C++ library, which contains
        interfaces of components like rendering algorithms, sensors, emitters,
        textures, participating media, etc. (→ :ref:`sec-api-render`)
    * - ``mitsuba.python``
      - Higher-level functionality that is developed in Python: infrastructure
        for automatic differentiation, testing (Chi^2 test), etc. (→
        :ref:`sec-api-python`)


The :ref:`API reference <sec-api>` provides further details on their contents.
