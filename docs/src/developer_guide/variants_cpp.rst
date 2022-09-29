.. _sec-variants-cpp:

Variants in C++
===============

As described in the section on :ref:`choosing variants <sec-variants>`, Mitsuba
3 code can be compiled into different variants, which are parameterized by
their computational backend and representation of color. To enable such
retargeting from a single implementation, the system relies on C++ templates
and metaprogramming. Indeed, most C++ classes and functions in Mitsuba 3 are
templates with the following two type parameters:

- ``Float`` and
- ``Spectrum``.

These two parameters exactly correspond to the previously mentioned
computational backend and color representation. During compilation, Mitsuba's
build system reads the ``mitsuba.conf`` file and substitutes the types of
selected variants into these template parameters. For example,

.. code-block:: text

    "scalar_rgb": {
        "float": "float",
        "spectrum": "Color<Float, 3>"
    },

causes an
`explicit template instantiation <https://en.cppreference.com/w/cpp/language/class_template#Explicit_instantiation>`_
with

.. code-block::

    Float    = float;
    Spectrum = Color<Float, 3>;

The resulting C++ symbols will be added to the shared libraries
(``dist/libmitsuba.so``, ``dist/plugin/*.so``, ...). At runtime, the
user-specified variant will determine the set of symbols to be used by the
renderer.

Type aliases
------------

Of course, ``Float`` and ``Spectrum`` are not the only types that are used in a
renderer. It also access to integer arithmetic types, vectors, normals, points,
rays, matrices, and so on. More complex data structures like intersection and
sampling records are also commonly used.

It would be tedious to have to define these types in the context of a given
variant. To facilitate this process, Mitsuba provides a helper macro to import
suitable types that are inferred from the definition of ``Float`` and
``Spectrum``.

For example, after evaluating this macro at the beginning of a templated
function, we are then able to use other templated Mitsuba types (e.g.
`Vector2f`, `Ray3f`, `SurfaceInteraction`, ...) as if they were not templated.

.. code-block:: c++

    template <typename Float, typename Spectrum>
    void my_function() {
        /// Import type aliases (e.g. using Vector3f = Vector<Float, 3>;)
        MI_IMPORT_TYPES()

        // Can now use those types as if they were not templated
        Point3f p(4.f, 3.f, 0.f);
        Vector3f v(1.f, 0.f, 0.f);
        Ray3f ray(p, v);
        std::cout << ray << std::endl;
    }

.. note::

    The ``MI_VARIANT`` macro is often used as a shorthand notation instead of
    the somewhat verbose ``template <typename Float, typename Spectrum>``.

Those macros are described in more detail in :ref:`this section
<sec-plugin-macros-cpp>`.


Branching and masking
---------------------

When dealing with vectorized computational backends (e.g. ``llvm_*``,
``cuda_*``), additional scrutiny is needed to adapt C++ branching logic (in
particular, ``if`` statements).

Consider the result of a ray intersection in scalar mode. The resulting
:py:class:`~mitsuba.SurfaceInteraction3f` record holds information
concerning a single surface intersection. In this case, conditional logic works
fine using normal ``if`` statements.

On the other hand, the same data structure in a vectorized backend (e.g.
``cuda_rgb``) holds information concerning *many* surface intersections. Since
any condition may only be |true| for a subset of the elements, conditionals
logic can no longer be carried out using ordinary ``if`` statements.

The alternative operation ``dr::select(mask, arg1, arg2)`` takes a *mask*
argument (typically the result of a comparison) and evaluates ``(mask ? arg1 :
arg2)`` in parallel for each lane. We refer to `Dr.Jit's documentation
<https://enoki.readthedocs.io/en/master/basics.html#working-with-masks>`_ for
further information on working with masks. The following shows an example
contrasting these two cases:

.. code-block:: cpp

    // --------------------
    // Scalar code (discouraged)

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

    return dr::select(si.is_valid(), 1.0f, 0.f);

Moreover, most of the functions/methods take an *optional* `active` parameter
that encodes which *lanes* remain active. In the example above, we can e.g.
provide this information to the ``ray_intersect`` routine to avoid computation
(particularly, memory reads) associated with invalid entries. The updated code
then reads:

.. code-block:: cpp

    // Mask specifying the active lanes
    Mask active = ...;

    Scene scene = ...;
    Ray3f ray = ...;
    SurfaceInteraction3f si = scene->ray_intersect(ray, active);

    return dr::select(active & si.is_valid(), 1.0f, 0.f);

JIT backend synchronization point
---------------------------------

As described in `Dr.Jit's documentation
<https://enoki.readthedocs.io/en/master/gpu.html#suggestions-regarding-horizontal-operations>`_,
the ``cuda`` and ``llvm`` computational backends rely on a JIT compiler that
dynamically generates kernels using NVIDIA's PTX intermediate language. This JIT
compiler is highly efficient for *vertical* operations (additions,
multiplications, gathers, scatters, etc.).  However, applying a *horizontal*
operations (e.g. ``dr::any()``, ``dr::all()``, ``dr::hsum()``, etc.) to a
``JITArray<T>`` will flush all currently queued computations, which limits the
amount of parallelism.

In many cases, horizontal mask-related operations can safely be skipped if this
yields a performance benefit. For this reason, the Mitsuba 3 codebase makes
frequent use of alternative reduction operations (``any_or<>()``,
``all_or<>()``, ...) that skip evaluation on GPU targets.

For example, the code ``...`` in the example below will
only be executed if ``condition`` is ``true`` in ``scalar_*`` variants.

.. code-block:: cpp

    Mask condition = ...;
    if (any_or<true>(condition)) {
        ...
    }

In ``cuda`` and ``llvm`` variants, we are typically working with arrays
containing millions of elements, and it is quite likely that at least of one of
the array entries will in any case trigger execution of the ``...``. The
``any_or<true>(condition)`` then skips the costly horizontal reduction and
always assumes the condition to be true.

Pointer types
-------------

The ``MI_IMPORT_TYPES`` macro also imports variant-specific type aliases for
pointer types. This is important: for example, consider the ``BSDF`` associated
with a surface intersection. In a scalar variant , this is nicely represented
using a ``const BSDF *`` pointer. However, on a vectorized variants (``cuda_*``,
``llvm_*``), the intersection is in fact an array of many intersections, and
the simple pointer is therefore replaced by an *array of pointers**. These
pointer aliases are used as follows:

.. code-block:: c++

    // Imports BSDFPtr, EmitterPtr, etc..
    MI_IMPORT_TYPES()

    Scene scene = ...;
    Mask active = ...;
    Ray3f ray = ...;
    SurfaceInteraction3f si = scene->ray_intersect(ray, active);

    // Array of pointers if Float is an array
    BSDFPtr bsdf = si.bsdf();

    // Dr.Jit is able to dispatch method calls involving arrays of pointers
    bsdf->eval(..., active);

More information on vectorized method calls is provided in the `Dr.Jit
documentation <https://enoki.readthedocs.io/en/master/calls.html>`_.

Variant-specific code
---------------------

The C++17 ``if constexpr`` statement is often used throughout the codebase to
restrict code fragments to specific variants. For instance the following C++
snippet converts a spectrum to an XYZ tristimulus value, which crucially
depends on the color representation of the variant being compiled.

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

Since ``if constexpr`` is resolved at compile-time, this branch does not cause
any runtime overheads. Another useful feature of ``if constexpr`` is that it
suppresses compilation errors in disabled branches. This makes it possible to
write generic code that could potentially produce compilation errors when expressed
using ordinary (non-``constexpr``) ``if`` statements (for example, by accessing
a member of a class that may not exist in all variants).

Mitsuba provides various *type-traits* such as ``is_monochromatic_v`` to query
variant-specific properties. They can be found in
:file:`include/mitsuba/core/traits.h`.
