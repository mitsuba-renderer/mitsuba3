#pragma once

#include <enoki/array_traits.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/traits.h>

NAMESPACE_BEGIN(mitsuba)

class DifferentiableObject;
class DifferentiableParameters;
struct BSDFContext;
template <typename Float, typename Spectrum> class BSDF;
template <typename Float, typename Spectrum> class ContinuousSpectrum;
template <typename Float, typename Spectrum> class Emitter;
template <typename Float, typename Spectrum> class Endpoint;
template <typename Float, typename Spectrum> class Film;
template <typename Float, typename Spectrum> class ImageBlock;
template <typename Float, typename Spectrum> class Integrator;
template <typename Float, typename Spectrum> class Medium;
template <typename Float, typename Spectrum> class Mesh;
template <typename Float, typename Spectrum> class MicrofacetDistribution;
template <typename Float, typename Spectrum> class ReconstructionFilter;
template <typename Float, typename Spectrum> class Sampler;
template <typename Float, typename Spectrum> class Scene;
template <typename Float, typename Spectrum> class Sensor;
template <typename Float, typename Spectrum> class Shape;
template <typename Float, typename Spectrum> class ShapeKDTree;
template <typename Float, typename Spectrum> class Texture3D;

template <typename Float, typename Spectrum> struct DirectionSample;
template <typename Float, typename Spectrum> struct PositionSample;
template <typename Float, typename Spectrum> struct Interaction;
template <typename Float, typename Spectrum> struct SurfaceInteraction;
template <typename Float, typename Spectrum> struct MediumInteraction;
template <typename Float, typename Spectrum> struct BSDFSample3;
template <typename Float, typename Spectrum> struct RadianceSample;

template <typename Float_, typename Spectrum_> struct RenderAliases {
    using Float    = Float_;
    using Spectrum = Spectrum_;

    using Wavelength = wavelength_t<Spectrum>;

    using Ray3f                  = Ray<Point<Float, 3>, Spectrum>;
    using RayDifferential3f      = RayDifferential<Point<Float, 3>, Spectrum>;

    using PositionSample3f       = PositionSample<Float, Spectrum>;
    using DirectionSample3f      = DirectionSample<Float, Spectrum>;
    using Interaction3f          = Interaction<Float, Spectrum>;
    using SurfaceInteraction3f   = SurfaceInteraction<Float, Spectrum>;
    using MediumInteraction3f    = MediumInteraction<Float, Spectrum>;
    using BSDFSample3f           = BSDFSample3<Float, Spectrum>;

    using Scene                  = mitsuba::Scene<Float, Spectrum>;
    using Sampler                = mitsuba::Sampler<Float, Spectrum>;
    using MicrofacetDistribution = mitsuba::MicrofacetDistribution<Float, Spectrum>;
    using Shape                  = mitsuba::Shape<Float, Spectrum>;
    using Mesh                   = mitsuba::Mesh<Float, Spectrum>;
    using Integrator             = mitsuba::Integrator<Float, Spectrum>;
    using BSDF                   = mitsuba::BSDF<Float, Spectrum>;
    using Sensor                 = mitsuba::Sensor<Float, Spectrum>;
    using Emitter                = mitsuba::Emitter<Float, Spectrum>;
    using Endpoint               = mitsuba::Endpoint<Float, Spectrum>;
    using Medium                 = mitsuba::Medium<Float, Spectrum>;
    using Film                   = mitsuba::Film<Float, Spectrum>;
    using ImageBlock             = mitsuba::ImageBlock<Float, Spectrum>;
    using ReconstructionFilter   = mitsuba::ReconstructionFilter<Float, Spectrum>;
    using ContinuousSpectrum     = mitsuba::ContinuousSpectrum<Float, Spectrum>;
    using Texture3D              = mitsuba::Texture3D<Float, Spectrum>;

    using ObjectPtr              = replace_scalar_t<Float, const Object *>;
    using BSDFPtr                = replace_scalar_t<Float, const BSDF *>;
    using MediumPtr              = replace_scalar_t<Float, const Medium *>;
    using ShapePtr               = replace_scalar_t<Float, const Shape *>;
    using EmitterPtr             = replace_scalar_t<Float, const Emitter *>;
};

#define MTS_VARIANT template <typename Float, typename Spectrum>

#define MTS_IMPORT_RENDER_BASIC_TYPES()                                                            \
    MTS_IMPORT_CORE_TYPES()                                                                        \
    using RenderAliases        = mitsuba::RenderAliases<Float, Spectrum>;                          \
    using Wavelength           = typename RenderAliases::Wavelength;                               \
    using Ray3f                = typename RenderAliases::Ray3f;                                    \
    using RayDifferential3f    = typename RenderAliases::RayDifferential3f;

#define MTS_IMPORT_TYPES(...)                                                                      \
    MTS_IMPORT_RENDER_BASIC_TYPES();                                                               \
    using PositionSample3f     = typename RenderAliases::PositionSample3f;                         \
    using DirectionSample3f    = typename RenderAliases::DirectionSample3f;                        \
    using Interaction3f        = typename RenderAliases::Interaction3f;                            \
    using SurfaceInteraction3f = typename RenderAliases::SurfaceInteraction3f;                     \
    using MediumInteraction3f  = typename RenderAliases::MediumInteraction3f;                      \
    using BSDFSample3f         = typename RenderAliases::BSDFSample3f;                             \
    ENOKI_MAP_USING(RenderAliases, ##__VA_ARGS__)

#define MTS_IMPORT_OBJECT_TYPES()                                                                  \
    using Scene                  = typename RenderAliases::Scene;                                  \
    using Sampler                = typename RenderAliases::Sampler;                                \
    using MicrofacetDistribution = typename RenderAliases::MicrofacetDistribution;                 \
    using Shape                  = typename RenderAliases::Shape;                                  \
    using Mesh                   = typename RenderAliases::Mesh;                                   \
    using Integrator             = typename RenderAliases::Integrator;                             \
    using BSDF                   = typename RenderAliases::BSDF;                                   \
    using Sensor                 = typename RenderAliases::Sensor;                                 \
    using Emitter                = typename RenderAliases::Emitter;                                \
    using Endpoint               = typename RenderAliases::Endpoint;                               \
    using Medium                 = typename RenderAliases::Medium;                                 \
    using Film                   = typename RenderAliases::Film;                                   \
    using ImageBlock             = typename RenderAliases::ImageBlock;                             \
    using ReconstructionFilter   = typename RenderAliases::ReconstructionFilter;                   \
    using ContinuousSpectrum     = typename RenderAliases::ContinuousSpectrum;                     \
    using Texture3D              = typename RenderAliases::Texture3D;                              \
    using ObjectPtr              = typename RenderAliases::ObjectPtr;                              \
    using BSDFPtr                = typename RenderAliases::BSDFPtr;                                \
    using MediumPtr              = typename RenderAliases::MediumPtr;                              \
    using ShapePtr               = typename RenderAliases::ShapePtr;                               \
    using EmitterPtr             = typename RenderAliases::EmitterPtr;

// -----------------------------------------------------------------------------

NAMESPACE_END(mitsuba)
