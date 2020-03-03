Variants in C++
===============

As described in :ref:`sec-variants`, the Mitsuba 2 codebase can be compiled in many different
variants, parameterized by their *computational backend* and the *color representation* components.

In order to achieve this *retargetable* ability with a single codebase, the system had to rely
heavily on template metaprogramming. Indeed, a large portion of the C++ classes and functions in
Mitsuba 2 are templates.
You will quickly notice that most of them are templated over the following two template parameters:

- ``Float``
- ``Spectrum``

In C++, those two template parameters embody the *computational backend* and *color representation*
components of the variants, respectively.

The C++ *meaning* of those template parameters for a specific variant is defined in the
``mitsuba.conf`` file. For instance

.. code-block:: text

    "scalar_rgb": {
        "float": "float",
        "spectrum": "Color<Float, 3>"
    },

indicates that every class / function templated over `Float` and/or `Spectrum` will be
`explicitly instantiated <https://en.cppreference.com/w/cpp/language/class_template#Explicit_instantiation>`_
with the following template parameters for the `scalar_rgb` variant:

.. code-block::

    Float    = float;
    Spectrum = Color<Float, 3>;

The resulting C++ symbols will be added to the shared libraries (``dist/libmitsuba-core.so``,
``dist/libmitsuba-render.so``, ``dist/plugin/*.so``, ...). At runtime, the user-specified variant will
determine the set of symbols to be used by the renderer.

.. note:: The ``MTS_VARIANT`` macro is a shorthand notation for
          ``template <typename Float, typename Spectum>``


Macros for aliases
------------------

The framework provides helper macros to generate a sequence of alias declarations based on
the type of `Float` and `Spectrum`. By calling this macro at the begining of a
templated function, we are then able to use other templated Mitsuba types (e.g. `Vector2f`, `Ray3f`,
`SurfaceInteraction`, ...) as if they were not templated.

.. code-block:: c++

    template <typename Float, typename Spectrum>
    void my_function() {
        # Generate aliases (e.g. using Vector3f = Vector<Float, 3>;)
        MTS_IMPORT_TYPES()

        # Can now use those types as if they were not templated
        Point3f p(4.f, 3.f, 0.f);
        Vector3f v(1.f, 0.f, 0.f);
        Ray3f ray(p, v);
        std::cout << ray << std::endl;
    }

Those macros are described in more detail in the next chapter:

- :ref:`macro-import-core-types`
- :ref:`macro-import-types`


Branching and masking
---------------------

When dealing with *array-based* computational backends (e.g. ``packet_*``, ``gpu_*``), it is
necessary to adapt C++ branching logic (e.g. ``if`` statements). For instance, as demonstrated in
the following C++ snippet, the result of a ray intersection routine (here a
:py:class:`~mitsuba.render.SurfaceInteraction3f` struct) carries information of a single
intersection. Conditional logic in this case can simply be handle using ``if`` statements.

On the other hand, with *array-based* variants (e.g. `gpu_rgb`) the ``si`` variable carries
information of an array of intersections. As a condition might only be |true| for a subset of the
intersection this array, it is not possible to rely on ``if`` statement anymore. Here we use the
``enoki::select`` routine instead.

.. code-block:: c++

    // --------------------
    // Scalar code

    Scene scene = ...;
    Ray3f ray = ...;
    SurfaceInteraction3f si = scene->ray_intersect(ray);

    if (si.is_valid())
        return 1.f;
    else
        return 0.f;

    // --------------------
    // Generic code

    Scene scene = ...;
    Ray3f ray = ...;
    SurfaceInteraction3f si = scene->ray_intersect(ray);

    return enoki::select(si.is_valid(), 1.0f, 0.f);


Moreover, most of the functions/methods in the codebase will take an *optional* `active`
parameter. It carries information about which *lanes* of the computational array are still active.

In the example above, we should indicate to the ``ray_intersect`` routine which of the lanes are
active so we do not waste time computing intersections for inactive lanes.
The code would then become:

.. code-block:: c++

    // Mask specifying the active lanes
    Mask active = ...;

    Scene scene = ...;
    Ray3f ray = ...;
    SurfaceInteraction3f si = scene->ray_intersect(ray, active);

    return enoki::select(active & si.is_valid(), 1.0f, 0.f);


For more information on how to work with masks, please refer to
`Working with masks <https://enoki.readthedocs.io/en/master/basics.html#working-with-masks>`_.


CUDA backend synchronization point
----------------------------------

As described in
`GPU Array <https://enoki.readthedocs.io/en/master/gpu.html#suggestions-regarding-horizontal-operations>`_,
the ``gpu_*`` computational backend relies on a JIT compiler that will generate PTX kernels at
run-time. Horizontal operations (e.g. ``any()``, ``hsum()``) on ``CUDAArray`` will always trigger
the generation of a kernel. This has some performance implication as each kernel introduce some overheads.
For this reason, the Mitsuba 2 codebase extensively use the Enoki alternative horizontal reductions
of masks (e.g. ``any_or()``, ``all_or()``, ...) to skip evaluation when compiling for GPU target.


Pointers
--------

The ``MTS_IMPORT_TYPES`` macro also generate alias declarations for pointer types. This is useful
for instance when a function templated over ``Float`` should return a pointer that might be
different for every lane. In this case, it is necessary to use an *array of pointers* (which is
supported by Enoki), like in the code example below:

.. code-block:: c++

    // Adds: using BSDFPtr = replace_scalar_t<Float, const BSDF *>;
    MTS_IMPORT_TYPES()

    Scene scene = ...;
    Mask active = ...;
    Ray3f ray = ...;
    SurfaceInteraction3f si = scene->ray_intersect(ray, active);

    // Array of pointers if Float is an array
    BSDFPtr bsdf = si.bsdf();

    // Enoki support method calls on array of pointers types
    bsdf->eval(...);

For more information, please refer to `Method calls <https://enoki.readthedocs.io/en/master/calls.html>`_.


Variant-specific code
---------------------

The C++17 ``if constexpr`` statement if often used throughout the codebase to write code specific to
a variant. For instance the following C++ snippet will handle differently the *spectral* result of
some computations, depending on the color representation of the variant being compiled:

.. code-block:: c++

    Ray3f ray = ...;
    Mask active = ...;
    Spectrum result = compute_stuff(ray, active);

    Color3f xyz;
    if constexpr (is_monochromatic_v<Spectrum>)
        xyz = result.x();
    else if constexpr (is_rgb_v<Spectrum>)
        xyz = srgb_to_xyz(result, active);
    else
        xyz = spectrum_to_xyz(result, ray.wavelengths, active);

The ``if constexpr`` statement being resolved at *compile-time*, this doesn't introduce any branch
in the code generated, so no performance penalty.

For this purpose, the framework implements various *type-traits* specific to the renderer, which can
be found in ``include/mitsuba/core/traits.h``.
