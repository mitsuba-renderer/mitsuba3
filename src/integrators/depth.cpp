#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Example of one an extremely simple type of integrator that is also
 * helpful for debugging: returns the distance from the camera to the closest
 * intersected object, or 0 if no intersection was found.
 */
template <typename Float, typename Spectrum>
class DepthIntegrator final : public SamplingIntegrator<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(SamplingIntegrator)
    MTS_IMPORT_TYPES(Scene, Sampler, Medium)

    DepthIntegrator(const Properties &props) : Base(props) { }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler * /* sampler */,
                                     const RayDifferential3f &ray,
                                     const Medium * /* medium */,
                                     Float * /* aovs */,
                                     Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        SurfaceInteraction3f si = scene->ray_intersect(ray, active);

        return {
            select(si.is_valid(), si.t, 0.f),
            si.is_valid()
        };
    }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS_VARIANT(DepthIntegrator, SamplingIntegrator)
MTS_EXPORT_PLUGIN(DepthIntegrator, "Depth integrator");
NAMESPACE_END(mitsuba)
