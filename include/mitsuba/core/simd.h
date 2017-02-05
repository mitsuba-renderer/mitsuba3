#pragma once

#include <mitsuba/mitsuba.h>
#include <enoki/array.h>

NAMESPACE_BEGIN(mitsuba)

using namespace enoki;

// =============================================================
//! @{ \name Static packet data types for vectorization
// =============================================================

constexpr size_t PacketSize = Array<float>::Size;

using FloatP   = Array<Float,        PacketSize>;
using Float16P = Array<enoki::half,  PacketSize>;
using Float32P = Array<float,        PacketSize>;
using Float64P = Array<double,       PacketSize>;
using Int32P   = Array<int32_t,      PacketSize>;
using UInt32P  = Array<uint32_t,     PacketSize>;
using Int64P   = Array<int64_t,      PacketSize>;
using UInt64P  = Array<uint64_t,     PacketSize>;

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

using Vector2fP = TVector<FloatP, 3>;
using Vector2fX = TVector<FloatX, 3>;

using Vector4fP = TVector<FloatP, 4>;
using Vector4fX = TVector<FloatX, 4>;

using Vector3fP = TVector<FloatP, 3>;
using Vector3fX = TVector<FloatX, 3>;

using Point2fP  = TPoint<FloatP, 2>;
using Point2fX  = TPoint<FloatX, 2>;

using Point3fP  = TPoint<FloatP, 3>;
using Point3fX  = TPoint<FloatX, 3>;

using Normal3fP = TNormal<FloatP>;
using Normal3fX = TNormal<FloatX>;

using Ray3fP    = TRay<Point3fP, Vector3fP>;
using Ray3fX    = TRay<Point3fX, Vector3fX>;

//! @}
// =============================================================

NAMESPACE_END(mitsuba)
