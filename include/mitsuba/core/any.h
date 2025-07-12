#pragma once

#include <mitsuba/core/fwd.h>
#include <typeinfo>
#include <cstring>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Type-erased storage for arbitrary objects
 *
 * This class resembles <tt>std::any</tt> but supports advanced customization by
 * exposing the underlying type-erased storage implementation \ref Any::Base.
 * The Mitsuba \ref Property class uses \ref Any when it needs to store things
 * that aren't part of the supported set of property types, such as Dr.Jit
 * tensor objects or other specialized types. The main reason for using \ref Any
 * (as opposed to the builtin <tt>std::any</tt>) is to enable seamless use in
 * Python bindings, which requires access to \ref Any::Base (see
 * <tt>src/core/python/properties.cpp</tt> for what this entails).
 *
 * Instances of this type are copyable with reference semantics. The class is
 * not thread-safe (i.e., it may not be copied concurrently).
 */
class Any {
public:
    // Type-erased storage backing the Any class
    struct Base {
        mutable size_t ref_count = 1;
        virtual ~Base() = default;
        virtual const std::type_info &type() const = 0;
        virtual void *ptr() = 0;

        void inc_ref() const { ++ref_count; }
        bool dec_ref() const { return --ref_count == 0; }
    };

    Any() = default;
    Any(const Any &a) : p(a.p) { if (p) p->inc_ref(); }
    Any(Any &&a) noexcept : p(a.p) { a.p = nullptr; }
    template <typename T>
    explicit Any(T &&t) : p(new Storage<std::decay_t<T>>(std::forward<T>(t))) { }
    ~Any() { release(); }

    Any &operator=(const Any &a) {
        if (this != &a) { release(); p = a.p; if (p) p->inc_ref(); }
        return *this;
    }

    Any &operator=(Any &&a) noexcept {
        if (this != &a) { release(); p = a.p; a.p = nullptr; }
        return *this;
    }

    void *data() { return p ? p->ptr() : nullptr; }
    const void *data() const { return p ? p->ptr() : nullptr; }
    const std::type_info &type() const { return p ? p->type() : typeid(void); }

    template <typename T> friend T *any_cast(Any *);
    template <typename T> friend const T *any_cast(const Any *);
    bool operator==(const Any &other) const { return data() == other.data(); }
    bool operator!=(const Any &other) const { return data() != other.data(); }

    // Constructor that accepts a Base pointer. For relatively advanced use cases that repurpose the Any class
    Any(Base *base) : p(base) { }

protected:
    template <typename T> struct Storage : Base {
        T value;
        template <typename U> Storage(U &&u) noexcept : value(std::forward<U>(u)) {}
        const std::type_info &type() const override { return typeid(T); }
        void *ptr() override { return &value; }
    };

    void release() {
        if (p && p->dec_ref()) {
            delete p;
            p = nullptr;
        }
    }

    Base *p = nullptr;
};

namespace detail {
    inline bool compare_typeid(const std::type_info &a, const std::type_info &b) {
        return &a == &b || std::strcmp(a.name(), b.name()) == 0;
    }
};

template <typename T> T *any_cast(Any *a) {
    return (a && a->p && detail::compare_typeid(a->p->type(), typeid(T))) ?
           static_cast<T *>(a->p->ptr()) : nullptr;
}

template <typename T> const T *any_cast(const Any *a) {
    return (a && a->p && detail::compare_typeid(a->p->type(), typeid(T))) ?
           static_cast<const T *>(a->p->ptr()) : nullptr;
}

NAMESPACE_END(mitsuba)
