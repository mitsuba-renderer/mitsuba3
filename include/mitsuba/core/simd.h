#pragma once

#include <mitsuba/mitsuba.h>
#include <enoki/array.h>

NAMESPACE_BEGIN(mitsuba)

/// Declare ssize_t, which does not exist on some platforms
using ssize_t = std::make_signed_t<size_t>;

using namespace enoki;

// =============================================================
//! @{ \name Static packet data types for vectorization
// =============================================================

constexpr size_t PacketSize = Array<float>::Size;

using FloatP        = Array<Float,        PacketSize>;
using Float16P      = Array<enoki::half,  PacketSize>;
using Float32P      = Array<float,        PacketSize>;
using Float64P      = Array<double,       PacketSize>;

using Int32P        = Array<int32_t,      PacketSize>;
using UInt32P       = Array<uint32_t,     PacketSize>;
using Int64P        = Array<int64_t,      PacketSize>;
using UInt64P       = Array<uint64_t,     PacketSize>;

using SizeP         = Array<size_t,       PacketSize>;
using SSizeP        = Array<ssize_t,      PacketSize>;

//! @}
// =============================================================

// =============================================================
//! @{ \name Dynamic packet data types for vectorization
// =============================================================

using FloatX   = DynamicArray<FloatP>;
using Float16X = DynamicArray<Float16P>;
using Float32X = DynamicArray<Float32P>;
using Float64X = DynamicArray<Float64P>;
using Int32X   = DynamicArray<Int32P>;
using UInt32X  = DynamicArray<UInt32P>;
using Int64X   = DynamicArray<Int64P>;
using UInt64X  = DynamicArray<UInt64P>;

//! @}
// =============================================================

// =============================================================
//! @{ \name Packet types for vectors, points, rays, etc.
// =============================================================

using Vector2fP = Vector<FloatP, 3>;
using Vector2fX = Vector<FloatX, 3>;

using Vector4fP = Vector<FloatP, 4>;
using Vector4fX = Vector<FloatX, 4>;

using Vector3fP = Vector<FloatP, 3>;
using Vector3fX = Vector<FloatX, 3>;

using Point2fP  = Point<FloatP, 2>;
using Point2fX  = Point<FloatX, 2>;

using Point3fP  = Point<FloatP, 3>;
using Point3fX  = Point<FloatX, 3>;

using Normal3fP = Normal<FloatP>;
using Normal3fX = Normal<FloatX>;

using Ray3fP    = Ray<Point3fP>;
using Ray3fX    = Ray<Point3fX>;

//! @}
// =============================================================

/// Convenience function which computes an array size/type suffix (like '2u' or '3fP')
template <typename T> std::string type_suffix() {
    using B = scalar_t<T>;

    std::string id = std::to_string(array_size<T>::value);

    if (std::is_floating_point<B>::value) {
        if (std::is_same<B, double>::value)
            id += 'd';
        else
            id += 'f';
    } else {
        if (std::is_signed<B>::value)
            id += 'i';
        else
            id += 'u';
    }

    if (is_sarray<value_t<T>>::value)
        id += 'P';
    else if (is_sarray<value_t<T>>::value)
        id += 'X';

    return id;
}


NAMESPACE_END(mitsuba)
