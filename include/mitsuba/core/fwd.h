#pragma once

#include <mitsuba/core/platform.h>
#include <enoki/fwd.h>
#include <cstddef>
#include <cstdint>

NAMESPACE_BEGIN(mitsuba)

/// Import the complete Enoki namespace
using namespace enoki;

// =============================================================
//! @{ \name Forward declarations of Mitsuba classes/structs
// =============================================================

class Object;
class Class;
template <typename> class ref;

class AnnotatedStream;
class AnimatedTransform;
class Appender;
class ArgParser;
class Bitmap;
class DefaultFormatter;
class DummyStream;
class FileResolver;
class FileStream;
class Formatter;
class Logger;
class MemoryStream;
class Mutex;
class PluginManager;
class Properties;
class ScopedThreadEnvironment;
class Stream;
class StreamAppender;
class Struct;
class StructConverter;
class Thread;
class ThreadLocalBase;
class ZStream;
enum ELogLevel : int;

template <typename, typename> class ThreadLocal;
template <typename> class AtomicFloat;

//! @}
// =============================================================

// =============================================================
//! @{ \name Static & dynamic data types for vectorization
// =============================================================

constexpr size_t PacketSize = enoki::max_packet_size / sizeof(float);

using FloatP   = Packet<Float, PacketSize>;
using FloatX   = DynamicArray<FloatP>;
using MaskP    = PacketMask<Float, PacketSize>;

using Float16P = Packet<enoki::half, PacketSize>;
using Float16X = DynamicArray<Float16P>;

using Float32P = Packet<float, PacketSize>;
using Float32X = DynamicArray<Float32P>;

using Float64P = Packet<double, PacketSize>;
using Float64X = DynamicArray<Float64P>;

using Int32P   = Packet<int32_t, PacketSize>;
using Int32X   = DynamicArray<Int32P>;

using UInt32P  = Packet<uint32_t, PacketSize>;
using UInt32X  = DynamicArray<UInt32P>;

using Int64P   = Packet<int64_t, PacketSize>;
using Int64X   = DynamicArray<Int64P>;

using UInt64P  = Packet<uint64_t, PacketSize>;
using UInt64X  = DynamicArray<UInt64P>;

using SizeP    = Packet<size_t, PacketSize>;
using SizeX    = DynamicArray<SizeP>;

using SSizeP   = Packet<ssize_t, PacketSize>;
using SSizeX   = DynamicArray<SSizeP>;

using BoolP    = Packet<bool, PacketSize>;
using BoolX    = DynamicArray<BoolP>;

//! @}
// =============================================================

// =============================================================
//! @{ \name Common type aliases
// =============================================================

template <typename Value, size_t Size>    struct Vector;
template <typename Value, size_t Size>    struct Point;
template <typename Value, size_t Size>    struct Normal;
template <typename Point>                 struct Ray;
template <typename Point>                 struct RayDifferential;
template <typename Point>                 struct BoundingBox;
template <typename Point>                 struct BoundingSphere;
template <typename Vector>                struct Frame;
template <typename Value, size_t Size>    struct Color;
template <typename Value>                 struct Spectrum;
template <typename VectorN>               struct Transform;

using Vector1f = Vector<Float, 1>;
using Vector2f = Vector<Float, 2>;
using Vector3f = Vector<Float, 3>;
using Vector4f = Vector<Float, 4>;

using Vector2fP = Vector<FloatP, 2>;
using Vector2fX = Vector<FloatX, 2>;
using Vector3fP = Vector<FloatP, 3>;
using Vector3fX = Vector<FloatX, 3>;
using Vector4fP = Vector<FloatP, 4>;
using Vector4fX = Vector<FloatX, 4>;

using Vector1d = Vector<double, 1>;
using Vector2d = Vector<double, 2>;
using Vector3d = Vector<double, 3>;
using Vector4d = Vector<double, 4>;

using Vector2dP = Vector<Float64P, 2>;
using Vector2dX = Vector<Float64X, 2>;
using Vector3dP = Vector<Float64P, 3>;
using Vector3dX = Vector<Float64X, 3>;
using Vector4dP = Vector<Float64P, 4>;
using Vector4dX = Vector<Float64X, 4>;

using Vector1h = Vector<enoki::half, 1>;
using Vector2h = Vector<enoki::half, 2>;
using Vector3h = Vector<enoki::half, 3>;
using Vector4h = Vector<enoki::half, 4>;

using Vector2hP = Vector<Float16P, 2>;
using Vector2hX = Vector<Float16X, 2>;
using Vector3hP = Vector<Float16P, 3>;
using Vector3hX = Vector<Float16X, 3>;
using Vector4hP = Vector<Float16P, 4>;
using Vector4hX = Vector<Float16X, 4>;

using Vector1i = Vector<int32_t, 1>;
using Vector2i = Vector<int32_t, 2>;
using Vector3i = Vector<int32_t, 3>;
using Vector4i = Vector<int32_t, 4>;

using Vector1u = Vector<uint32_t, 1>;
using Vector2u = Vector<uint32_t, 2>;
using Vector3u = Vector<uint32_t, 3>;
using Vector4u = Vector<uint32_t, 4>;

using Vector2iP = Vector<Int32P, 2>;
using Vector2iX = Vector<Int32X, 2>;
using Vector3iP = Vector<Int32P, 3>;
using Vector3iX = Vector<Int32X, 3>;

using Vector1s = Vector<size_t, 1>;
using Vector2s = Vector<size_t, 2>;
using Vector3s = Vector<size_t, 3>;
using Vector4s = Vector<size_t, 4>;

using Point1f = Point<Float, 1>;
using Point2f = Point<Float, 2>;
using Point3f = Point<Float, 3>;
using Point4f = Point<Float, 4>;

using Point2fP = Point<FloatP, 2>;
using Point2fX = Point<FloatX, 2>;
using Point3fP = Point<FloatP, 3>;
using Point3fX = Point<FloatX, 3>;

using Point1d = Point<double, 1>;
using Point2d = Point<double, 2>;
using Point3d = Point<double, 3>;
using Point4d = Point<double, 4>;

using Point1i = Point<int32_t, 1>;
using Point2i = Point<int32_t, 2>;
using Point3i = Point<int32_t, 3>;
using Point4i = Point<int32_t, 4>;

using Point1u = Point<uint32_t, 1>;
using Point2u = Point<uint32_t, 2>;
using Point3u = Point<uint32_t, 3>;
using Point4u = Point<uint32_t, 4>;

using Point2iP = Point<Int32P, 2>;
using Point2iX = Point<Int32X, 2>;
using Point3iP = Point<Int32P, 3>;
using Point3iX = Point<Int32X, 3>;

using Point1s = Point<size_t, 1>;
using Point2s = Point<size_t, 2>;
using Point3s = Point<size_t, 3>;
using Point4s = Point<size_t, 4>;

using Normal3f = Normal<Float, 3>;
using Normal3h = Normal<enoki::half, 3>;
using Normal3d = Normal<double, 3>;

using Normal3fP = Normal<FloatP, 3>;
using Normal3hP = Normal<Float16P, 3>;
using Normal3fX = Normal<FloatX, 3>;

using Frame3f  = Frame<Vector3f>;
using Frame3fP = Frame<Vector3fP>;
using Frame3fX = Frame<Vector3fX>;

using BoundingBox1f = BoundingBox<Point1f>;
using BoundingBox2f = BoundingBox<Point2f>;
using BoundingBox3f = BoundingBox<Point3f>;
using BoundingBox4f = BoundingBox<Point4f>;

using BoundingSphere3f = BoundingSphere<Point3f>;

using Ray2f = Ray<Point2f>;
using Ray3f = Ray<Point3f>;
using Ray3fP = Ray<Point3fP>;
using Ray3fX = Ray<Point3fX>;

using RayDifferential3f  = RayDifferential<Point3f>;
using RayDifferential3fP = RayDifferential<Point3fP>;
using RayDifferential3fX = RayDifferential<Point3fX>;

using Transform3f  = Transform<Vector3f>;
using Transform3fP = Transform<Vector3fP>;
using Transform3fX = Transform<Vector3fX>;

using Transform4f  = Transform<Vector4f>;
using Transform4fP = Transform<Vector4fP>;
using Transform4fX = Transform<Vector4fX>;

using Color3f  = Color<Float,  3>;
using Color3fP = Color<FloatP, 3>;
using Color3fX = Color<FloatX, 3>;

/**
 * The following types are used for computations involving data that
 * is sampled at a fixed number of points in the wavelength domain.
 *
 * Note that this is not restricted to radiance data -- probabilities or
 * sampling weights often need to be expressed in a spectrally varying manner,
 * and this type also applies to these situations.
 */
using Spectrumf  = Spectrum<Float>;
using SpectrumfP = Spectrum<FloatP>;
using SpectrumfX = Spectrum<FloatX>;

//! @}
// =============================================================

NAMESPACE_BEGIN(filesystem)
class path;
NAMESPACE_END(filesystem)

namespace fs = filesystem;

NAMESPACE_END(mitsuba)
