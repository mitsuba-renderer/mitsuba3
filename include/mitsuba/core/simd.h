#pragma once

#include <mitsuba/mitsuba.h>
#include <enoki/array.h>

NAMESPACE_BEGIN(mitsuba)

/// Convenience function which computes an array size/type suffix (like '2u' or '3fP')
template <typename T> std::string type_suffix() {
    using B = scalar_t<T>;

    std::string id = std::to_string(array_size<T>::value);

    if (std::is_floating_point<B>::value) {
        if (std::is_same<B, enoki::half>::value) {
            id += 'h';
        } else {
            #if defined(SINGLE_PRECISION)
                id += std::is_same<B, float>::value ? 'f' : 'd';
            #else
                id += std::is_same<B, double>::value ? 'f' : 's';
            #endif
        }
    } else {
        if (std::is_signed<B>::value)
            id += 'i';
        else
            id += 'u';
    }

    if (is_static_array<value_t<T>>::value)
        id += 'P';
    else if (is_dynamic_array<value_t<T>>::value)
        id += 'X';

    return id;
}


NAMESPACE_END(mitsuba)
