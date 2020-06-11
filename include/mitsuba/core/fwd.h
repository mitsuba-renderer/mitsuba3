#pragma once

#include <mitsuba/core/platform.h>
#include <enoki/fwd.h>
#include <enoki/array_traits.h>
#include <cstddef>
#include <cstdint>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/// Import the complete Enoki namespace
using namespace enoki;

// =============================================================
//! @{ \name Forward declarations of Mitsuba classes/structs
// =============================================================

class Object;
class Class;
template <typename> class ref;

class AnimatedTransform;
class AnnotatedStream;
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
class TraversalCallback;
class ZStream;
enum LogLevel : int;

template <typename, typename> class ThreadLocal;
template <typename> class AtomicFloat;

//! @}
// =============================================================

// =============================================================
//! @{ \name Common type aliases
// =============================================================

template <typename Value, size_t Size>          struct Vector;
template <typename Value, size_t Size>          struct Point;
template <typename Value, size_t Size>          struct Normal;
template <typename Value, size_t Size>          struct Color;
template <typename Value, size_t Size>          struct Spectrum;
template <typename Point>                       struct Transform;
template <typename Point, typename Spectrum>    struct Ray;
template <typename Point, typename Spectrum>    struct RayDifferential;
template <typename Point>                       struct BoundingBox;
template <typename Point>                       struct BoundingSphere;
template <typename Vector>                      struct Frame;
template <typename Float>                       struct DiscreteDistribution;
template <typename Float>                       struct ContinuousDistribution;

template <typename Spectrum> using StokesVector  = enoki::Array<Spectrum, 4>;
template <typename Spectrum> using MuellerMatrix = enoki::Matrix<Spectrum, 4>;

//! @}
// =============================================================

// =============================================================
//! @{ \name Buffer types
// =============================================================

template <typename Value,
          typename T = std::conditional_t<is_static_array_v<Value>, value_t<Value>, Value>>
using DynamicBuffer = std::conditional_t<
    is_dynamic_array_v<T>,
    T,
    DynamicArray<Packet<scalar_t<T>>>
>;

//! @}
// =============================================================

// =============================================================
//! @{ \name Platform agnostic vector types
// =============================================================

// TODO clean this
template <typename T, typename Type = T>
using host_vector =
    std::vector<scalar_t<T>,
                std::conditional_t<is_cuda_array_v<Type>, cuda_host_allocator<scalar_t<T>>,
                                   std::allocator<scalar_t<T>>>>;

template <typename T>
using managed_vector =
    std::vector<scalar_t<T>,
                std::conditional_t<is_cuda_array_v<T>, cuda_managed_allocator<scalar_t<T>>,
                                   std::allocator<scalar_t<T>>>>;

//! @}
// =============================================================

// =============================================================
//! @{ \name CoreAliases struct
// =============================================================

template <typename Float_> struct CoreAliases {
    using Float   = Float_;

    using Mask    = mask_t<Float>;

    using Int8    = replace_scalar_t<Float, int8_t>;
    using Int32   =   int32_array_t<Float>;
    using UInt32  =  uint32_array_t<Float>;
    using Int64   =   int64_array_t<Float>;
    using UInt64  =  uint64_array_t<Float>;
    using Float32 = float32_array_t<Float>;
    using Float64 = float64_array_t<Float>;

    using Vector1i = Vector<Int32, 1>;
    using Vector2i = Vector<Int32, 2>;
    using Vector3i = Vector<Int32, 3>;
    using Vector4i = Vector<Int32, 4>;

    using Vector1u = Vector<UInt32, 1>;
    using Vector2u = Vector<UInt32, 2>;
    using Vector3u = Vector<UInt32, 3>;
    using Vector4u = Vector<UInt32, 4>;

    using Vector1f = Vector<Float, 1>;
    using Vector2f = Vector<Float, 2>;
    using Vector3f = Vector<Float, 3>;
    using Vector4f = Vector<Float, 4>;

    using Vector1d = Vector<Float64, 1>;
    using Vector2d = Vector<Float64, 2>;
    using Vector3d = Vector<Float64, 3>;
    using Vector4d = Vector<Float64, 4>;

    using Point1i = Point<Int32, 1>;
    using Point2i = Point<Int32, 2>;
    using Point3i = Point<Int32, 3>;
    using Point4i = Point<Int32, 4>;

    using Point1u = Point<UInt32, 1>;
    using Point2u = Point<UInt32, 2>;
    using Point3u = Point<UInt32, 3>;
    using Point4u = Point<UInt32, 4>;

    using Point1f = Point<Float, 1>;
    using Point2f = Point<Float, 2>;
    using Point3f = Point<Float, 3>;
    using Point4f = Point<Float, 4>;

    using Point1d = Point<Float64, 1>;
    using Point2d = Point<Float64, 2>;
    using Point3d = Point<Float64, 3>;
    using Point4d = Point<Float64, 4>;

    using Normal3f = Normal<Float, 3>;
    using Normal3d = Normal<Float64, 3>;

    using Matrix2f = Matrix<Float, 2>;
    using Matrix2d = Matrix<Float64, 2>;
    using Matrix3f = Matrix<Float, 3>;
    using Matrix3d = Matrix<Float64, 3>;
    using Matrix4f = Matrix<Float, 4>;
    using Matrix4d = Matrix<Float64, 4>;

    using Quaternion4f = Quaternion<Float>;
    using Quaternion4d = Quaternion<Float64>;

    using BoundingBox1f    = BoundingBox<Point1f>;
    using BoundingBox2f    = BoundingBox<Point2f>;
    using BoundingBox3f    = BoundingBox<Point3f>;
    using BoundingBox4f    = BoundingBox<Point4f>;
    using BoundingSphere1f = BoundingSphere<Point1f>;
    using BoundingSphere2f = BoundingSphere<Point2f>;
    using BoundingSphere3f = BoundingSphere<Point3f>;
    using BoundingSphere4f = BoundingSphere<Point4f>;

    using Frame3f          = Frame<Float>;
    using Transform3f      = Transform<Point3f>;
    using Transform4f      = Transform<Point4f>;

    using Color1f          = Color<Float, 1>;
    using Color3f          = Color<Float, 3>;

    /*
     * The following aliases are only used for casting to python object with PY_CAST_VARIANTS.
     * They won't be exposed by the MTS_IMPORT_BASE_TYPES macro.
     */
    using Array1f        = Array<Float, 1>;
    using Array3f        = Array<Float, 3>;
    using DynamicBuffer  = mitsuba::DynamicBuffer<Float>;
};

//! @}
// =============================================================

#define MTS_VARIANT template <typename Float, typename Spectrum>

#define MTS_IMPORT_CORE_TYPES_PREFIX(Float_, prefix)                                               \
    using prefix ## CoreAliases          = mitsuba::CoreAliases<Float_>;                           \
    using prefix ## Mask                 = typename prefix ## CoreAliases::Mask;                   \
    using prefix ## Int8                 = typename prefix ## CoreAliases::Int8;                   \
    using prefix ## Int32                = typename prefix ## CoreAliases::Int32;                  \
    using prefix ## UInt32               = typename prefix ## CoreAliases::UInt32;                 \
    using prefix ## Int64                = typename prefix ## CoreAliases::Int64;                  \
    using prefix ## UInt64               = typename prefix ## CoreAliases::UInt64;                 \
    using prefix ## Float32              = typename prefix ## CoreAliases::Float32;                \
    using prefix ## Float64              = typename prefix ## CoreAliases::Float64;                \
    using prefix ## Vector1i             = typename prefix ## CoreAliases::Vector1i;               \
    using prefix ## Vector2i             = typename prefix ## CoreAliases::Vector2i;               \
    using prefix ## Vector3i             = typename prefix ## CoreAliases::Vector3i;               \
    using prefix ## Vector4i             = typename prefix ## CoreAliases::Vector4i;               \
    using prefix ## Vector1u             = typename prefix ## CoreAliases::Vector1u;               \
    using prefix ## Vector2u             = typename prefix ## CoreAliases::Vector2u;               \
    using prefix ## Vector3u             = typename prefix ## CoreAliases::Vector3u;               \
    using prefix ## Vector4u             = typename prefix ## CoreAliases::Vector4u;               \
    using prefix ## Vector1f             = typename prefix ## CoreAliases::Vector1f;               \
    using prefix ## Vector2f             = typename prefix ## CoreAliases::Vector2f;               \
    using prefix ## Vector3f             = typename prefix ## CoreAliases::Vector3f;               \
    using prefix ## Vector4f             = typename prefix ## CoreAliases::Vector4f;               \
    using prefix ## Vector1d             = typename prefix ## CoreAliases::Vector1d;               \
    using prefix ## Vector2d             = typename prefix ## CoreAliases::Vector2d;               \
    using prefix ## Vector3d             = typename prefix ## CoreAliases::Vector3d;               \
    using prefix ## Vector4d             = typename prefix ## CoreAliases::Vector4d;               \
    using prefix ## Point1i              = typename prefix ## CoreAliases::Point1i;                \
    using prefix ## Point2i              = typename prefix ## CoreAliases::Point2i;                \
    using prefix ## Point3i              = typename prefix ## CoreAliases::Point3i;                \
    using prefix ## Point4i              = typename prefix ## CoreAliases::Point4i;                \
    using prefix ## Point1u              = typename prefix ## CoreAliases::Point1u;                \
    using prefix ## Point2u              = typename prefix ## CoreAliases::Point2u;                \
    using prefix ## Point3u              = typename prefix ## CoreAliases::Point3u;                \
    using prefix ## Point4u              = typename prefix ## CoreAliases::Point4u;                \
    using prefix ## Point1f              = typename prefix ## CoreAliases::Point1f;                \
    using prefix ## Point2f              = typename prefix ## CoreAliases::Point2f;                \
    using prefix ## Point3f              = typename prefix ## CoreAliases::Point3f;                \
    using prefix ## Point4f              = typename prefix ## CoreAliases::Point4f;                \
    using prefix ## Point1d              = typename prefix ## CoreAliases::Point1d;                \
    using prefix ## Point2d              = typename prefix ## CoreAliases::Point2d;                \
    using prefix ## Point3d              = typename prefix ## CoreAliases::Point3d;                \
    using prefix ## Point4d              = typename prefix ## CoreAliases::Point4d;                \
    using prefix ## Normal3f             = typename prefix ## CoreAliases::Normal3f;               \
    using prefix ## Normal3d             = typename prefix ## CoreAliases::Normal3d;               \
    using prefix ## Matrix2f             = typename prefix ## CoreAliases::Matrix2f;               \
    using prefix ## Matrix2d             = typename prefix ## CoreAliases::Matrix2d;               \
    using prefix ## Matrix3f             = typename prefix ## CoreAliases::Matrix3f;               \
    using prefix ## Matrix3d             = typename prefix ## CoreAliases::Matrix3d;               \
    using prefix ## Matrix4f             = typename prefix ## CoreAliases::Matrix4f;               \
    using prefix ## Matrix4d             = typename prefix ## CoreAliases::Matrix4d;               \
    using prefix ## Quaternion4f         = typename prefix ## CoreAliases::Quaternion4f;           \
    using prefix ## Quaternion4d         = typename prefix ## CoreAliases::Quaternion4d;           \
    using prefix ## BoundingBox1f        = typename prefix ## CoreAliases::BoundingBox1f;          \
    using prefix ## BoundingBox2f        = typename prefix ## CoreAliases::BoundingBox2f;          \
    using prefix ## BoundingBox3f        = typename prefix ## CoreAliases::BoundingBox3f;          \
    using prefix ## BoundingBox4f        = typename prefix ## CoreAliases::BoundingBox4f;          \
    using prefix ## BoundingSphere1f     = typename prefix ## CoreAliases::BoundingSphere1f;       \
    using prefix ## BoundingSphere2f     = typename prefix ## CoreAliases::BoundingSphere2f;       \
    using prefix ## BoundingSphere3f     = typename prefix ## CoreAliases::BoundingSphere3f;       \
    using prefix ## BoundingSphere4f     = typename prefix ## CoreAliases::BoundingSphere4f;       \
    using prefix ## Frame3f              = typename prefix ## CoreAliases::Frame3f;                \
    using prefix ## Transform3f          = typename prefix ## CoreAliases::Transform3f;            \
    using prefix ## Transform4f          = typename prefix ## CoreAliases::Transform4f;            \
    using prefix ## Color1f              = typename prefix ## CoreAliases::Color1f;                \
    using prefix ## Color3f              = typename prefix ## CoreAliases::Color3f;

#define MTS_IMPORT_CORE_TYPES()                                                                    \
    MTS_IMPORT_CORE_TYPES_PREFIX(Float, )                                                          \
    using ScalarFloat = scalar_t<Float>;                                                           \
    MTS_IMPORT_CORE_TYPES_PREFIX(ScalarFloat, Scalar)

#define MTS_MASK_ARGUMENT(mask)                                                                    \
    (void) mask;                                                                                   \
    if constexpr (is_scalar_v<Float>)                                                              \
        mask = true;

#define MTS_MASKED_FUNCTION(profiler_phase, mask)                                                  \
    ScopedPhase scope_phase(profiler_phase);                                                       \
    (void) mask;                                                                                   \
    if constexpr (is_scalar_v<Float>)                                                              \
        mask = true;

NAMESPACE_BEGIN(filesystem)
class path;
NAMESPACE_END(filesystem)

namespace fs = filesystem;

NAMESPACE_END(mitsuba)

extern "C" {
#if defined(MTS_ENABLE_EMBREE)
    // Forward declarations for Embree
    typedef struct RTCDeviceTy* RTCDevice;
    typedef struct RTCSceneTy* RTCScene;
    typedef struct RTCGeometryTy* RTCGeometry;
#endif
};
