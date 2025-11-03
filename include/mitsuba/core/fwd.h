#pragma once

#include <mitsuba/core/platform.h>
#include <drjit-core/jit.h>
#include <drjit-core/traits.h>
#include <drjit/fwd.h>
#include <drjit/array_traits.h>
#include <drjit/map.h>
#include <drjit/call.h>
#include <drjit/if_stmt.h>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/// Define Dr.Jit namespace alias
namespace dr = drjit;

// =============================================================
//! @{ \name Forward declarations of Mitsuba classes/structs
// =============================================================

class Object;

/// Forward declaration of field template
template <typename DeviceType, typename HostType, typename SFINAE> struct field;

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
class Stream;
class StreamAppender;
class Struct;
class StructConverter;
class Thread;
class TraversalCallback;
class ZStream;
enum LogLevel : int;
enum class ObjectType : uint32_t;

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
template <typename Point, bool Affine>          struct Transform;
template <typename Point>                       using AffineTransform = Transform<Point, true>;
template <typename Point>                       using ProjectiveTransform = Transform<Point, false>;
template <typename Point, typename Spectrum>    struct Ray;
template <typename Point, typename Spectrum>    struct RayDifferential;
template <typename Point>                       struct BoundingBox;
template <typename Point>                       struct BoundingSphere;
template <typename Vector>                      struct Frame;
template <typename Float>                       struct DiscreteDistribution;
template <typename Float>                       struct ContinuousDistribution;

template <typename Spectrum> using StokesVector  = dr::Array<Spectrum, 4>;
template <typename Spectrum> using MuellerMatrix = dr::Matrix<Spectrum, 4>;

//! @}
// =============================================================

// =============================================================
//! @{ \name Buffer types
// =============================================================

template <typename Value,
          typename T = std::conditional_t<dr::is_static_array_v<Value>,
                                          dr::value_t<Value>, Value>>
using DynamicBuffer =
    std::conditional_t<dr::is_dynamic_array_v<T>, T, dr::DynamicArray<T>>;

//! @}
// =============================================================

// =============================================================
//! @{ \name CoreAliases struct
// =============================================================

template <typename Float_> struct CoreAliases {
    using Float = Float_;

    using Mask = dr::mask_t<Float>;
    using Bool = Mask;

    using Int8    = dr::replace_scalar_t<Float, int8_t>;
    using Int32   = dr::int32_array_t<Float>;
    using UInt32  = dr::uint32_array_t<Float>;
    using Int64   = dr::int64_array_t<Float>;
    using UInt64  = dr::uint64_array_t<Float>;
    using Float16 = dr::float16_array_t<Float>;
    using Float32 = dr::float32_array_t<Float>;
    using Float64 = dr::float64_array_t<Float>;

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

    using Matrix2f = dr::Matrix<Float, 2>;
    using Matrix2d = dr::Matrix<Float64, 2>;
    using Matrix3f = dr::Matrix<Float, 3>;
    using Matrix3d = dr::Matrix<Float64, 3>;
    using Matrix4f = dr::Matrix<Float, 4>;
    using Matrix4d = dr::Matrix<Float64, 4>;

    using Quaternion4f = dr::Quaternion<Float>;
    using Quaternion4d = dr::Quaternion<Float64>;

    using BoundingBox1f    = BoundingBox<Point1f>;
    using BoundingBox2f    = BoundingBox<Point2f>;
    using BoundingBox3f    = BoundingBox<Point3f>;
    using BoundingBox4f    = BoundingBox<Point4f>;
    using BoundingSphere1f = BoundingSphere<Point1f>;
    using BoundingSphere2f = BoundingSphere<Point2f>;
    using BoundingSphere3f = BoundingSphere<Point3f>;
    using BoundingSphere4f = BoundingSphere<Point4f>;

    using Frame3f     = Frame<Float>;

    using AffineTransform3f = AffineTransform<Point3f>;
    using AffineTransform4f = AffineTransform<Point4f>;
    using AffineTransform3d = AffineTransform<Point3d>;
    using AffineTransform4d = AffineTransform<Point4d>;
    using ProjectiveTransform3f = ProjectiveTransform<Point3f>;
    using ProjectiveTransform4f = ProjectiveTransform<Point4f>;
    using ProjectiveTransform3d = ProjectiveTransform<Point3d>;
    using ProjectiveTransform4d = ProjectiveTransform<Point4d>;

    using Color1f = Color<Float, 1>;
    using Color3f = Color<Float, 3>;
    using Color1d = Color<Float64, 1>;
    using Color3d = Color<Float64, 3>;

    using TensorXf = dr::Tensor<mitsuba::DynamicBuffer<Float>>;
    using TensorXf16 = dr::Tensor<mitsuba::DynamicBuffer<Float16>>;
    using TensorXf32 = dr::Tensor<mitsuba::DynamicBuffer<Float32>>;
    using TensorXf64 = dr::Tensor<mitsuba::DynamicBuffer<Float64>>;

    using Texture1f = dr::Texture<Float, 1>;
    using Texture2f = dr::Texture<Float, 2>;
    using Texture3f = dr::Texture<Float, 3>;

    using Texture1f16 = dr::Texture<Float16, 1>;
    using Texture2f16 = dr::Texture<Float16, 2>;
    using Texture3f16 = dr::Texture<Float16, 3>;

    /*
     * The following aliases are only used for casting to python object with PY_CAST_VARIANTS.
     * They won't be exposed by the MI_IMPORT_BASE_TYPES macro.
     */
    using Array1f = dr::Array<Float, 1>;
    using Array3f = dr::Array<Float, 3>;
    using DynamicBuffer = mitsuba::DynamicBuffer<Float>;

};

//! @}
// =============================================================

#define MI_VARIANT template <typename Float, typename Spectrum>

#define MI_IMPORT_CORE_TYPES_PREFIX(Float_, prefix)                                                \
    using prefix ## CoreAliases          = mitsuba::CoreAliases<Float_>;                           \
    using prefix ## Mask                 = typename prefix ## CoreAliases::Mask;                   \
    using prefix ## Bool                 = typename prefix ## CoreAliases::Bool;                   \
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
    using prefix ## Transform3f [[deprecated]] = typename prefix ## CoreAliases::AffineTransform3f;\
    using prefix ## Transform4f [[deprecated]] = typename prefix ## CoreAliases::AffineTransform4f;\
    using prefix ## Transform3d [[deprecated]] = typename prefix ## CoreAliases::AffineTransform3d;\
    using prefix ## Transform4d [[deprecated]] = typename prefix ## CoreAliases::AffineTransform4d;\
    using prefix ## AffineTransform3f    = typename prefix ## CoreAliases::AffineTransform3f;      \
    using prefix ## AffineTransform4f    = typename prefix ## CoreAliases::AffineTransform4f;      \
    using prefix ## AffineTransform3d    = typename prefix ## CoreAliases::AffineTransform3d;      \
    using prefix ## AffineTransform4d    = typename prefix ## CoreAliases::AffineTransform4d;      \
    using prefix ## ProjectiveTransform3f = typename prefix ## CoreAliases::ProjectiveTransform3f; \
    using prefix ## ProjectiveTransform4f = typename prefix ## CoreAliases::ProjectiveTransform4f; \
    using prefix ## ProjectiveTransform3d = typename prefix ## CoreAliases::ProjectiveTransform3d; \
    using prefix ## ProjectiveTransform4d = typename prefix ## CoreAliases::ProjectiveTransform4d; \
    using prefix ## Color1f              = typename prefix ## CoreAliases::Color1f;                \
    using prefix ## Color3f              = typename prefix ## CoreAliases::Color3f;                \
    using prefix ## Color1d              = typename prefix ## CoreAliases::Color1d;                \
    using prefix ## Color3d              = typename prefix ## CoreAliases::Color3d;                \
    using prefix ## TensorXf             = typename prefix ## CoreAliases::TensorXf;               \
    using prefix ## TensorXf16           = typename prefix ## CoreAliases::TensorXf16;             \
    using prefix ## TensorXf32           = typename prefix ## CoreAliases::TensorXf32;             \
    using prefix ## TensorXf64           = typename prefix ## CoreAliases::TensorXf64;             \
    using prefix ## Texture1f16          = typename prefix ## CoreAliases::Texture1f16;            \
    using prefix ## Texture2f16          = typename prefix ## CoreAliases::Texture2f16;            \
    using prefix ## Texture3f16          = typename prefix ## CoreAliases::Texture3f16;            \
    using prefix ## Texture1f            = typename prefix ## CoreAliases::Texture1f;              \
    using prefix ## Texture2f            = typename prefix ## CoreAliases::Texture2f;              \
    using prefix ## Texture3f            = typename prefix ## CoreAliases::Texture3f;

// Variadic macro to import a set of types from the base class
#define __MI_USING_TYPES_MACRO__(x) using typename Base::x;
#define MI_USING_TYPES(...) DRJIT_MAP(__MI_USING_TYPES_MACRO__, __VA_ARGS__)

// Variadic macro to import a set of variables from the base class
#define __MI_USING_MEMBERS_MACRO__(x) using Base::x;
#define MI_USING_MEMBERS(...) DRJIT_MAP(__MI_USING_MEMBERS_MACRO__, __VA_ARGS__)

#define MI_IMPORT_CORE_TYPES()                                                 \
    using ScalarFloat = dr::scalar_t<Float>;                                   \
    MI_IMPORT_CORE_TYPES_PREFIX(Float, )                                       \
    MI_IMPORT_CORE_TYPES_PREFIX(ScalarFloat, Scalar)

#define MI_MASK_ARGUMENT(mask)                                                 \
    (void) mask;                                                               \
    if constexpr (!dr::is_array_v<Float>)                                      \
        mask = true;

#define MI_MASKED_FUNCTION(profiler_phase, mask)                               \
    ScopedPhase scope_phase(profiler_phase);                                   \
    (void) mask;                                                               \
    if constexpr (!dr::is_array_v<Float>)                                      \
        mask = true;

NAMESPACE_BEGIN(filesystem)
class path;
NAMESPACE_END(filesystem)

namespace fs = filesystem;

#if defined(MI_ENABLE_EMBREE)
extern "C" {
    // Forward declarations for Embree (we import Embree into mitsuba's namespace
    // to avoid name clashes with other libraries).
    typedef struct RTCDeviceTy* RTCDevice;
    typedef struct RTCSceneTy* RTCScene;
    typedef struct RTCGeometryTy* RTCGeometry;
}
#endif

NAMESPACE_END(mitsuba)

extern "C" {

// =============================================================
//! @{ \name Helper macros
// =============================================================

#define MI_DECLARE_ENUM_OPERATORS_IMPL(name, neq_expr)                         \
    constexpr uint32_t operator|(name f1, name f2) {                           \
        return (uint32_t) f1 | (uint32_t) f2;                                  \
    }                                                                          \
    constexpr uint32_t operator|(uint32_t f1, name f2) {                       \
        return f1 | (uint32_t) f2;                                             \
    }                                                                          \
    constexpr uint32_t operator&(name f1, name f2) {                           \
        return (uint32_t) f1 & (uint32_t) f2;                                  \
    }                                                                          \
    constexpr uint32_t operator&(uint32_t f1, name f2) {                       \
        return f1 & (uint32_t) f2;                                             \
    }                                                                          \
    constexpr uint32_t operator~(name f1) { return ~(uint32_t) f1; }           \
    constexpr uint32_t operator+(name e) { return (uint32_t) e; }              \
    template <typename UInt32>                                                 \
    constexpr auto has_flag(UInt32 flags, name f) {                            \
        return neq_expr(flags & (uint32_t) f, 0u);                             \
    }

#if defined(MI_ENABLE_CUDA) || defined(MI_ENABLE_LLVM)
#  define MI_DECLARE_ENUM_OPERATORS(name)                                      \
   MI_DECLARE_ENUM_OPERATORS_IMPL(name,                                        \
                             [&](UInt32 a, UInt32 b) {                         \
                                 return a != b;                                \
                             })
#else
# define MI_DECLARE_ENUM_OPERATORS(name)                                       \
  MI_DECLARE_ENUM_OPERATORS_IMPL(name,                                         \
                            [&](UInt32 a, UInt32 b) {                          \
                                return a != b;                                 \
                            })
#endif

/**
 * \brief Macro, generating the implementation of the ``traverse_1_cb_ro``
 *     method.
 *
 * The first argument should be the base class, from which the current class
 * inherits. The other arguments should list all members of that class, which
 * are supposed to be read only traversable.
 */
#define MI_TRAVERSE_CB_RO(Base, ...)                                           \
        void traverse_1_cb_ro(void *payload,                                   \
                              drjit::detail::traverse_callback_ro fn)          \
            const override {                                                   \
            static_assert(std::is_base_of<                                     \
                          drjit::TraversableBase,                              \
                          std::remove_pointer_t<decltype(this)>>::value);      \
            /*                                                                 \
             * Only traverse the scene for frozen functions, since             \
             * accidentally traversing the scene in loops or vcalls can cause  \
             * errors with variable size mismatches, and backpropagation of    \
             * gradients.                                                      \
             */                                                                \
            if (!jit_flag(JitFlag::EnableObjectTraversal))                     \
                return;                                                        \
            if constexpr (!std::is_same_v<Base, drjit::TraversableBase>)       \
                Base::traverse_1_cb_ro(payload, fn);                           \
            DRJIT_MAP(DR_TRAVERSE_MEMBER_RO, __VA_ARGS__)                      \
        }

/**
 * \breif Macro, generating the implementation of the ``traverse_1_cb_rw``
 *     method.
 *
 * The first argument should be the base class, from which the current class
 * inherits. The other arguments should list all members of that class, which
 * are supposed to be read and write traversable.
 */
#define MI_TRAVERSE_CB_RW(Base, ...)                                           \
        void traverse_1_cb_rw(                                                 \
            void *payload, drjit::detail::traverse_callback_rw fn) override {  \
            static_assert(std::is_base_of<                                     \
                          drjit::TraversableBase,                              \
                          std::remove_pointer_t<decltype(this)>>::value);      \
            /*                                                                 \
             * Only traverse the scene for frozen functions, since             \
             * accidentally traversing the scene in loops or vcalls can cause  \
             * errors with variable size mismatches, and backpropagation of    \
             * gradients.                                                      \
             */                                                                \
            if (!jit_flag(JitFlag::EnableObjectTraversal))                     \
                return;                                                        \
            if constexpr (!std::is_same_v<Base, drjit::TraversableBase>)       \
                Base::traverse_1_cb_rw(payload, fn);                           \
            DRJIT_MAP(DR_TRAVERSE_MEMBER_RW, __VA_ARGS__)                      \
        }

/**
 * \brief Macro, generating the implementations of the ``traverse_1_cb_ro`` and
 *     ``traverse_1_cb_rw`` methods.
 *
 * The first argument should be the base class, from which the current class
 * inherits. The other arguments should list all members of that class, which
 * are supposed to be read and write traversable.
 *
 * This macro differs from ``DR_TRAVERSE_CB`` in that the functions it generates
 * do not traverse the object if the flag ``JitFlag::EnableObjectTraversal``
 * is not set. This is required, to circumvent issues where scene variables would
 * accidentally be added to loop states, and dispatch arguments.
 */
#define MI_TRAVERSE_CB(Base, ...)                                              \
public:                                                                        \
    MI_TRAVERSE_CB_RO(Base, __VA_ARGS__)                                       \
    MI_TRAVERSE_CB_RW(Base, __VA_ARGS__)

/**
 * \brief Macro, generating the implementations of the ``traverse_1_cb_ro`` and
 *     ``traverse_1_cb_rw`` methods.
 *
 * In contrast to ``MI_TRAVERSE_CB``, this macro only declares the functions and
 * ``MI_IMPLEMENT_TRAVERSE_CB`` implements them. Use this macro if the class is
 * declared in a header file and implemented in a source file.
 */
#define MI_DECLARE_TRAVERSE_CB(...)                                            \
        DRJIT_INLINE auto traverse_1_cb_fields_() {                            \
            return drjit::tie(__VA_ARGS__);                                    \
        }                                                                      \
        DRJIT_INLINE auto traverse_1_cb_fields_() const {                      \
            return drjit::tie(__VA_ARGS__);                                    \
        }                                                                      \
                                                                               \
    public:                                                                    \
        void traverse_1_cb_ro(void *payload,                                   \
                              drjit::detail::traverse_callback_ro fn)          \
            const override;                                                    \
        void traverse_1_cb_rw(                                                 \
            void *payload, drjit::detail::traverse_callback_rw fn) override;

/**
 * \brief Macro, generating the implementations of the ``traverse_1_cb_ro`` and
 *     ``traverse_1_cb_rw`` methods.
 *
 * In contrast to ``MI_TRAVERSE_CB``, this macro generates the implementations
 * for the functions declared by ``MI_DECLARE_TRAVERSE_CB``. Use this macro if
 * the class is declared in a header file and implemented in a source file.
 */
#define MI_IMPLEMENT_TRAVERSE_CB(Type, Base)                                   \
        MI_VARIANT                                                             \
        void Type<Float, Spectrum>::traverse_1_cb_ro(                          \
            void *payload, drjit::detail::traverse_callback_ro fn) const {     \
                                                                               \
            /*                                                                 \
             * Only traverse the scene for frozen functions, since             \
             * accidentally traversing the scene in loops or vcalls can cause  \
             * errors with variable size mismatches, and backpropagation of    \
             * gradients.                                                      \
             */                                                                \
            if (!jit_flag(JitFlag::EnableObjectTraversal))                     \
                return;                                                        \
                                                                               \
            if constexpr (!std::is_same_v<Base, drjit::TraversableBase>)       \
                Base::traverse_1_cb_ro(payload, fn);                           \
                                                                               \
            drjit::traverse_1(this->traverse_1_cb_fields_(),                   \
                              [payload, fn](auto &x) {                         \
                                  drjit::traverse_1_fn_ro(x, payload, fn);     \
                              });                                              \
        }                                                                      \
        MI_VARIANT                                                             \
        void Type<Float, Spectrum>::traverse_1_cb_rw(                          \
            void *payload, drjit::detail::traverse_callback_rw fn) {           \
                                                                               \
            /*                                                                 \
             * Only traverse the scene for frozen functions, since             \
             * accidentally traversing the scene in loops or vcalls can cause  \
             * errors with variable size mismatches, and backpropagation of    \
             * gradients.                                                      \
             */                                                                \
            if (!jit_flag(JitFlag::EnableObjectTraversal))                     \
                return;                                                        \
                                                                               \
            if constexpr (!std::is_same_v<Base, drjit::TraversableBase>)      \
                Base::traverse_1_cb_rw(payload, fn);                           \
            drjit::traverse_1(this->traverse_1_cb_fields_(),                   \
                              [payload, fn](auto &x) {                         \
                                  drjit::traverse_1_fn_rw(x, payload, fn);     \
                              });                                              \
        }

//! @}
// =============================================================

};
