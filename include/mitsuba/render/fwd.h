#pragma once

#include <mitsuba/core/fwd.h>
// #include <mitsuba/core/spectrum.h>
#include <mitsuba/core/traits.h>

NAMESPACE_BEGIN(mitsuba)

struct BSDFContext;
template <typename Float, typename Spectrum> class BSDF;
template <typename Float, typename Spectrum> class OptixDenoiser;
template <typename Float, typename Spectrum> class Emitter;
template <typename Float, typename Spectrum> class Endpoint;
template <typename Float, typename Spectrum> class Film;
template <typename Float, typename Spectrum> class ImageBlock;
template <typename Float, typename Spectrum> class Integrator;
template <typename Float, typename Spectrum> class SamplingIntegrator;
template <typename Float, typename Spectrum> class MonteCarloIntegrator;
template <typename Float, typename Spectrum> class AdjointIntegrator;
template <typename Float, typename Spectrum> class Medium;
template <typename Float, typename Spectrum> class Mesh;
template <typename Float, typename Spectrum> class MicrofacetDistribution;
template <typename Float, typename Spectrum> class ReconstructionFilter;
template <typename Float, typename Spectrum> class Sampler;
template <typename Float, typename Spectrum> class Scene;
template <typename Float, typename Spectrum> class Sensor;
template <typename Float, typename Spectrum> class PhaseFunction;
template <typename Float, typename Spectrum> class ProjectiveCamera;
template <typename Float, typename Spectrum> class Shape;
template <typename Float, typename Spectrum> class ShapeGroup;
template <typename Float, typename Spectrum> class ShapeKDTree;
template <typename Float, typename Spectrum> class Texture;
template <typename Float, typename Spectrum> class Volume;
template <typename Float, typename Spectrum> class VolumeGrid;
template <typename Float, typename Spectrum> class MeshAttribute;

template <typename Float, typename Spectrum> struct DirectionSample;
template <typename Float, typename Spectrum> struct PositionSample;
template <typename Float, typename Spectrum> struct BSDFSample3;
template <typename Float, typename Spectrum> struct PhaseFunctionContext;
template <typename Float, typename Spectrum> struct Interaction;
template <typename Float, typename Spectrum> struct MediumInteraction;
template <typename Float, typename Spectrum> struct SurfaceInteraction;
template <typename Float, typename Shape>    struct PreliminaryIntersection;


template <typename Float_, typename Spectrum_> struct RenderAliases {
    using Float                     = Float_;
    using Spectrum                  = Spectrum_;

    /// Strip away any masking-related wrappers from 'Float' and 'Spectrum'
    using FloatU                 = underlying_t<Float>;
    using SpectrumU              = underlying_t<Spectrum>;

    using Wavelength                = wavelength_t<Spectrum>;
    using UnpolarizedSpectrum       = unpolarized_spectrum_t<Spectrum>;

    using StokesVector4f            = StokesVector<UnpolarizedSpectrum>;
    using MuellerMatrix4f           = MuellerMatrix<UnpolarizedSpectrum>;

    using Ray3f                     = Ray<Point<Float, 3>, Spectrum>;
    using RayDifferential3f         = RayDifferential<Point<Float, 3>, Spectrum>;

    using PositionSample3f          = PositionSample<Float, Spectrum>;
    using DirectionSample3f         = DirectionSample<Float, Spectrum>;
    using BSDFSample3f              = BSDFSample3<Float, Spectrum>;
    using PhaseFunctionContext      = mitsuba::PhaseFunctionContext<Float, Spectrum>;
    using Interaction3f             = Interaction<Float, Spectrum>;
    using MediumInteraction3f       = MediumInteraction<Float, Spectrum>;
    using SurfaceInteraction3f      = SurfaceInteraction<Float, Spectrum>;
    using PreliminaryIntersection3f = PreliminaryIntersection<Float, mitsuba::Shape<FloatU, SpectrumU>>;

    using Scene                  = mitsuba::Scene<FloatU, SpectrumU>;
    using Sampler                = mitsuba::Sampler<FloatU, SpectrumU>;
    using MicrofacetDistribution = mitsuba::MicrofacetDistribution<FloatU, SpectrumU>;
    using Shape                  = mitsuba::Shape<FloatU, SpectrumU>;
    using ShapeGroup             = mitsuba::ShapeGroup<FloatU, SpectrumU>;
    using ShapeKDTree            = mitsuba::ShapeKDTree<FloatU, SpectrumU>;
    using Mesh                   = mitsuba::Mesh<FloatU, SpectrumU>;
    using Integrator             = mitsuba::Integrator<FloatU, SpectrumU>;
    using SamplingIntegrator     = mitsuba::SamplingIntegrator<FloatU, SpectrumU>;
    using MonteCarloIntegrator   = mitsuba::MonteCarloIntegrator<FloatU, SpectrumU>;
    using AdjointIntegrator      = mitsuba::AdjointIntegrator<FloatU, SpectrumU>;
    using BSDF                   = mitsuba::BSDF<FloatU, SpectrumU>;
    using OptixDenoiser          = mitsuba::OptixDenoiser<FloatU, SpectrumU>;
    using Sensor                 = mitsuba::Sensor<FloatU, SpectrumU>;
    using ProjectiveCamera       = mitsuba::ProjectiveCamera<FloatU, SpectrumU>;
    using Emitter                = mitsuba::Emitter<FloatU, SpectrumU>;
    using Endpoint               = mitsuba::Endpoint<FloatU, SpectrumU>;
    using Medium                 = mitsuba::Medium<FloatU, SpectrumU>;
    using PhaseFunction          = mitsuba::PhaseFunction<FloatU, SpectrumU>;
    using Film                   = mitsuba::Film<FloatU, SpectrumU>;
    using ImageBlock             = mitsuba::ImageBlock<FloatU, SpectrumU>;
    using ReconstructionFilter   = mitsuba::ReconstructionFilter<FloatU, SpectrumU>;
    using Texture                = mitsuba::Texture<FloatU, SpectrumU>;
    using Volume                 = mitsuba::Volume<FloatU, SpectrumU>;
    using VolumeGrid             = mitsuba::VolumeGrid<FloatU, SpectrumU>;

    using MeshAttribute          = mitsuba::MeshAttribute<FloatU, SpectrumU>;

    using ObjectPtr              = dr::replace_scalar_t<Float, const Object *>;
    using BSDFPtr                = dr::replace_scalar_t<Float, const BSDF *>;
    using MediumPtr              = dr::replace_scalar_t<Float, const Medium *>;
    using PhaseFunctionPtr       = dr::replace_scalar_t<Float, const PhaseFunction *>;
    using ShapePtr               = dr::replace_scalar_t<Float, const Shape *>;
    using SensorPtr              = dr::replace_scalar_t<Float, const Sensor *>;
    using EmitterPtr             = dr::replace_scalar_t<Float, const Emitter *>;
};

#define MMI_USING_MEMBERS_MACRO2(x) \
    using Base::x;

/**
 * \brief Imports the desired methods and fields by generating a sequence of
 * `using` declarations. This is useful when inheriting from template parents,
 * since methods and fields must be explicitly made visible.
 *
 * For example,
 *
 * \code
 *     MI_IMPORT_BASE(BSDF, m_flags, m_components)
 * \endcode
 *
 * expands to
 *
 * \code
 *     using Base = BSDF<Float, Spectrum>;
 *     using Base::m_flags;
 *     using Base::m_components;
 * \endcode
 */
#define MI_IMPORT_BASE(Name, ...)                                                                  \
    using Base = Name<Float, Spectrum>;                                                            \
    MI_USING_MEMBERS(__VA_ARGS__)


#define MI_IMPORT_RENDER_BASIC_TYPES()                                                             \
    MI_IMPORT_CORE_TYPES()                                                                         \
    using RenderAliases        = mitsuba::RenderAliases<Float, Spectrum>;                          \
    using Wavelength           = typename RenderAliases::Wavelength;                               \
    using UnpolarizedSpectrum  = typename RenderAliases::UnpolarizedSpectrum;                      \
    using StokesVector4f       = typename RenderAliases::StokesVector4f;                           \
    using MuellerMatrix4f      = typename RenderAliases::MuellerMatrix4f;                          \
    using Ray3f                = typename RenderAliases::Ray3f;                                    \
    using RayDifferential3f    = typename RenderAliases::RayDifferential3f;

#define MI_IMPORT_TYPES_MACRO(x) using x = typename RenderAliases::x;

#define MI_IMPORT_TYPES(...)                                                                       \
    MI_IMPORT_RENDER_BASIC_TYPES()                                                                 \
    using PositionSample3f          = typename RenderAliases::PositionSample3f;                    \
    using DirectionSample3f         = typename RenderAliases::DirectionSample3f;                   \
    using Interaction3f             = typename RenderAliases::Interaction3f;                       \
    using SurfaceInteraction3f      = typename RenderAliases::SurfaceInteraction3f;                \
    using MediumInteraction3f       = typename RenderAliases::MediumInteraction3f;                 \
    using PreliminaryIntersection3f = typename RenderAliases::PreliminaryIntersection3f;           \
    using BSDFSample3f              = typename RenderAliases::BSDFSample3f;                        \
    DRJIT_MAP(MI_IMPORT_TYPES_MACRO, __VA_ARGS__)

#define MI_IMPORT_OBJECT_TYPES()                                                                   \
    using Scene                  = typename RenderAliases::Scene;                                  \
    using Sampler                = typename RenderAliases::Sampler;                                \
    using MicrofacetDistribution = typename RenderAliases::MicrofacetDistribution;                 \
    using Shape                  = typename RenderAliases::Shape;                                  \
    using ShapeKDTree            = typename RenderAliases::ShapeKDTree;                            \
    using Mesh                   = typename RenderAliases::Mesh;                                   \
    using Integrator             = typename RenderAliases::Integrator;                             \
    using SamplingIntegrator     = typename RenderAliases::SamplingIntegrator;                     \
    using MonteCarloIntegrator   = typename RenderAliases::MonteCarloIntegrator;                   \
    using AdjointIntegrator      = typename RenderAliases::AdjointIntegrator;                      \
    using BSDF                   = typename RenderAliases::BSDF;                                   \
    using OptixDenoiser          = typename RenderAliases::OptixDenoiser;                          \
    using Sensor                 = typename RenderAliases::Sensor;                                 \
    using ProjectiveCamera       = typename RenderAliases::ProjectiveCamera;                       \
    using Emitter                = typename RenderAliases::Emitter;                                \
    using Endpoint               = typename RenderAliases::Endpoint;                               \
    using Medium                 = typename RenderAliases::Medium;                                 \
    using PhaseFunction          = typename RenderAliases::PhaseFunction;                          \
    using Film                   = typename RenderAliases::Film;                                   \
    using ImageBlock             = typename RenderAliases::ImageBlock;                             \
    using ReconstructionFilter   = typename RenderAliases::ReconstructionFilter;                   \
    using Texture                = typename RenderAliases::Texture;                                \
    using Volume                 = typename RenderAliases::Volume;                                 \
    using ObjectPtr              = typename RenderAliases::ObjectPtr;                              \
    using BSDFPtr                = typename RenderAliases::BSDFPtr;                                \
    using MediumPtr              = typename RenderAliases::MediumPtr;                              \
    using ShapePtr               = typename RenderAliases::ShapePtr;                               \
    using EmitterPtr             = typename RenderAliases::EmitterPtr;                             \
    using SensorPtr              = typename RenderAliases::SensorPtr;

NAMESPACE_END(mitsuba)
