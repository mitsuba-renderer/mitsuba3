#include <tuple>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <iostream>
NAMESPACE_BEGIN(mitsuba)

/**!

.. _integrator-depth:

Depth integrator (:monosp:`depth`)
----------------------------------

Example of one an extremely simple type of integrator that is also
helpful for debugging: returns the distance from the camera to the closest
intersected object, or 0 if no intersection was found.

.. tabs::
    .. code-tab::  xml
        :name: depth-integrator

        <integrator type="depth"/>

    .. code-tab:: python

        'type': 'depth'

 */

template <typename Float, typename Spectrum>
class DepthIntegrator final : public SamplingIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(SamplingIntegrator)
    MI_IMPORT_TYPES(Scene, Sampler, Medium, BSDF, BSDFPtr)

    DepthIntegrator(const Properties &props) : Base(props) { }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler * /* sampler */,
                                     const RayDifferential3f &ray_,
                                     const Medium * /* medium */,
                                     Float * /* aovs */,
                                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        SurfaceInteraction3f si = scene->ray_intersect(
            ray_, RayFlags::All | RayFlags::BoundaryTest, true, active);
        dr::masked(si, !si.is_valid()) = dr::zeros<SurfaceInteraction3f>();

        Ray3f ray                     = Ray3f(ray_);
        BSDFPtr bsdf = si.bsdf(ray);
        BSDFContext   bsdf_ctx;
        
        if (dr::any_or<true>(si.is_valid()))
        {
            Mask isGlass = has_flag(bsdf->flags(), BSDFFlags::Transmission);
            if (dr::any_or<true>(isGlass))
            {
                 auto [bsdf_sample, color]
                 = bsdf->sample(bsdf_ctx, si, 0, 0, true);
                Float eta = bsdf_sample.eta;
                std::cout<<eta;
                // std::cout<<bsdf_sample.eta;    
                //Ray3f next_ray = si.spawn_ray(si.to_world(bsdf_sample.wo));
                
                // SurfaceInteraction3f si2 = scene->ray_intersect(
                //     next_ray, RayFlags::All | RayFlags::BoundaryTest, true, active);
                // std::cout<<si2.is_valid();
                // SurfaceInteraction3f si_g = scene->ray_intersect(
                // ray_, RayFlags::All | RayFlags::BoundaryTest, true, active);
            }
            return {
            dr::select(si.is_valid(),si.t  , 0.f),
            si.is_valid()
            };
        }
        else
        {
            return {
                dr::select(si.is_valid(),0.0f, 0.f),
                si.is_valid()
            };
        }
        //Mask mask = active && has_flag(, BSDFFlags::Transmission);
        //auto test = bsdf->flags();
        // std::cout<<bsdf.is_valid();


    }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(DepthIntegrator, SamplingIntegrator)
MI_EXPORT_PLUGIN(DepthIntegrator, "Depth integrator");
NAMESPACE_END(mitsuba)
