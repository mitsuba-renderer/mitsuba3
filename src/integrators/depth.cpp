#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Depth integrator (for debugging) Returns a values proportional to the
 * distance from the camera to the closest intersected object, or 0 if no
 * intersection was found.
 */
class DepthIntegrator : public SamplingIntegrator {
public:
    DepthIntegrator(const Properties &props) : SamplingIntegrator(props) { }

    template <typename RayDifferential,
              typename Value = typename RayDifferential::Value,
              typename Point3 = typename RayDifferential::Point,
              typename RadianceSample = RadianceSample<Point3>,
              typename Spectrum = Spectrum<Value>,
              typename Mask = mask_t<Value>>
    Spectrum eval_impl(const RayDifferential &ray, RadianceSample &rs, Mask active) const {
        auto const &si = rs.ray_intersect(ray, active);
        Float scale = norm(rs.scene->kdtree()->bbox().extents());

        return select(si.is_valid(), si.t / scale, 0.f);
    }

    MTS_IMPLEMENT_INTEGRATOR()
    MTS_DECLARE_CLASS()
};


MTS_IMPLEMENT_CLASS(DepthIntegrator, SamplingIntegrator);
MTS_EXPORT_PLUGIN(DepthIntegrator, "Depth integrator");
NAMESPACE_END(mitsuba)
