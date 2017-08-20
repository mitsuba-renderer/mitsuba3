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

class AnnotatedStraem;
class Appender;
class ArgParser;
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
class Stream;
class StreamAppender;
class Struct;
class StructConverter;
class Transform;
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

template <typename Type, size_t Size>  struct Vector;
template <typename Type, size_t Size>  struct Point;
template <typename Type, size_t Size>  struct Normal;
template <typename Point>              struct Ray;
template <typename Point>              struct RayDifferential;
template <typename Point>              struct BoundingBox;
template <typename Point>              struct BoundingSphere;
template <typename Vector>             struct Frame;
template <typename Type, size_t Size>  struct Color;
template <typename Type>               struct Spectrum;

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

using Vector1i = Vector<int32_t, 1>;
using Vector2i = Vector<int32_t, 2>;
using Vector3i = Vector<int32_t, 3>;
using Vector4i = Vector<int32_t, 4>;

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

using Normal3f = Normal<Float, 3>;
using Normal3d = Normal<double, 3>;

using Normal3fP = Normal<FloatP, 3>;
using Normal3fX = Normal<FloatX, 3>;

using Frame3f  = Frame<Vector3f>;
using Frame3fP = Frame<Point3fP>;
using Frame3fX = Frame<Point3fX>;

using BoundingBox1f = BoundingBox<Point1f>;
using BoundingBox2f = BoundingBox<Point2f>;
using BoundingBox3f = BoundingBox<Point3f>;
using BoundingBox4f = BoundingBox<Point4f>;

using BoundingSphere3f = BoundingSphere<Point3f>;

using Ray2f  = Ray<Point2f>;
using Ray3f  = Ray<Point3f>;
using Ray3fP = Ray<Point3fP>;
using Ray3fX = Ray<Point3fX>;

using RayDifferential3f  = RayDifferential<Point3f>;
using RayDifferential3fP = RayDifferential<Point3fP>;
using RayDifferential3fX = RayDifferential<Point3fX>;

//! @}
// =============================================================

NAMESPACE_BEGIN(filesystem)
class path;
NAMESPACE_END(filesystem)

namespace fs = filesystem;

NAMESPACE_END(mitsuba)
