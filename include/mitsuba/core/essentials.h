#pragma once

#include <mitsuba/core/platform.h>
#include <cstdint>
#include <cstring>
#include <utility>

NAMESPACE_BEGIN(mitsuba)

/** \brief The following is used to ensure that the getters and setters
 * for all the same types are available for both \ref Stream implementations
 * and \AnnotatedStream. */

template <typename... Args> struct for_each_type;

template <typename T, typename... Args>
struct for_each_type<T, Args...> {
    template <typename UserFunctionType, typename ...Params>
    static void recurse(Params&&... params) {
        UserFunctionType::template apply<T>(std::forward<Params>(params)...);
        for_each_type<Args...>::template recurse<UserFunctionType>(std::forward<Params>(params)...);
    }
};

/// Base case
template <>
struct for_each_type<> {
    template <typename UserFunctionType, typename... Params>
    static void recurse(Params&&...) { }
};

/// Replacement for std::is_constructible which also works when the destructor is not accessible
template <typename T, typename... Args> struct is_constructible {
private:
    static std::false_type test(...);
    template <typename T2 = T>
    static std::true_type test(decltype(new T2(std::declval<Args>()...)) value);
public:
    static constexpr bool value = decltype(test(std::declval<T *>()))::value;
};

template <typename T, typename... Args>
constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

NAMESPACE_END(mitsuba)
