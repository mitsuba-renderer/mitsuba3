#include <random>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

/** Depth integrator.
 * Returns a radiance inversely proportional to the distance from the camera to
 * the closest intersected object, or 0 if there's none.
 */
class DepthIntegrator : public SamplingIntegrator {
public:
    DepthIntegrator(const Properties &props) : SamplingIntegrator(props) { }

    void configure_sampler(const Scene *scene, Sampler *sampler) override {
        SamplingIntegrator::configure_sampler(scene, sampler);
    }

    // =============================================================
    //! @{ \name Integrator interface
    // =============================================================
    template <typename RayDifferential,
              typename Point3 = typename RayDifferential::Point,
              typename RadianceSample = RadianceSample<Point3>>
    auto Li_impl(const RayDifferential &r, RadianceSample &r_rec,
                 const mask_t<value_t<Point3>> &active_) const {
        using Value    = value_t<Point3>;
        using Mask     = mask_t<Value>;
        using Spectrum = Spectrum<Value>;

        // Some aliases and local variables
        Mask active(active_);
        const Scene *scene = r_rec.scene;
        const BoundingBox<Point3> &bbox(scene->kdtree()->bbox());
        RayDifferential ray(r);
        Spectrum Li(0.0f);

        // Bounds on distances to objects (for scaling).
        Value min_distance = 0.0f;
        masked(min_distance, ~bbox.contains(ray.o)) = bbox.distance(ray.o);
        const Value max_distance = min_distance + hmax(bbox.extents());

        // Intersect ray against scene.
        // For a binary image, we could use shadow rays.
        active &= r_rec.ray_intersect(ray, active);

        if (none(active))  // Early return
            return Spectrum(0.0f);

        // Grayscale value (inversely proportional to the intersection distance).
        Value d = (r_rec.its.t - min_distance) / (max_distance - min_distance);
        Assert(all((d >= 0.0f) | ~active));
        masked(Li, active) += Spectrum(Value(1.0) - min(Value(1.0), d));

        return Li;
    }

    MTS_IMPLEMENT_INTEGRATOR()

    //! @}
    // =============================================================

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "DepthIntegrator[]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
};


MTS_IMPLEMENT_CLASS(DepthIntegrator, SamplingIntegrator);
MTS_EXPORT_PLUGIN(DepthIntegrator, "Depth integrator");
NAMESPACE_END(mitsuba)
