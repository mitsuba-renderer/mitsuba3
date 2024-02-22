#pragma once

#include <atomic>
#include <stdexcept>
#include <mitsuba/core/class.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Object base class with builtin reference counting
 *
 * This class (in conjunction with the ``ref`` reference counter) constitutes the
 * foundation of an efficient reference-counted object hierarchy. The
 * implementation here is an alternative to standard mechanisms for reference
 * counting such as ``std::shared_ptr`` from the STL.
 *
 * Why not simply use ``std::shared_ptr``? To be spec-compliant, such shared
 * pointers must associate a special record with every instance, which stores
 * at least two counters plus a deletion function. Allocating this record
 * naturally incurs further overheads to maintain data structures within the
 * memory allocator. In addition to this, the size of an individual
 * ``shared_ptr`` references is at least two data words. All of this quickly
 * adds up and leads to significant overheads for large collections of
 * instances, hence the need for an alternative in Mitsuba.
 *
 * In contrast, the ``Object`` class allows for a highly efficient
 * implementation that only adds 64 bits to the base object (for the counter)
 * and has no overhead for references. In addition, when using Mitsuba in
 * Python, this counter is shared with Python such that the ownerhsip and
 * lifetime of any ``Object`` instance across C++ and Python is managed by it.
 */
class MI_EXPORT_LIB Object : public nanobind::intrusive_base {
public:
    /// Default constructor
    Object() { }

    /// Copy constructor
    Object(const Object &) { }

    /// Destructor
    ~Object() { };

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
     * \brief Return a \ref Class instance containing run-time type information
     * about this Object
     * \sa Class
     */
    virtual const Class *class_() const;

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

private:
    static Class *m_class;
};

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
    Differentiable = 0x0,

    /// Tracking gradients w.r.t. this parameter is not allowed
    NonDifferentiable = 0x1,

    /// Tracking gradients w.r.t. this parameter will introduce discontinuities
    Discontinuous = 0x2,
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
                throw("Parameter can't be differentiable because of its type!");
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

// This checks that it is safe to reinterpret a \c ref object into the
// underlying pointer type.
static_assert(sizeof(ref<Object>) == sizeof(Object *),
              "ref<T> must be reinterpretable as a T*.");

NAMESPACE_END(mitsuba)

// Necessary when printing jit arrays of pointers to mitsuba::Object
namespace drjit {
    template <typename Array>
    struct call_support<mitsuba::Object, Array> {
        static constexpr const char *Domain = "mitsuba::Object";
    };
}
