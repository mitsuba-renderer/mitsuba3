#pragma once

#define NB_INTRUSIVE_EXPORT MI_EXPORT

#include <mitsuba/mitsuba.h>
#include <nanobind/intrusive/ref.h>
#include <typeinfo>
#include <string>
#include <string_view>
#include <stdexcept>
#include <drjit/traversable_base.h>

NAMESPACE_BEGIN(mitsuba)

template <typename T> using ref = nanobind::ref<T>;

/**
 * \brief Available scene object types
 *
 * This enumeration lists high-level interfaces that can be implemented
 * by Mitsuba scene objects. The scene loader uses these to ensure
 * that a loaded object matches the expected interface.
 *
 * Note: This enum is forward-declared at the beginning of the file to allow
 * its usage in macros that appear before the full definition.
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

    /// Generates sample positions and directions, subclasses \ref Sampler
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
 * The counter provided by ``drjit::TraversableBase`` establishes a unified
 * reference count that is consistent across both C++ and Python. It is more
 * efficient than ``std::shared_ptr<T>`` (as no external control block is
 * needed) and works without Python actually being present.
 *
 * Object subclasses are *traversable*, that is, they expose methods that
 * Dr.Jit can use to walk through object graphs, discover attributes, and
 * potentially change them. This enables function freezing (@dr.freeze) that
 * must detect and apply changes when executing frozen functions.
 */
class MI_EXPORT_LIB Object : public drjit::TraversableBase {
public:
    MI_TRAVERSE_CB(drjit::TraversableBase)

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

    /// Return an identifier of the current instance (or empty if none)
    virtual std::string_view id() const;

    /// Set the identifier of the current instance (no-op if not supported)
    virtual void set_id(std::string_view id);

    /// Return the C++ class name of this object (e.g. "SmoothDiffuse")
    virtual std::string_view class_name() const;

    /// Return the instance variant (empty if this is not a variant object)
    virtual std::string_view variant_name() const;

    /// Object type of the type as static constant
    static constexpr ObjectType Type = ObjectType::Unknown;

    /// Class name of the type as static constant
    static constexpr const char *ClassName = "Object";
};

/**
 * \brief Runtime type information macro for Mitsuba classes
 *
 * This macro associates class name string with Object subclasses. This enables
 * runtime identification and more helpful log messages.
 *
 * The macro generates:
 * - A static constexpr string `ClassName` providing the stringified class name
 * - An override of the virtual `class_name()` method from the Object base class
 *
 * Example:
 * \code
 * class MyShape : public Shape<Float, Spectrum> {
 *     MI_DECLARE_CLASS(MyShape)
 *     // ... rest of the class implementation
 * };
 * \endcode
 */
#define MI_DECLARE_CLASS(Name)                                                 \
    static constexpr const char *ClassName = #Name;                            \
    virtual std::string_view class_name() const override { return ClassName; }

/**
 * \brief Macro for declaring plugin base classes with variant support
 *
 * This macro extends MI_DECLARE_CLASS() to provide additional metadata required
 * for Mitsuba's plugin base classes like (e.g., BSDF, Shape, Texture,
 * Integrator).
 *
 * The macro additionally generates:
 *
 * 1. A static constexpr string ``Variant`` identifying the variant name
 *    (e.g., ``scalar_rgb``, ``cuda_ad_rgb``).
 *
 * 2. A static constexpr string `Domain` identifying the plugin category.
 *
 * 3. A static constexpr ``Type`` member that provides the same information
 *    as an ObjectType enumeration value.
 *
 * 4. Overrides of the virtual `variant_name()` and ``type()`` method from the
 * Object base class.
 */
#define MI_DECLARE_PLUGIN_BASE_CLASS(Name)                                     \
    MI_DECLARE_CLASS(Name)                                                     \
    static constexpr const char *Variant =                                     \
        detail::variant<Float, Spectrum>::name;                                \
    static constexpr const char *Domain = #Name;                               \
    static constexpr ObjectType Type    = ObjectType::Name;                    \
    virtual std::string_view variant_name() const override { return Variant; } \
    virtual ObjectType type() const override { return Type; };

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

/// Turn an ObjectType enumeration value into string form
inline constexpr const char* object_type_name(ObjectType ot) {
    switch (ot) {
        case ObjectType::Scene: return "Scene";
        case ObjectType::Sensor: return "Sensor";
        case ObjectType::Film: return "Film";
        case ObjectType::Emitter: return "Emitter";
        case ObjectType::Sampler: return "Sampler";
        case ObjectType::Shape: return "Shape";
        case ObjectType::Texture: return "Texture";
        case ObjectType::Volume: return "Volume";
        case ObjectType::Medium: return "Medium";
        case ObjectType::BSDF: return "BSDF";
        case ObjectType::Integrator: return "Integrator";
        case ObjectType::PhaseFunction: return "Phase";
        case ObjectType::ReconstructionFilter: return "ReconstructionFilter";
        default:
            break;
    }
    return "unknown";
}

/**
 * \brief CRTP base class for JIT-registered objects
 *
 * This class provides automatic registration/deregistration with Dr.Jit's
 * instance registry for JIT-compiled variants. It uses the Curiously Recurring
 * Template Pattern (CRTP) to access static members of the derived class.
 *
 * The derived class must provide:
 * - `static constexpr const char *Variant`: variant name string
 * - `static constexpr ObjectType Type`: object type enumeration value
 * - `static constexpr const char *Domain`: string that represents the plugin category
 * - `using UInt32 = ...`: type that indicates whether this is a JIT variant
 *
 * The `MI_DECLARE_PLUGIN_BASE_CLASS()` macro ensures that these attributes are present.
 */
template <typename Derived>
class JitObject : public Object {
public:
    /// Return the identifier of this instance
    std::string_view id() const override { return m_id; }

    /// Set the identifier of this instance
    void set_id(std::string_view id) override { m_id = id; }

protected:
    /// Constructor with ID and optional ObjectType
    JitObject(std::string_view id, ObjectType type = ObjectType::Unknown) : m_id(id) {
        if constexpr (dr::is_jit_v<typename Derived::UInt32>) {
            const char *domain = type == ObjectType::Unknown
                                     ? Derived::Domain
                                     : object_type_name(type);
            jit_registry_put(Derived::Variant, domain, this);
        }
    }

    /// Copy constructor (registers the new instance)
    JitObject(const JitObject &) {
        if constexpr (dr::is_jit_v<typename Derived::UInt32>)
            jit_registry_put(Derived::Variant, Derived::Domain, this);
    }

    /// Move constructor (registers the new instance)
    JitObject(JitObject &&) noexcept {
        if constexpr (dr::is_jit_v<typename Derived::UInt32>)
            jit_registry_put(Derived::Variant, Derived::Domain, this);
    }

    /// Deregister from JIT registry on destruction
    ~JitObject() {
        if constexpr (dr::is_jit_v<typename Derived::UInt32>)
            jit_registry_remove(this);
    }

private:
    /// Stores the identifier of this instance
    std::string m_id;
};


// -----------------------------------------------------------------------------
//                 Type declarations and macros for plugins
// -----------------------------------------------------------------------------

/// Represents a function that instantiate a plugin from a \ref Properties object
using PluginInstantiateFn = ref<Object> (*)(void *payload, const Properties &);

/// Represents a function that releases the resources of a plugin. It should only
/// be called when the plugin is no longer in use. See 'plugin.cpp'.
using PluginReleaseFn = void (*)(void *payload);

/// Represents a function that can be used to register variants of a plugin
using PluginRegisterFn = void (*)(std::string_view name, std::string_view variant,
                                  ObjectType type, PluginInstantiateFn cons);

/// Represents the entry point of a plugin
using PluginEntryFn = void (*)(std::string_view name, PluginRegisterFn);

/// Entry point of plugins, registers provided classes with the plugin manager
#define MI_EXPORT_PLUGIN(Name)                                                 \
    extern "C" MI_EXPORT void init_plugin(std::string_view name,               \
                                          PluginRegisterFn fn) {               \
        MI_REGISTER_PLUGIN(fn, name, Name);                                    \
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
 * uses this mechanism for two primary purposes:
 *
 * 1. **Dynamic scene modification**: After a scene is loaded, the traversal
 *    mechanism allows programmatic access to modify scene parameters without
 *    rebuilding the entire scene. This enables workflows where parameters
 *    are adjusted and the scene is re-rendered with different settings.
 *
 * 2. **Differentiable parameter discovery**: The traversal callback can
 *    discover all differentiable parameters in a scene (e.g., material
 *    properties, transformation matrices, emission values). These parameters
 *    can then be exposed to gradient-based optimizers for inverse rendering
 *    tasks, which in practice involves the ``SceneParameters`` Python class.
 *
 * The callback receives information about each traversed object's parameters
 * through the \ref put() methods, which distinguish between regular parameters
 * and references to other scene objects that are handled recursively.
 */
class TraversalCallback {
public:
    template <typename T, typename Flags> void put(std::string_view name, ref<T> &value, Flags flags) {
        put_object(name, value.get(), (uint32_t) flags);
    }

    template <typename T, typename Flags> void put(std::string_view name, T *value, Flags flags) {
        put_object(name, value, (uint32_t) flags);
    }

    template <typename T, typename Flags> void put(std::string_view name, T &&value, Flags flags) {
        uint32_t flags_val = (uint32_t) flags;
        if constexpr (!dr::is_drjit_struct_v<T> || !(dr::is_diff_v<T> && dr::is_floating_point_v<T>))
            if ((flags_val & (uint32_t) ParamFlags::Differentiable) != 0)
                throw std::runtime_error("The specified parameter type cannot be differentiable!");
        put_value(name, &value, flags_val, typeid(T));
    }

    /// Forward declaration for field<...> values that simultaneously store host+device values
    template <typename DeviceType, typename HostType, typename SFINAE, typename Flags>
    void put(std::string_view name, field<DeviceType, HostType, SFINAE> &value, Flags flags);

    virtual ~TraversalCallback() = default;

protected:
    /// Actual implementation for regular parameters [To be provided by subclass]
    virtual void put_value(std::string_view name,
                           void *value,
                           uint32_t flags,
                           const std::type_info &type) = 0;

    /// Actual implementation for Object references [To be provided by subclass]
    virtual void put_object(std::string_view name,
                            Object *value,
                            uint32_t flags) = 0;
};

/// Prints the canonical string representation of an object instance
MI_EXPORT_LIB std::ostream& operator<<(std::ostream &os, const Object *object);

/// Prints the canonical string representation of an object instance
template <typename T>
std::ostream& operator<<(std::ostream &os, const ref<T> &object) {
    return operator<<(os, object.get());
}

NAMESPACE_END(mitsuba)
