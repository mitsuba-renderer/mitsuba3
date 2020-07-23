#pragma once

#include <mitsuba/mitsuba.h>
#include <enoki/array_traits.h>
#include <string>

#if defined(MTS_ENABLE_OPTIX)
#  include <enoki/cuda.h>
#  include <enoki/autodiff.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/// Convenience function which computes an array size/type suffix (like '2u' or '3fP')
template <typename T> std::string type_suffix() {
    using S = ek::scalar_t<T>;
    using V = ek::value_t<T>;

    std::string id = std::to_string(ek::array_size_v<T>);

    if constexpr (ek::is_floating_point_v<S>) {
        if (std::is_same_v<S, ek::half>)
            id += 'h';
        else if (std::is_same_v<S, float>)
            id += 'f';
        else
            id += 'd';
    } else {
        if (ek::is_signed_v<S>)
            id += 'i';
        else
            id += 'u';
    }

    if (ek::is_diff_array_v<V>)
        id += 'D';

    if (ek::is_packed_array_v<V>)
        id += 'P';
    else if (ek::is_llvm_array_v<V>)
        id += 'L';
    else if (ek::is_cuda_array_v<V>)
        id += 'C';
    else if (ek::is_dynamic_array_v<V>)
        id += 'X';

    return id;
}

/// Round an integer to a multiple of the current packet size
template <typename Float>
inline size_t round_to_packet_size(size_t size) {
    constexpr size_t PacketSize = Float::Size;
    return (size + PacketSize - 1) / PacketSize * PacketSize;
}

NAMESPACE_END(mitsuba)
