#pragma once

#include <mitsuba/mitsuba.h>
#include <string>
#include <tuple>

#include <drjit/array_router.h>

NAMESPACE_BEGIN(mitsuba)

/// Convenience function which computes an array size/type suffix (like '2u' or '3fP')
template <typename T> std::string type_suffix() {
    using S = dr::scalar_t<T>;
    using V = dr::value_t<T>;

    std::string id = std::to_string(dr::array_size_v<T>);

    if constexpr (dr::is_floating_point_v<S>) {
        if constexpr (std::is_same_v<S, dr::half>)
            id += 'h';
        else if constexpr (std::is_same_v<S, float>)
            id += 'f';
        else
            id += 'd';
    } else {
        if constexpr (dr::is_signed_v<S>)
            id += 'i';
        else
            id += 'u';
    }

    if constexpr (dr::is_diff_v<V>)
        id += 'D';

    if constexpr (dr::is_packed_array_v<V>)
        id += 'P';
    else if constexpr (dr::is_llvm_v<V>)
        id += 'L';
    else if constexpr (dr::is_cuda_v<V>)
        id += 'C';
    else if constexpr (dr::is_dynamic_array_v<V>)
        id += 'X';

    return id;
}

NAMESPACE_END(mitsuba)
