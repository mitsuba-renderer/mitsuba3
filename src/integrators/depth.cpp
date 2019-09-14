#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Depth integrator (for debugging). Returns the distance from the
 * camera to the closest intersected object, or 0 if no intersection was found.
 */
template <typename Float, typename Spectrum>
class DepthIntegrator : public SamplingIntegrator<Float, Spectrum> {
    /// Declare runtime type information for Mitsuba's object model
    MTS_DECLARE_CLASS()

    /// Import types derived from Float and Spectrum (e.g. Vector3f, etc.)
    MTS_IMPORT_TYPES()
public:
    DepthIntegrator(const Properties &props) : SamplingIntegrator(props) {}

    std::pair<Spectrum, Mask> eval(const Scene *scene, Sampler *sampler,
                                   const RayDifferential3f &ray, Mask active) const {
        SurfaceInteraction3f si = scene->ray_intersect(ray, active);
        return { select(si.is_valid(), si.t, 0.f), si.is_valid() };
    }
};

MTS_IMPLEMENT_CLASS(DepthIntegrator, SamplingIntegrator);
MTS_EXPORT_PLUGIN(DepthIntegrator, "Depth integrator");
NAMESPACE_END(mitsuba)
