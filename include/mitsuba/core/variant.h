#pragma once

#include <cstddef> // for size_t
#include <type_traits>
#include <typeinfo>
#include <utility> // for std::move

#include <mitsuba/core/platform.h>

#if defined(_MSC_VER)
  #pragma warning(push)
  #pragma warning(disable : 4522) /* warning C4522: multiple assignment operators specified */
  #define noexcept(arg)
#endif

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Basic C++11 variant data structure
 *
 * Significantly redesigned version of the approach described at
 * http://www.ojdip.net/2013/10/implementing-a-variant-type-in-cpp
 */

NAMESPACE_BEGIN(detail)

template <size_t Arg1, size_t... Args> struct static_max;

template <size_t Arg> struct static_max<Arg> {
    static const size_t value = Arg;
};

template <size_t Arg1, size_t Arg2, size_t... Args> struct static_max<Arg1, Arg2, Args...> {
    static const size_t value = Arg1 >= Arg2 ? static_max<Arg1, Args...>::value:
    static_max<Arg2, Args...>::value;
};

template<typename... Args> struct variant_helper;

template <typename T, typename... Args> struct variant_helper<T, Args...> {
    static bool copy(const std::type_info *type_info, const void *source, void *target)
        noexcept(std::is_nothrow_copy_constructible_v<T> &&
            noexcept(variant_helper<Args...>::copy(type_info, source, target))) {
        if (type_info == &typeid(T)) {
            new (target) T(*reinterpret_cast<const T *>(source));
            return true;
        } else {
            return variant_helper<Args...>::copy(type_info, source, target);
        }
    }

    static bool move(const std::type_info *type_info, void *source, void *target)
        noexcept(std::is_nothrow_move_constructible_v<T> &&
            noexcept(variant_helper<Args...>::move(type_info, source, target))) {
        if (type_info == &typeid(T)) {
            new (target) T(std::move(*reinterpret_cast<T *>(source)));
            return true;
        } else {
            return variant_helper<Args...>::move(type_info, source, target);
        }
    }

    static void destruct(const std::type_info *type_info, void *ptr)
        noexcept(std::is_nothrow_destructible_v<T> &&
            noexcept(variant_helper<Args...>::destruct(type_info, ptr))) {
        if (type_info == &typeid(T))
            reinterpret_cast<T*>(ptr)->~T();
        else
            variant_helper<Args...>::destruct(type_info, ptr);
    }

    static bool equals(const std::type_info *type_info, const void *v1, const void *v2) {
        if (type_info == &typeid(T))
            return (*reinterpret_cast<const T *>(v1)) == (*reinterpret_cast<const T *>(v2));
        else
            return variant_helper<Args...>::equals(type_info, v1, v2);
    }

    template <typename Visitor>
    static auto visit(const std::type_info *type_info, void *ptr, Visitor &v)
        -> decltype(v(std::declval<T>())) {
        if (type_info == &typeid(T))
            return v(*reinterpret_cast<T*>(ptr));
        else
            return variant_helper<Args...>::visit(type_info, ptr, v);
    }
};

template <> struct variant_helper<>  {
    static bool copy(const std::type_info *, const void *, void *) noexcept(true) { return false; }
    static bool move(const std::type_info *, void *, void *) noexcept(true) { return false; }
    static void destruct(const std::type_info *, void *) noexcept(true) { }
    static bool equals(const std::type_info *, const void *, const void *) { return false; }
    template <typename Visitor>
    static auto visit(const std::type_info *, void *, Visitor& v) -> decltype(v(nullptr)) {
        return v(nullptr);
    }
};

NAMESPACE_END(detail)

template <typename... Args> struct variant {
private:
    static const size_t data_size = detail::static_max<sizeof(Args)...>::value;
    static const size_t data_align = detail::static_max<alignof(Args)...>::value;

    using storage_type = typename std::aligned_storage<data_size, data_align>::type;
    using helper_type = detail::variant_helper<Args...>;

private:
    storage_type data;
    const std::type_info *type_info = nullptr;

public:
    variant() { }

    variant(const variant<Args...> &v)
        noexcept(noexcept(helper_type::copy(type_info, &v.data, &data)))
        : type_info(v.type_info) {
        helper_type::copy(type_info, &v.data, &data);
    }

    variant(variant<Args...>&& v)
        noexcept(noexcept(helper_type::move(type_info, &v.data, &data)))
        : type_info(v.type_info) {
        helper_type::move(type_info, &v.data, &data);
        helper_type::destruct(type_info, &v.data);
        v.type_info = nullptr;
    }

    ~variant() noexcept(noexcept(helper_type::destruct(type_info, &data))) {
        helper_type::destruct(type_info, &data);
    }

    variant<Args...>& operator=(variant<Args...> &v)
        noexcept(noexcept(operator=((const variant<Args...> &) v))) {
        return operator=((const variant<Args...> &) v);
    }

    variant<Args...>& operator=(const variant<Args...> &v)
        noexcept(
            noexcept(helper_type::copy(type_info, &v.data, &data)) &&
            noexcept(helper_type::destruct(type_info, &data))
        ) {
        helper_type::destruct(type_info, &data);
        type_info = v.type_info;
        helper_type::copy(type_info, &v.data, &data);
        return *this;
    }

    variant<Args...>& operator=(variant<Args...> &&v)
        noexcept(
            noexcept(helper_type::move(type_info, &v.data, &data)) &&
            noexcept(helper_type::destruct(type_info, &data))
        ) {
        helper_type::destruct(type_info, &data);
        type_info = v.type_info;
        helper_type::move(type_info, &v.data, &data);
        helper_type::destruct(type_info, &v.data);
        v.type_info = nullptr;
        return *this;
    }

    template <typename T> variant<Args...>& operator=(T &&value)
        noexcept(
            noexcept(helper_type::copy(type_info, &value, &data)) &&
            noexcept(helper_type::move(type_info, &value, &data)) &&
            noexcept(helper_type::destruct(type_info, &data))
        ) {
        helper_type::destruct(type_info, &data);
        type_info = &typeid(T);
        bool success;
        if (std::is_rvalue_reference_v<T&&>)
            success = helper_type::move(type_info, &value, &data);
        else
            success = helper_type::copy(type_info, &value, &data);
        if (!success)
            throw std::bad_cast();
        return *this;
    }

    const std::type_info &type() const {
        return *type_info;
    }

    template <typename T> bool is() const {
        return type_info == &typeid(T);
    }

    bool empty() const { return type_info == nullptr; }

    template <typename Visitor>
    auto visit(Visitor &&v) -> decltype(helper_type::visit(type_info, &data, v)) {
        return helper_type::visit(type_info, &data, v);
    }

#if defined(__GNUG__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

    template <typename T> operator T&() {
        if (!is<T>())
            throw std::bad_cast();
        return *reinterpret_cast<T *>(&data);
    }

    template <typename T> operator const T&() const {
        if (!is<T>())
            throw std::bad_cast();
        return *reinterpret_cast<const T *>(&data);
    }

#if defined(__GNUG__)
#  pragma GCC diagnostic pop
#endif

    bool operator==(const variant<Args...> &other) const {
        if (type_info != other.type_info)
            return false;
        else
            return helper_type::equals(type_info, &data, &other.data);
    }

    bool operator!=(const variant<Args...> &other) const {
        return !operator==(other);
    }
};

NAMESPACE_END(mitsuba)

#if defined(_MSC_VER)
#  pragma warning(pop)
#  undef noexcept
#endif
