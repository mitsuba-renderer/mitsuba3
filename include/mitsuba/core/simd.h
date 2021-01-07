#pragma once

#include <mitsuba/mitsuba.h>
#include <string>
#include <tuple>

#include <enoki/array_router.h>

NAMESPACE_BEGIN(mitsuba)

/// Convenience function which computes an array size/type suffix (like '2u' or '3fP')
template <typename T> std::string type_suffix() {
    using S = ek::scalar_t<T>;
    using V = ek::value_t<T>;

    std::string id = std::to_string(ek::array_size_v<T>);

    if constexpr (ek::is_floating_point_v<S>) {
        if constexpr (std::is_same_v<S, ek::half>)
            id += 'h';
        else if constexpr (std::is_same_v<S, float>)
            id += 'f';
        else
            id += 'd';
    } else {
        if constexpr (ek::is_signed_v<S>)
            id += 'i';
        else
            id += 'u';
    }

    if constexpr (ek::is_diff_array_v<V>)
        id += 'D';

    if constexpr (ek::is_packed_array_v<V>)
        id += 'P';
    else if constexpr (ek::is_llvm_array_v<V>)
        id += 'L';
    else if constexpr (ek::is_cuda_array_v<V>)
        id += 'C';
    else if constexpr (ek::is_dynamic_array_v<V>)
        id += 'X';

    return id;
}

NAMESPACE_END(mitsuba)
