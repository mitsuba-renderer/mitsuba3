Plugins & Macros
================

This section briefly reviews the "skeleton" of a basic plugin definition to
clarify the role of the various ``MTS_*`` macros, which are responsible for
importing types, instantiating variants, and providing run-time type
information.

Example code
------------

Consider the following hypothetical plugin ``MyPlugin``:

.. code-block:: cpp

    NAMESPACE_BEGIN(mitsuba)

    template <typename Float, typename Spectrum>
    class MyPlugin : public PluginInterface<Float, Spectrum> {
    public:
        MTS_IMPORT_BASE(PluginInterface, m_some_member, some_method)
        MTS_IMPORT_TYPES()

        MyPlugin();

        Spectrum foo(..., Mask active) const override {
            MTS_MASKED_FUNCTION(ProfilerPhase::MyEval, active)
            // ...
        }

        /// Declare RTTI data structures
        MTS_DECLARE_CLASS()
    protected:
        /// Important: declare a protected virtual destructor
        virtual ~MyPlugin();

    };

    /// Implement RTTI data structures
    MTS_IMPLEMENT_CLASS_VARIANT(MyPlugin, PluginInterface)
    MTS_EXPORT_PLUGIN(MyPlugin, "Description of my plugin")
    NAMESPACE_END(mitsuba)

Macros
------

Note that not all macros listed here actually occur in the above code example.

:monosp:`MTS_MASK_ARGUMENT(mask)`
*********************************

This macro typically occurs at the beginning of a function that takes
a mask as an argument.

.. code-block:: cpp

    void my_method(..., Mask active) {
        MTS_MASK_ARGUMENT(active);
    }

Masking is not really needed on scalar variants: if a function is called, we
assume that ``active=true``. Masking can unfortunately decrease performance in
this case due to the generation of extra branches. Here, ``MTS_MASK_ARGUMENT``
therefore expands to

.. code-block:: cpp

    if constexpr (is_scalar_v<Float>)
        active = true;

which turns the mask argument into a compile-time constant on scalar targets,
allowing the compiler to optimize away the undesired branches.
``MTS_MASK_ARGUMENT`` is also part of the next macro.


:monosp:`MTS_MASKED_FUNCTION(phase, mask)`
------------------------------------------

This macro builds on the ``MTS_MASK_ARGUMENT`` macro and typically occurs at
the beginning of a function that takes a mask as an argument.

.. code-block:: cpp

    void my_method(..., Mask mask) {
        MTS_MASKED_FUNCTION(ProfilerPhase::MyMethod, active);
    }

Masking is not really needed on scalar variants: if a function is called, we
assume that ``active=true``. Masking can unfortunately decrease performance in
this case due to the generation of extra branches. Here,
``MTS_MASKED_FUNCTION`` macro expands to

.. code-block:: cpp

    if constexpr (is_scalar_v<Float>)
        active = true;
    ScopedPhase _(ProfilerPhase::MyMethod);

which turns the mask argument into a compile-time constant on scalar targets,
allowing the compiler to optimize away the undesired branches.

Mitsuba ships with a powerful sampling profiler that facilitates tracking down
hot-spots during rendering. The last line of this macro (``ScopedPhase``)
informs this profiler that we are currently executing a function that belongs
to the profiler phase ``phase``.


:monosp:`MTS_IMPORT_BASE(Name, ...)`
------------------------------------
Because most Mitsuba classes are templates, attributes and methods of parent
classes are not visible by default. They can be imported explicitly via ``using
Base::some_method;`` statements, but writing many such statements is tiresome.
The variadic macro ``MTS_IMPORT_BASE`` expands into arbitrarily many such
``using`` statements. For example,

.. code-block:: cpp

    MTS_IMPORT_BASE(Name, m_some_member, some_method)

expands to

.. code-block:: cpp

    using Base = Name<Float, Spectrum>;
    using Base::m_some_member;
    using Base::some_method;

.. _macro-import-core-types:

:monosp:`MTS_IMPORT_CORE_TYPES()`
---------------------------------

This macro will generate a sequence of ``using`` declarations to import the
Mitsuba core types (e.g. ``Vector{1/2/3}{i/u/f/d}``, ``Point{1/2/3}{i/u/f/d}``,
...). They are automatically inferred from the definition of ``Float``.

.. note::

    A type named ``Float`` must exist preceding evaluation of this macro.

For example,

.. code-block:: cpp

    using Float = float;

    MTS_IMPORT_CORE_TYPES()

    // expands to:

    // ...
    using Point2f = Point<Float, 2>;
    using Point3f = Point<Float, 3>;
    // ...
    using BoundingBox3f = BoundingBox<Point3f>;
    // ...


.. _macro-import-types:

:monosp:`MTS_IMPORT_TYPES(...)`
-------------------------------

This macro invokes ``MTS_IMPORT_CORE_TYPES()`` and furthermore imports
rendering-related types, such as ``Ray3f``, ``SurfaceInteraction3f``, ``BSDF``,
etc. These templated aliases will depend on the preceding declaration of the
``Float`` and ``Spectrum``.

It is also possible to pass other types as arguments, for which templated
aliases will be created:

.. code-block:: cpp

    using Float    = float;
    using Spectrum = Spectrum<Float, 4>;

    MTS_IMPORT_TYPES(MyType1, MyType2)

    // expands to:

    MTS_IMPORT_CORE_TYPES()
    // ...
    using Ray3f = Ray<Point<Float, 3>, Spectrum>;
    // ...
    using SurfaceInteraction3f = SurfaceInteraction<Float, Spectrum>;
    // ...
    using MyType1 = MyType1<Float, Spectrum>; // alias for the optional parameters
    using MyType2 = MyType2<Float, Spectrum>;


:monosp:`MTS_DECLARE_CLASS()`
-----------------------------

This macro should be invoked in the :monosp:`class` declaration of the plugin.
It will declare RTTI (run-time type information) data structures useful for
doing things like:

- Checking if an object derives from a certain :monosp:`class`
- Determining the parent of a :monosp:`class` at runtime
- Instantiating a :monosp:`class` by name
- Unserializing a :monosp:`class` from a binary data stream


:monosp:`MTS_IMPLEMENT_CLASS_VARIANT(Name, Parent)`
---------------------------------------------------

This macro should be invoked at the bottom of ``.cpp`` files that implement new
plugin classes. Its role is to initialize the RTTI data structures for this
plugin that were previously declared using ``MTS_DECLARE_CLASS()``.

- The ``Name`` argument should be the name of the plugin :monosp:`class`.
- The ``Parent`` argument should take the name of the plugin interface :monosp:`class`.


:monosp:`MTS_EXPORT_PLUGIN(Name, Descr)`
----------------------------------------

This macro will explicitly instantiate all enabled variants of a plugin:

.. code-block:: cpp

    MTS_EXPORT_PLUGIN(Name, "My fancy plugin")

    // expands to:

    template class MTS_EXPORT Name<float, Color<float, 1>>    // scalar_mono
    template class MTS_EXPORT Name<float, Spectrum<float, 4>> // scalar_spectral
    // ...

It also associates a human-readable description ``Descr`` with the plugin that
Mitsuba's graphical user interface may use in the future.
