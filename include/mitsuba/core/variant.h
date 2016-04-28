#pragma once

#include <mitsuba/core/platform.h>
#include <typeinfo>
#include <type_traits>

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

template<typename T, typename... Args> struct variant_helper<T, Args...> {
    static bool copy(const std::type_info *type_info, const void *source, void *target)
        noexcept(std::is_nothrow_copy_constructible<T>::value() &&
            noexcept(variant_helper<Args...>::copy(type_info, source, target))) {
        if (type_info == &typeid(T)) {
            new (target) T(*reinterpret_cast<const T *>(source));
            return true;
        } else {
            return variant_helper<Args...>::copy(type_info, source, target);
        }
    }

    static bool move(const std::type_info *type_info, void *source, void *target)
        noexcept(std::is_nothrow_move_constructible<T>::value() &&
            noexcept(variant_helper<Args...>::move(type_info, source, target))) {
        if (type_info == &typeid(T)) {
            new (target) T(std::move(*reinterpret_cast<T *>(source)));
            return true;
        } else {
            return variant_helper<Args...>::move(type_info, source, target);
        }
    }

    static void destruct(const std::type_info *type_info, void *ptr)
        noexcept(std::is_nothrow_destructible<T>::value() &&
            noexcept(variant_helper<Args...>::destruct(type_info, ptr))) {
        if (type_info == &typeid(T))
            reinterpret_cast<T*>(ptr)->~T();
        else
            variant_helper<Args...>::destruct(type_info, ptr);
    }
};

template<> struct variant_helper<>  {
    static bool copy(const std::type_info *, const void *, void *) noexcept { return false; }
    static bool move(const std::type_info *, void *, void *) noexcept { return false; }
    static void destruct(const std::type_info *, void *) noexcept { }
};

NAMESPACE_END(detail)

template <typename... Args> struct variant {
private:
    static const size_t data_size = detail::static_max<sizeof(Args)...>::value;
    static const size_t data_align = detail::static_max<alignof(Args)...>::value;

    using storage_type = typename std::aligned_storage<data_size, data_align>::type;
    using helper_type = detail::variant_helper<Args...>;

public:
    variant() { }

    variant(const variant<Args...> &v)
        noexcept(helper_type::copy(type_info, &v.data, &data))
        : type_info(v.type_info) {
        helper_type::copy(type_info, &v.data, &data);
    }

    variant(variant<Args...>&& v)
        noexcept(helper_type::move(type_info, &v.data, &data))
        : type_info(v.type_info) {
        helper_type::move(type_info, &v.data, &data);
        helper_type::destruct(type_info, &v.data);
        v.type_info = nullptr;
    }

    ~variant() noexcept(helper_type::destruct(type_info, &data)) {
        helper_type::destruct(type_info, &data);
    }

    variant<Args...>& operator=(variant<Args...> &v)
        noexcept(operator=((const variant<Args...> &) v)) {
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
        if (std::is_rvalue_reference<T&&>::value)
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

private:
    storage_type data;
    const std::type_info *type_info = nullptr;
};

NAMESPACE_END(mitsuba)
