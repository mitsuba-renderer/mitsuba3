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
 * implementation that only adds 32 bits to the base object (for the counter)
 * and has no overhead for references.
 */
class MI_EXPORT_LIB Object {
public:
    /// Default constructor
    Object() { }

    /// Copy constructor
    Object(const Object &) { }

    /// Return the current reference count
    int ref_count() const { return m_ref_count; };

    /// Increase the object's reference count by one
    void inc_ref() const { ++m_ref_count; }

    /** \brief Decrease the reference count of the object and possibly
     * deallocate it.
     *
     * The object will automatically be deallocated once the reference count
     * reaches zero.
     */
    void dec_ref(bool dealloc = true) const noexcept;

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

protected:
    /** \brief Virtual protected deconstructor.
     * (Will only be called by \ref ref)
     */
    virtual ~Object();

private:
    mutable std::atomic<int> m_ref_count { 0 };

    static Class *m_class;
};

/**
 * \brief Reference counting helper
 *
 * The \a ref template is a simple wrapper to store a pointer to an object. It
 * takes care of increasing and decreasing the object's reference count as
 * needed. When the last reference goes out of scope, the associated object
 * will be deallocated.
 *
 * The advantage over C++ solutions such as <tt>std::shared_ptr</tt> is that
 * the reference count is very compactly integrated into the base object
 * itself.
 */
template <typename T> class ref {
public:
    /// Create a <tt>nullptr</tt>-valued reference
    ref() { }

    /// Construct a reference from a pointer
    template <typename T2 = T>
    ref(T *ptr) : m_ptr(ptr) {
        static_assert(std::is_base_of_v<Object, T2>,
                      "Cannot create reference to object not inheriting from Object class.");
        if (m_ptr)
            ((Object *) m_ptr)->inc_ref();
    }

    /// Construct a reference from another convertible reference
    template <typename T2>
    ref(const ref<T2> &r) : m_ptr((T2 *) r.get()) {
        static_assert(std::is_convertible_v<T2*, T*>, "Cannot create reference to object from another unconvertible reference.");
        if (m_ptr)
            ((Object *) m_ptr)->inc_ref();
    }

    /// Copy constructor
    ref(const ref &r) : m_ptr(r.m_ptr) {
        if (m_ptr)
            ((Object *) m_ptr)->inc_ref();
    }

    /// Move constructor
    ref(ref &&r) noexcept : m_ptr(r.m_ptr) {
        r.m_ptr = nullptr;
    }

    /// Destroy this reference
    ~ref() {
        if (m_ptr)
            ((Object *) m_ptr)->dec_ref();
    }

    /// Move another reference into the current one
    ref& operator=(ref&& r) noexcept {
        if (&r != this) {
            if (m_ptr)
                ((Object *) m_ptr)->dec_ref();
            m_ptr = r.m_ptr;
            r.m_ptr = nullptr;
        }
        return *this;
    }

    /// Overwrite this reference with another reference
    ref& operator=(const ref& r) noexcept {
        if (m_ptr != r.m_ptr) {
            if (r.m_ptr)
                ((Object *) r.m_ptr)->inc_ref();
            if (m_ptr)
                ((Object *) m_ptr)->dec_ref();
            m_ptr = r.m_ptr;
        }
        return *this;
    }

    /// Overwrite this reference with a pointer to another object
    template <typename T2 = T>
    ref& operator=(T *ptr) noexcept {
        static_assert(std::is_base_of_v<Object, T2>,
                      "Cannot create reference to an instance that does not"
                      " inherit from the Object class..");
        if (m_ptr != ptr) {
            if (ptr)
                ((Object *) ptr)->inc_ref();
            if (m_ptr)
                ((Object *) m_ptr)->dec_ref();
            m_ptr = ptr;
        }
        return *this;
    }

    /// Compare this reference to another reference
    bool operator==(const ref &r) const { return m_ptr == r.m_ptr; }

    /// Compare this reference to another reference
    bool operator!=(const ref &r) const { return m_ptr != r.m_ptr; }

    /// Compare this reference to a pointer
    bool operator==(const T* ptr) const { return m_ptr == ptr; }

    /// Compare this reference to a pointer
    bool operator!=(const T* ptr) const { return m_ptr != ptr; }

    /// Access the object referenced by this reference
    T* operator->() { return m_ptr; }

    /// Access the object referenced by this reference
    const T* operator->() const { return m_ptr; }

    /// Return a C++ reference to the referenced object
    T& operator*() { return *m_ptr; }

    /// Return a const C++ reference to the referenced object
    const T& operator*() const { return *m_ptr; }

    /// Return a pointer to the referenced object
    operator T* () { return m_ptr; }

    /// Return a pointer to the referenced object
    operator const T* () const { return m_ptr; }

    /// Return a const pointer to the referenced object
    T* get() { return m_ptr; }

    /// Return a pointer to the referenced object
    const T* get() const { return m_ptr; }

    /// Check if the object is defined
    operator bool() const { return m_ptr != nullptr; }
private:
    T *m_ptr = nullptr;
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
