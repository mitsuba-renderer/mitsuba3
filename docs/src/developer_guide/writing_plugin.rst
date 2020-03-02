Writing plugins in C++
======================

Plugin example
**************

A basic plugin definition might look as follows:

.. code-block:: c++

    NAMESPACE_BEGIN(mitsuba)

    template <typename Float, typename Spectrum>
    class MyPlugin : public PluginInterface<Float, Spectrum> {
    public:
        MTS_IMPORT_BASE(PluginInterface, m_some_member, some_method)
        MTS_IMPORT_TYPES()

        MyPlugin();

        auto foo(..., Mask active) {
            MTS_MASKED_FUNCTION(ProfilerPhase::MyPhase, active)
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
******

:monosp:`MTS_MASK_ARGUMENT(Mask)`
---------------------------------

This macro disables the *mask logic* for any :monosp:`scalar` variants of the renderer for
performance reasons. It will force the ``Mask`` variable to be ``true`` at compile time in order to
help the compiler to perform more branch eliminations.


:monosp:`MTS_MASKED_FUNCTION(Phase, Mask)`
------------------------------------------

This macro should be invoked at the begining of any methods of a pluging taking a ``Mask`` argument
(usually named ``active``). It serves two purposes:

1. Initializes a profiler phase for the scope of this function. The ``Phase`` argument must be a
   valid ``ProfilerPhase`` handled by the ``Profiler`` (see ``profiler.h``).
2. Invokes the ``MTS_MASK_ARGUMENT`` macro described above to disable the *mask logic* for any
   :monosp:`scalar` variants of the renderer.


:monosp:`MTS_IMPORT_BASE(Name, ...)`
------------------------------------

This macro will import the desired methods and fields by generating a sequence of ``using``
declarations. This is useful when inheriting from template parents, since methods and fields must be
explicitly made visible.

For example:

.. code-block:: c++

    MTS_IMPORT_BASE(Name, m_some_member, some_method)

    // expands to:

    using Base = Name<Float, Spectrum>;
    using Base::m_some_member;
    using Base::some_method;


:monosp:`MTS_IMPORT_CORE_TYPES()`
---------------------------------

This macro will generate a sequence of ``using`` declarations for all the Mitsuba *core* templated
types (e.g. ``Vector{1/2/3}{i/u/f/d}``, ``Point{1/2/3}{i/u/f/d}``, ...), using the right
variant template parameter ``Float``.

.. note:: A type alias declarations for ``Float`` must exist preceding any call to this macro.

For example:

.. code-block:: c++

    using Float = float;

    MTS_IMPORT_CORE_TYPES()

    // expands to:

    // ...
    using Point2f = Point<Float, 2>;
    using Point3f = Point<Float, 3>;
    // ...
    using BoundingBox3f = BoundingBox<Point3f>;
    // ...


:monosp:`MTS_IMPORT_TYPES(...)`
-------------------------------

This macro is invokes ``MTS_IMPORT_CORE_TYPES()`` and also adds ``using`` declarations for the
Mitsuba *render* templated types, such as ``Ray3f``, ``SurfaceInteraction3f``, ``BSDF``, etc. These
templated aliases will depend on the preceding declaration of the ``Float`` and ``Spectrum`` types.

It is also possible to pass other types as arguments, for which templated aliases will be create as well:

.. code-block:: c++

    using Float    = float;
    using Spectrum = Spectrum<Float, 4>;

    MTS_IMPORT_TYPES(MY_TYPE1, MY_TYPE2)

    // expands to:

    MTS_IMPORT_CORE_TYPES()
    // ...
    using Ray3f = Ray<Point<Float, 3>, Spectrum>;
    // ...
    using SurfaceInteraction3f = SurfaceInteraction<Float, Spectrum>;
    // ...
    using MY_TYPE1 = MY_TYPE1<Float, Spectrum>; // alias for the optional parameters
    using MY_TYPE2 = MY_TYPE2<Float, Spectrum>;


:monosp:`MTS_DECLARE_CLASS()`
-----------------------------

This macro should be invoked in the :monosp:`class` declaration of the plugin. It will declare RTTI
(run-time type information) data structures useful for doing things like:

- Checking if an object derives from a certain :monosp:`class`
- Determining the parent of a :monosp:`class` at runtime
- Instantiating a :monosp:`class` by name
- Unserializing a :monosp:`class` from a binary data stream


:monosp:`MTS_IMPLEMENT_CLASS_VARIANT(Name, Parent)`
---------------------------------------------------

This macro should be invoked in the main implementation ``.cpp`` file of any plugin. It will
statically initialize the RTTI data structures for this plugin when lauching the renderer.

- The ``Name`` argument should be the name of the plugin :monosp:`class`.
- The ``Parent`` argument should take the name of the plugin interface :monosp:`class`.


:monosp:`MTS_EXPORT_PLUGIN(Name, Descr)`
----------------------------------------

This macro will explicitly instantiate all enabled variants of a plugin:

.. code-block:: c++

    MTS_EXPORT_PLUGIN(Name, Descr)

    // expands to:

    template class MTS_EXPORT Name<float, Color<float, 1>>    // scalar_mono
    template class MTS_EXPORT Name<float, Spectrum<float, 4>> // scalar_spectral
    // ...

This macro is necessary as the plugin interfaces (e.g. ``BSDF``) invoke the
``MTS_EXTERN_CLASS_RENDER(Name)`` macro which declare that a template of this :monosp:`class` is to
be imported and not instantiated.

The ``Descr`` :monosp:`string` argument is used to write a more verbose description of the plugin in
the generated DLL.
