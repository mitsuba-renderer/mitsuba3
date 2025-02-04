#pragma once

#define NB_INTRUSIVE_EXPORT MI_EXPORT

#include <mitsuba/mitsuba.h>
#include <nanobind/intrusive/ref.h>
#include <typeinfo>

NAMESPACE_BEGIN(mitsuba)

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
    /// Default constructor
    Object() = default;

    /// Copy constructor
    Object(const Object &) = default;

    /// Destructor
    ~Object();

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

    /// Return an identifier of the current instance (if available)
    virtual std::string id() const;

    /// Set an identifier to the current instance (if applicable)
    virtual void set_id(const std::string& id);

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

    /// Return the type of this object
    virtual ObjectType type() const;

    /// Static class name identifier
    static constexpr const char *ClassName = "Object";

    /// Virtual method that returns 'ClassName' (mainly used for logging)
    virtual const char *class_name() const;
};

/// This enumeration lists high-level interfaces that can be implemented
/// by Mitsuba scene objects. The scene loader to uses these to ensure
/// that a loaded object matches the expected interface.
enum class ObjectType : uint32_t {
    /// The default returned by Object subclasses
    Unknown,
    /// The top-level scene object. No subclasses exist
    Scene,
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
    /// A rendering algorithm aka. Integrator
    Integrator,
    /// A phase function characterizing scattering in volumes
    PhaseFunction,
    /// A filter used to reconstruct/resample images
    ReconstructionFilter
};


/// Global to indicate that we are not currently in a class
static constexpr const char *ClassName = nullptr;

#define MI_DECLARE_CLASS(Name)                                                 \
    static constexpr const char *ClassName = #Name;                            \
    virtual const char *class_name() const override { return ClassName; };

// -----------------------------------------------------------------------------
//                 Type declarations and macros for plugins
// -----------------------------------------------------------------------------

/// Represents a function that instantiate a plugin from a \ref Properties object
using PluginInstantiateFn = ref<Object> (*)(void *payload, const Properties &);

/// Reprsents a function that releases the resources of a plugin. It should only
/// be called when the plugin is no longer in use. See 'plugin.cpp'.
using PluginReleaseFn = void (*)(void *payload);

/// Represents a function that can be used to register variants of a plugin
using PluginRegisterFn = void (*)(const char *name, const char *variant,
                                  PluginInstantiateFn cons);

/// Represents the entry point of a plugin
using PluginEntryFn = void (*)(const char *name, PluginRegisterFn);

/// Entry point of plugins, registers provided classes with the plugin manager
#define MI_EXPORT_PLUGIN(Name)                                                 \
    extern "C" MI_EXPORT void init_plugin(const char *name,                    \
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
    Discontinuous = 2,
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

/// Register an instance with the Dr.Jit instance registry (for symbolic calls)
#define MI_REGISTER_OBJECT(name, self)                                         \
    if constexpr (dr::is_jit_v<Float>)                                         \
        jit_registry_put(detail::get_variant<Float, Spectrum>(),               \
                         "mitsuba::" name, self);

/// Unregister an instance from the Dr.Jit instance registroy
#define MI_UNREGISTER_OBJECT(self)                                             \
    if constexpr (dr::is_jit_v<Float>)                                         \
        jit_registry_remove(self);

#define MI_CALL_TEMPLATE_BEGIN(Name) DRJIT_CALL_TEMPLATE_BEGIN(mitsuba::Name)
#define MI_CALL_TEMPLATE_END(Name)                                             \
public:                                                                        \
    static constexpr const char *variant_() {                                  \
        return ::mitsuba::detail::get_variant<Ts...>();                        \
    }                                                                          \
    static_assert(is_detected_v<detail::has_variant_override, CallSupport_>);  \
    DRJIT_CALL_END(mitsuba::Name)



NAMESPACE_END(mitsuba)

// Necessary when printing Dr.Jit arrays of mitsuba::Object* pointers
namespace drjit {
    template <typename Array>
    struct call_support<mitsuba::Object, Array> {
        // This is for pointers to general `Object` instances, we don't have access
        // to specific `Float` and `Spectrum` types here.
        static constexpr const char *Variant = "";
        static constexpr const char *Domain = "mitsuba::Object";
    };
}

