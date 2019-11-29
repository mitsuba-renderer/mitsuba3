#pragma once

#include <mitsuba/mitsuba.h>
#include <enoki/array.h>
#include <enoki/dynamic.h>

#if defined(MTS_ENABLE_OPTIX)
#  include <enoki/cuda.h>
#  include <enoki/autodiff.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/// Convenience function which computes an array size/type suffix (like '2u' or '3fP')
template <typename T> std::string type_suffix() {
    using B = scalar_t<T>;

    std::string id = std::to_string(array_size_v<T>);

    if (std::is_floating_point_v<B>) {
        if (std::is_same_v<B, enoki::half>) {
            id += 'h';
        } else {
            if constexpr (is_float_v<B>)
                id += std::is_same_v<B, float> ? 'f' : 'd';
            else
                id += std::is_same_v<B, double> ? 'f' : 's';
        }
    } else {
        if (std::is_signed_v<B>)
            id += 'i';
        else
            id += 'u';
    }

    if (is_static_array_v<value_t<T>>)
        id += 'P';
    else if (is_diff_array_v<value_t<T>>)
        id += 'D';
    else if (is_cuda_array_v<value_t<T>>)
        id += 'C';
    else if (is_dynamic_array_v<value_t<T>>)
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
