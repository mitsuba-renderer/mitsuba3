#pragma once

#define NB_INTRUSIVE_EXPORT MI_EXPORT

#include <mitsuba/mitsuba.h>
#include <nanobind/intrusive/ref.h>
#include <typeinfo>
#include <string>
#include <string_view>

NAMESPACE_BEGIN(mitsuba)

/// Helper macro to declare a class
#define MI_DECLARE_CLASS(Name)                                                 \
    static constexpr const char *ClassName = #Name;

template <typename T> using ref = nanobind::ref<T>;

/**
 * \brief Object base class with builtin reference counting
 *
 * This class (in conjunction with the ``ref`` reference counter) constitutes
 * the foundation of an efficient reference-counted object hierarchy.
 *
 * We use an intrusive reference counting approach to avoid various gnarly
 * issues that arise in combined Python/C++ codebase, see the following page for
 * details: https://nanobind.readthedocs.io/en/latest/ownership_adv.html
 *
 * The counter provided by ``nb::intrusive_base`` establishes a unified
 * reference count that is consistent across both C++ and Python. It is more
 * efficient than ``std::shared_ptr<T>`` (as no external control block is
 * needed) and works without Python actually being present.
 */
class MI_EXPORT_LIB Object : public nanobind::intrusive_base {
public:
    /// Import default constructors
    Object() = default;
    Object(const Object &) = default;
    Object(Object &&) = default;

    /**
     * \brief Expand the object into a list of sub-objects and return them
     *
     * In some cases, an \ref Object instance is merely a container for a
     * number of sub-objects. In the context of Mitsuba, an example would be a
     * combined sun & sky emitter instantiated via XML, which recursively
     * expands into a separate sun & sky instance. This functionality is
     * supported by any Mitsuba object, hence it is located this level.
     */
    virtual std::vector<ref<Object>> expand() const;

    /**
     * \brief Traverse the attributes and object graph of this instance
     *
     * Implementing this function enables recursive traversal of C++ scene
     * graphs. It is e.g. used to determine the set of differentiable
     * parameters when using Mitsuba for optimization.
     *
     * \remark The default implementation does nothing.
     *
     * \sa TraversalCallback
     */
    virtual void traverse(TraversalCallback *cb);

    /**
     * \brief Update internal state after applying changes to parameters
     *
     * This function should be invoked when attributes (obtained via \ref
     * traverse) are modified in some way. The object can then update its
     * internal state so that derived quantities are consistent with the
     * change.
     *
     * \param keys
     *     Optional list of names (obtained via \ref traverse) corresponding
     *     to the attributes that have been modified. Can also be used to
     *     notify when this function is called from a parent object by adding
     *     a "parent" key to the list. When empty, the object should assume
     *     that any attribute might have changed.
     *
     * \remark The default implementation does nothing.
     *
     * \sa TraversalCallback
     */
    virtual void parameters_changed(const std::vector<std::string> &/*keys*/ = {});

    /**
     * \brief Return a human-readable string representation of the object's
     * contents.
     *
     * This function is mainly useful for debugging purposes and should ideally
     * be implemented by all subclasses. The default implementation simply
     * returns <tt>MyObject[<address of 'this' pointer>]</tt>, where
     * <tt>MyObject</tt> is the name of the class.
     */
    virtual std::string to_string() const;

    /// Return the object type. The default is \ref ObjectType::Unknown.
    virtual ObjectType type() const;

    /// Return the instance variant (or NULL, if this is not a variant object)
    virtual std::string_view variant() const;

    MI_DECLARE_CLASS(Object)
};

/**
 * \brief Available scene object types
 *
 * This enumeration lists high-level interfaces that can be implemented
 * by Mitsuba scene objects. The scene loader to uses these to ensure
 * that a loaded object matches the expected interface.
 */
enum class ObjectType : uint32_t {
    /// The default returned by Object subclasses
    Unknown,

    /// The top-level scene object. No subclasses exist
    Scene,

    /// A filter used to reconstruct/resample images
    ReconstructionFilter,

    /// Carries out radiance measurements, subclasses \ref Sensor
    Sensor,

    /// Storage representation of the sensor
    Film,

    /// Emits radiance, subclasses \ref Emitter
    Emitter,

    /// Sensor,
    Sampler,

    /// Denotes an arbitrary shape (including meshes)
    Shape,

    /// A 2D texture data source
    Texture,

    /// A 3D volume data source
    Volume,

    /// A participating medium
    Medium,

    /// A bidirectional reflectance distribution function
    BSDF,

    /// A phase function characterizing scattering in volumes
    PhaseFunction,

    /// A rendering algorithm aka. Integrator
    Integrator
};

namespace detail {
    /// Helper partial template overload to determine the variant string
    /// associated with a given floating point and spectral type

    template <typename Float, typename Spectrum> struct variant {
        static constexpr const char *name = nullptr;
    };

    MI_VARIANT_TEMPLATE()
}

/// Global to indicate that we are not currently in a class (used by the Logger)
static constexpr const char *ClassName = nullptr;

/// Helper macro to a variant-specific plugin base class
#define MI_DECLARE_PLUGIN_BASE_CLASS(Name)                                     \
    static constexpr ObjectType Type       = ObjectType::Name;                 \
    static constexpr const char *ClassName = #Name;                            \
    virtual ObjectType type() const override { return Type; };                 \

/**
 * \brief Variant-specific object base class
 *
 * Mitsuba scene objects are parameterized by a *variant*, which refers to a
 * floating point and radiance representation driving the underlying computation.
 *
 * Objects indicate their variant by deriving from the VariantObject base class.
 *
 * In JIT-compiled variants of Mitsuba, the variant object automatically registers
 * the scene object with Dr.Jit's instance registry, which is required to dispatch
 * polymorphic method calls.
 */
template <typename Float_, typename Spectrum_>
class VariantObject : public Object {
public:
    using Float = Float_;
    using Spectrum = Spectrum_;

    /// String identifying the (Float, Spectrum) variant
    static constexpr const char *Variant = detail::variant<Float, Spectrum>::name;

    /// Construct the variant object and potentially register it
    VariantObject();

    /**
     * \brief Construct the variant object and potentially register it
     *
     * This version of the constructor checks if the object has an identifier
     * and preserves it in that case. Use \ref id() to later query this
     * identifier.
     */
    VariantObject(const Properties &props);

    /// Copy constructor
    VariantObject(const VariantObject &);

    /// Move constructor
    VariantObject(VariantObject &&);

    /// Destruct the variant object and potentially deregister it
    ~VariantObject();

    /// Virtual method that returns the instance's variant name or NULL
    virtual std::string_view variant() const override;

    /// Return an identifier of the current instance
    virtual std::string_view id() const;

protected:
    char *m_id;
};

// -----------------------------------------------------------------------------
//                 Type declarations and macros for plugins
// -----------------------------------------------------------------------------

/// Represents a function that instantiate a plugin from a \ref Properties object
using PluginInstantiateFn = ref<Object> (*)(void *payload, const Properties &);

/// Reprsents a function that releases the resources of a plugin. It should only
/// be called when the plugin is no longer in use. See 'plugin.cpp'.
using PluginReleaseFn = void (*)(void *payload);

/// Represents a function that can be used to register variants of a plugin
using PluginRegisterFn = void (*)(std::string_view name, std::string_view variant,
                                  PluginInstantiateFn cons);

/// Represents the entry point of a plugin
using PluginEntryFn = void (*)(std::string_view name, PluginRegisterFn);

/// Entry point of plugins, registers provided classes with the plugin manager
#define MI_EXPORT_PLUGIN(Name)                                                 \
    extern "C" MI_EXPORT void init_plugin(std::string_view name,               \
                                          PluginRegisterFn fn) {               \
        MTS_REGISTER_PLUGIN(cb, name, Name);                                   \
    }

// -----------------------------------------------------------------------------
//                          Scene Traversal API
// -----------------------------------------------------------------------------

/**
 * \brief This list of flags is used to classify the different types of
 * parameters exposed by the plugins.
 *
 * For instance, in the context of differentiable rendering, it is important to
 * know which parameters can be differentiated, and which of those might
 * introduce discontinuities in the Monte Carlo simulation.
 */
enum class ParamFlags : uint32_t {
    /// Tracking gradients w.r.t. this parameter is allowed
    Differentiable = 0,

    /// Tracking gradients w.r.t. this parameter is not allowed
    NonDifferentiable = 1,

    /// Tracking gradients w.r.t. this parameter will introduce discontinuities
    Discontinuous = 0x2,

    /// This parameter is read-only
    ReadOnly = 0x4,
};

MI_DECLARE_ENUM_OPERATORS(ParamFlags)

/**
 * \brief Abstract class providing an interface for traversing scene graphs
 *
 * This interface can be implemented either in C++ or in Python, to be used in
 * conjunction with \ref Object::traverse() to traverse a scene graph. Mitsuba
 * currently uses this mechanism to determine a scene's differentiable
 * parameters.
 */
class TraversalCallback {
public:
    /// Inform the traversal callback about an attribute of an instance
    template <typename T>
    void put_parameter(const std::string &name, T &v, uint32_t flags) {
        if constexpr (!dr::is_drjit_struct_v<T> || !(dr::is_diff_v<T> && dr::is_floating_point_v<T>))
            if ((flags & ParamFlags::Differentiable) != 0)
                throw std::runtime_error("The specificied parameter type cannot be differentiable!");
        put_parameter_impl(name, &v, flags, typeid(T));
    }

    /// Inform the traversal callback that the instance references another Mitsuba object
    virtual void put_object(const std::string &name, Object *obj,
                            uint32_t flags) = 0;

    virtual ~TraversalCallback() = default;
protected:
    /// Actual implementation of \ref put_parameter(). [To be provided by subclass]
    virtual void put_parameter_impl(const std::string &name,
                                    void *ptr,
                                    uint32_t flags,
                                    const std::type_info &type) = 0;
};

/// Prints the canonical string representation of an object instance
MI_EXPORT_LIB std::ostream& operator<<(std::ostream &os, const Object *object);

/// Prints the canonical string representation of an object instance
template <typename T>
std::ostream& operator<<(std::ostream &os, const ref<T> &object) {
    return operator<<(os, object.get());
}

MI_EXTERN_CLASS(VariantObject)

NAMESPACE_END(mitsuba)
