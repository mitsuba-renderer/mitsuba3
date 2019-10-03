#pragma once

#include <mitsuba/core/fwd.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum> class BSDF;
struct BSDFContext;
class ContinuousSpectrum;
class DifferentiableParameters;
class DifferentiableObject;
class Emitter;
class Endpoint;
class Film;
class ImageBlock;
class Integrator;
class Medium;
class Sampler;
class Scene;
class Sensor;
class Shape;
class ShapeKDTree;
class Texture3D;

template <typename Float, typename Spectrum> struct DirectionSample;
template <typename Float, typename Spectrum> struct PositionSample;
template <typename Float, typename Spectrum> struct Interaction;
template <typename Float, typename Spectrum> struct SurfaceInteraction;
template <typename Float, typename Spectrum> struct MediumInteraction;
template <typename Float, typename Spectrum> struct BSDFSample;
template <typename Float, typename Spectrum> struct RadianceSample;

template <typename Value> using StokesVector = enoki::Array<Value, 4, true>;
template <typename Value> using MuellerMatrix = enoki::Matrix<Value, 4, true>;

template <typename Float_, typename Spectrum_> struct Aliases {
    using Float    = Float_;
    using Spectrum = Spectrum_;

    using Mask    = mask_t<Float>;

    using Int32   =   int32_array_t<Float>;
    using UInt32  =  uint32_array_t<Float>;
    using Int64   =   int64_array_t<Float>;
    using UInt64  =  uint64_array_t<Float>;
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

    using Color3f  = Color<Float, 3>;

    using BoundingBox1f = BoundingBox<Point1f>;
    using BoundingBox2f = BoundingBox<Point2f>;
    using BoundingBox3f = BoundingBox<Point3f>;
    using BoundingBox4f = BoundingBox<Point4f>;

    using Frame3f              = Frame<Vector3f>;
    using Ray3f                = Ray<Point3f, Spectrum>;
    using RayDifferential3f    = RayDifferential<Point3f, Spectrum>;
    using Transform3f          = Transform<Float, 3>;
    using Transform4f          = Transform<Float, 4>;

    using PositionSample3f     = PositionSample<Float, Spectrum>;
    using DirectionSample3f    = DirectionSample<Float, Spectrum>;
    using Interaction3f        = Interaction<Float, Spectrum>;
    using SurfaceInteraction3f = SurfaceInteraction<Float, Spectrum>;
    using MediumInteraction3f  = MediumInteraction<Float, Spectrum>;
    using BSDFSample3f         = BSDFSample<Float, Spectrum>;

    // TODO
    // using Sampler              = mitsuba::Sampler<Float>;
    // using Shape                = mitsuba::Shape<Float>;
    // using Integrator           = mitsuba::Integrator<Float, Spectrum>;
    using BSDF                 = mitsuba::BSDF<Float, Spectrum>;
    // using Sensor               = mitsuba::Sensor<Float, Spectrum>;
    // using Emitter              = mitsuba::Emitter<Float, Spectrum>;
    // using Medium               = mitsuba::Medium<Float, Spectrum>;
    using Sampler              = mitsuba::Sampler;
    using Shape                = mitsuba::Shape;
    using Integrator           = mitsuba::Integrator;
    // using BSDF                 = mitsuba::BSDF;
    using Sensor               = mitsuba::Sensor;
    using Emitter              = mitsuba::Emitter;
    using Medium               = mitsuba::Medium;

    using BSDFPtr             = replace_scalar_t<Float, const BSDF *>;
    using MediumPtr           = replace_scalar_t<Float, const Medium *>;
    using ShapePtr            = replace_scalar_t<Float, const Shape *>;
    using EmitterPtr          = replace_scalar_t<Float, const Emitter *>;
};

#define MTS_IMPORT_TYPES()                                                                         \
    using Aliases              = mitsuba::Aliases<Float, Spectrum>;                                \
    using Mask                 = typename Aliases::Mask;                                           \
    using Int32                = typename Aliases::Int32;                                          \
    using UInt32               = typename Aliases::UInt32;                                         \
    using Int64                = typename Aliases::Int64;                                          \
    using UInt64               = typename Aliases::UInt64;                                         \
    using Float64              = typename Aliases::Float64;                                        \
    using Vector1i             = typename Aliases::Vector1i;                                       \
    using Vector2i             = typename Aliases::Vector2i;                                       \
    using Vector3i             = typename Aliases::Vector3i;                                       \
    using Vector4i             = typename Aliases::Vector4i;                                       \
    using Vector1u             = typename Aliases::Vector1u;                                       \
    using Vector2u             = typename Aliases::Vector2u;                                       \
    using Vector3u             = typename Aliases::Vector3u;                                       \
    using Vector4u             = typename Aliases::Vector4u;                                       \
    using Vector1f             = typename Aliases::Vector1f;                                       \
    using Vector2f             = typename Aliases::Vector2f;                                       \
    using Vector3f             = typename Aliases::Vector3f;                                       \
    using Vector4f             = typename Aliases::Vector4f;                                       \
    using Vector1d             = typename Aliases::Vector1d;                                       \
    using Vector2d             = typename Aliases::Vector2d;                                       \
    using Vector3d             = typename Aliases::Vector3d;                                       \
    using Vector4d             = typename Aliases::Vector4d;                                       \
    using Point1i              = typename Aliases::Point1i;                                        \
    using Point2i              = typename Aliases::Point2i;                                        \
    using Point3i              = typename Aliases::Point3i;                                        \
    using Point4i              = typename Aliases::Point4i;                                        \
    using Point1u              = typename Aliases::Point1u;                                        \
    using Point2u              = typename Aliases::Point2u;                                        \
    using Point3u              = typename Aliases::Point3u;                                        \
    using Point4u              = typename Aliases::Point4u;                                        \
    using Point1f              = typename Aliases::Point1f;                                        \
    using Point2f              = typename Aliases::Point2f;                                        \
    using Point3f              = typename Aliases::Point3f;                                        \
    using Point4f              = typename Aliases::Point4f;                                        \
    using Point1d              = typename Aliases::Point1d;                                        \
    using Point2d              = typename Aliases::Point2d;                                        \
    using Point3d              = typename Aliases::Point3d;                                        \
    using Point4d              = typename Aliases::Point4d;                                        \
    using Normal3f             = typename Aliases::Normal3f;                                       \
    using Normal3d             = typename Aliases::Normal3d;                                       \
    using Color3f              = typename Aliases::Color3f;                                        \
    using BoundingBox1f        = typename Aliases::BoundingBox1f;                                  \
    using BoundingBox2f        = typename Aliases::BoundingBox2f;                                  \
    using BoundingBox3f        = typename Aliases::BoundingBox3f;                                  \
    using BoundingBox4f        = typename Aliases::BoundingBox4f;                                  \
    using Frame3f              = typename Aliases::Frame3f;                                        \
    using Ray3f                = typename Aliases::Ray3f;                                          \
    using RayDifferential3f    = typename Aliases::RayDifferential3f;                              \
    using Transform3f          = typename Aliases::Transform3f;                                    \
    using Transform4f          = typename Aliases::Transform4f;                                    \
    using PositionSample3f     = typename Aliases::PositionSample3f;                               \
    using DirectionSample3f    = typename Aliases::DirectionSample3f;                              \
    using Interaction3f        = typename Aliases::Interaction3f;                                  \
    using SurfaceInteraction3f = typename Aliases::SurfaceInteraction3f;                           \
    using MediumInteraction3f  = typename Aliases::MediumInteraction3f;                            \
    using BSDFSample3f         = typename Aliases::BSDFSample3f;                                   \
    using Sampler              = typename Aliases::Sampler;                                        \
    using Shape                = typename Aliases::Shape;                                          \
    using Integrator           = typename Aliases::Integrator;                                     \
    using BSDF                 = typename Aliases::BSDF;                                           \
    using Sensor               = typename Aliases::Sensor;                                         \
    using Emitter              = typename Aliases::Emitter;                                        \
    using Medium               = typename Aliases::Medium;                                         \
    using EmitterPtr           = typename Aliases::EmitterPtr;                                     \
    using BSDFPtr              = typename Aliases::BSDFPtr;                                        \
    using ShapePtr             = typename Aliases::ShapePtr;

// -----------------------------------------------------------------------------

NAMESPACE_END(mitsuba)
