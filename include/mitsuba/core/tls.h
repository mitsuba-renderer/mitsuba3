#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/object.h>
#include <memory>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Flexible platform-independent thread local storage class
 *
 * This class implements a generic thread local storage object that can be used
 * in situations where the new \c thread_local keyword is not available (e.g.
 * on Mac OS, as of 2016), or when TLS object are created dynamically (which
 * \c thread_local does not allow).
 *
 * The native TLS classes on Linux/MacOS/Windows only support a limited number
 * of dynamically allocated entries (usually 1024 or 1088). Furthermore, they
 * do not provide appropriate cleanup semantics when the TLS object or one of
 * the assocated threads dies. The custom TLS code provided by this class has
 * no such limits (caching in various subsystems of Mitsuba may create a huge
 * amount, so this is a big deal), and it also has the desired cleanup
 * semantics: TLS entries are destroyed when the owning thread dies \a or when
 * the \c ThreadLocal instance is freed (whichever occurs first).
 *
 * The implementation is designed to make the ``get()`` operation as fast as as
 * possible at the cost of more involved locking when creating or destroying
 * threads and TLS objects. To actually instantiate a TLS object with a
 * specific type, use the \ref ThreadLocal class.
 *
 * \sa ThreadLocal
 */

class MTS_EXPORT_CORE ThreadLocalBase {
    friend class Thread;
public:
    /// Functor to allocate memory for a TLS object
    using ConstructFunctor = void *(*)();
    /// Functor to release memory of a TLS object
    using DestructFunctor = void (*)(void *);

    /// Construct a new thread local storage object
    ThreadLocalBase(const ConstructFunctor &constructFunctor,
                    const DestructFunctor &destructFunctor);

    /// Destroy the thread local storage object
    ~ThreadLocalBase();

    /**
     * \brief Release all current instances associated with this TLS
     *
     * Dangerous: don't use this method when the data is still concurrently
     * being used by other threads
     */
    void clear();

protected:
    /// Return the data value associated with the current thread
    void *get();

    /// Return the data value associated with the current thread (const version)
    const void *get() const { return const_cast<ThreadLocalBase *>(this)->get(); };

    /// Set up core data structures for TLS management
    static void static_initialization();

    /// Destruct core data structures for TLS management
    static void static_shutdown();

    /** \brief A new thread was started -- set up local TLS data structures.
        Returns \c true upon success */
    static bool register_thread();

    /// A thread has died -- destroy any remaining TLS entries associated with it
    static bool unregister_thread();

private:
    ConstructFunctor m_construct_functor;
    DestructFunctor m_destruct_functor;
};

/**
 * \brief Flexible platform-independent thread local storage class
 *
 * This class implements a generic thread local storage object. For details,
 * refer to its base class, \ref ThreadLocalBase.
 */
template <typename Type, typename SFINAE = void> class ThreadLocal : ThreadLocalBase {
public:
    using ThreadLocalBase::clear;

    /// Construct a new thread local storage object
    ThreadLocal() : ThreadLocalBase(
            []() -> void * { return new Type(); },
            [](void *data) { delete static_cast<Type *>(data); }
        ) { }

    /// Update the data associated with the current thread
    ThreadLocal& operator=(const Type &value) {
        operator Type &() = value;
        return *this;
    }

    /// Return a reference to the data associated with the current thread
    operator Type &() {
        return *((Type *) ThreadLocalBase::get());
    }

    /**
     * \brief Return a reference to the data associated with the
     * current thread (const version)
     */
    operator const Type &() const {
        return *((const Type *) ThreadLocalBase::get());
    }
};

/**
 * \brief Flexible platform-independent thread local storage class
 *
 * This class implements a generic thread local storage object. For details,
 * refer to its base class, \ref ThreadLocalBase.
 *
 * This is a partial template specialization to subclasses of \ref Object.
 */
template <typename Type> class ThreadLocal<
        Type, typename std::enable_if<std::is_base_of_v<Object, Type>>::type>
    : ThreadLocal<ref<Type>> {
public:
    /// Update the data associated with the current thread
    ThreadLocal& operator=(Type *value) {
        ThreadLocal<ref<Type>>::operator ref<Type> &() = value;
        return *this;
    }

    /// Return a reference to the data associated with the current thread
    operator Type *() {
        return ThreadLocal<ref<Type>>::operator ref<Type> &();
    }

    /**
     * \brief Return a reference to the data associated with the
     * current thread (const version)
     */
    operator const Type &() const {
        return ThreadLocal<ref<Type>>::operator ref<Type> &();
    }
};

NAMESPACE_END(mitsuba)
