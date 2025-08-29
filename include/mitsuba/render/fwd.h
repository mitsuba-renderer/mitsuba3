#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/config.h>
#include <mitsuba/core/traits.h>

NAMESPACE_BEGIN(mitsuba)

struct BSDFContext;
struct OptixProgramGroupMapping;
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
template <typename Float, typename Spectrum> struct SilhouetteSample;
template <typename Float, typename Spectrum> struct PhaseFunctionContext;
template <typename Float, typename Spectrum> struct Interaction;
template <typename Float, typename Spectrum> struct MediumInteraction;
template <typename Float, typename Spectrum> struct SurfaceInteraction;
template <typename Float, typename Shape>    struct PreliminaryIntersection;

template <typename Float_, typename Spectrum_> struct RenderAliases {
    using Float                     = Float_;
    using Spectrum                  = Spectrum_;

    using Wavelength                = wavelength_t<Spectrum>;
    using UnpolarizedSpectrum       = unpolarized_spectrum_t<Spectrum>;

    using StokesVector4f            = StokesVector<UnpolarizedSpectrum>;
    using MuellerMatrix4f           = MuellerMatrix<UnpolarizedSpectrum>;

    using Ray3f                     = Ray<Point<Float, 3>, Spectrum>;
    using RayDifferential3f         = RayDifferential<Point<Float, 3>, Spectrum>;

    using PositionSample3f          = PositionSample<Float, Spectrum>;
    using DirectionSample3f         = DirectionSample<Float, Spectrum>;
    using BSDFSample3f              = BSDFSample3<Float, Spectrum>;
    using SilhouetteSample3f        = SilhouetteSample<Float, Spectrum>;
    using PhaseFunctionContext      = mitsuba::PhaseFunctionContext<Float, Spectrum>;
    using Interaction3f             = Interaction<Float, Spectrum>;
    using MediumInteraction3f       = MediumInteraction<Float, Spectrum>;
    using SurfaceInteraction3f      = SurfaceInteraction<Float, Spectrum>;
    using PreliminaryIntersection3f = PreliminaryIntersection<Float, mitsuba::Shape<Float, Spectrum>>;

    using Scene                  = mitsuba::Scene<Float, Spectrum>;
    using Sampler                = mitsuba::Sampler<Float, Spectrum>;
    using MicrofacetDistribution = mitsuba::MicrofacetDistribution<Float, Spectrum>;
    using Shape                  = mitsuba::Shape<Float, Spectrum>;
    using ShapeGroup             = mitsuba::ShapeGroup<Float, Spectrum>;
    using ShapeKDTree            = mitsuba::ShapeKDTree<Float, Spectrum>;
    using Mesh                   = mitsuba::Mesh<Float, Spectrum>;
    using Integrator             = mitsuba::Integrator<Float, Spectrum>;
    using SamplingIntegrator     = mitsuba::SamplingIntegrator<Float, Spectrum>;
    using MonteCarloIntegrator   = mitsuba::MonteCarloIntegrator<Float, Spectrum>;
    using AdjointIntegrator      = mitsuba::AdjointIntegrator<Float, Spectrum>;
    using BSDF                   = mitsuba::BSDF<Float, Spectrum>;
    using OptixDenoiser          = mitsuba::OptixDenoiser<Float, Spectrum>;
    using Sensor                 = mitsuba::Sensor<Float, Spectrum>;
    using ProjectiveCamera       = mitsuba::ProjectiveCamera<Float, Spectrum>;
    using Emitter                = mitsuba::Emitter<Float, Spectrum>;
    using Endpoint               = mitsuba::Endpoint<Float, Spectrum>;
    using Medium                 = mitsuba::Medium<Float, Spectrum>;
    using PhaseFunction          = mitsuba::PhaseFunction<Float, Spectrum>;
    using Film                   = mitsuba::Film<Float, Spectrum>;
    using ImageBlock             = mitsuba::ImageBlock<Float, Spectrum>;
    using ReconstructionFilter   = mitsuba::ReconstructionFilter<Float, Spectrum>;
    using Texture                = mitsuba::Texture<Float, Spectrum>;
    using Volume                 = mitsuba::Volume<Float, Spectrum>;
    using VolumeGrid             = mitsuba::VolumeGrid<Float, Spectrum>;

    using MeshAttribute          = mitsuba::MeshAttribute<Float, Spectrum>;

    using ObjectPtr              = dr::replace_scalar_t<Float, const Object *>;
    using BSDFPtr                = dr::replace_scalar_t<Float, const BSDF *>;
    using MediumPtr              = dr::replace_scalar_t<Float, const Medium *>;
    using PhaseFunctionPtr       = dr::replace_scalar_t<Float, const PhaseFunction *>;
    using ShapePtr               = dr::replace_scalar_t<Float, const Shape *>;
    using MeshPtr                = dr::replace_scalar_t<Float, const Mesh *>;
    using SensorPtr              = dr::replace_scalar_t<Float, const Sensor *>;
    using EmitterPtr             = dr::replace_scalar_t<Float, const Emitter *>;
    using TexturePtr             = dr::replace_scalar_t<Float, const Texture *>;
};

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
    using SilhouetteSample3f        = typename RenderAliases::SilhouetteSample3f;                  \
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
    using PhaseFunctionPtr       = typename RenderAliases::PhaseFunctionPtr;                       \
    using ShapePtr               = typename RenderAliases::ShapePtr;                               \
    using MeshPtr                = typename RenderAliases::MeshPtr;                                \
    using EmitterPtr             = typename RenderAliases::EmitterPtr;                             \
    using SensorPtr              = typename RenderAliases::SensorPtr;

NAMESPACE_END(mitsuba)
