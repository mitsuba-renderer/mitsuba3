#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>

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
    MI_IMPORT_TYPES(Scene, Sampler, Medium)

    DepthIntegrator(const Properties &props) : Base(props) { }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler * /* sampler */,
                                     const RayDifferential3f &ray,
                                     const Medium * /* medium */,
                                     Float * /* aovs */,
                                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        PreliminaryIntersection3f pi = scene->ray_intersect_preliminary(
            ray, /* coherent = */ true, active);

        return {
            dr::select(pi.is_valid(), pi.t, 0.f),
            pi.is_valid()
        };
    }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(DepthIntegrator, SamplingIntegrator)
MI_EXPORT_PLUGIN(DepthIntegrator, "Depth integrator");
NAMESPACE_END(mitsuba)
