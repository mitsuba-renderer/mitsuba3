#include <random>
#include <enoki/stl.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

namespace {
template <typename Value>
Value mi_weight(Value pdf_a, Value pdf_b) {
    pdf_a *= pdf_a;
    pdf_b *= pdf_b;
    return pdf_a / (pdf_a + pdf_b);
};
}  // namespace

/**
 * Path tracer.
 *
 * Adapted from PBRT v3 by:
 *   Matt Pharr, Greg Humphreys, and Wenzel Jakob
 */
class PathIntegrator : public MonteCarloIntegrator {
public:
    PathIntegrator(const Properties &props) : MonteCarloIntegrator(props) { }

    // =============================================================
    //! @{ \name Integrator interface
    // =============================================================
    template <typename RayDifferential,
              typename Point3 = typename RayDifferential::Point,
              typename RadianceSample = RadianceSample<Point3>,
              typename Value = typename RayDifferential::Value>
    auto eval_impl(const RayDifferential &r, RadianceSample &rs,
                   mask_t<Value> active) const {
        using Mask                = mask_t<Value>;
        using Spectrum            = Spectrum<Value>;
        using BSDFSample          = BSDFSample<Point3>;
        using DirectionSample     = DirectionSample<Point3>;
        using SurfaceInteraction3 = SurfaceInteraction<Point3>;
        using BSDFPtr             = like_t<Value, const BSDF *>;
        using Frame               = Frame<Point3>;

        // Some aliases and local variables
        const Scene *scene = rs.scene;
        RayDifferential ray(r);
        Spectrum Li(0.0f), throughput(1.0f);
        BSDFContext ctx;

        Mask scattered(false);
        /* Tracks the accumulated effect of radiance scaling due to rays passing
           through refractive boundaries. */
        Value eta(1.0f);

        // Intersect ray with scene
        SurfaceInteraction3 &si = rs.ray_intersect(ray, active);
        Mask hit = si.is_valid();
        active = active && hit;
        // Used to set alpha value at this location.
        const Mask hit_anything = active;

        for (int bounces = 0;; ++bounces) {
            /* If no intersection could be found, potentially return
               radiance from a environment luminaire if it exists */
            if (bounces == 0 && any(!hit) || any(scattered)) {
                masked(Li, active && !hit) =
                    Li + throughput * scene->eval_environment(ray, active && !hit);
            }

            // Path termination
            active = active && ((m_max_depth == -1) || bounces < m_max_depth);
            if (none(active))
                break;

            // Include emitted radiance
            auto is_emitter = si.is_emitter();
            if (bounces == 0 && any(is_emitter)) {
                masked(Li, active && is_emitter) =
                    Li + throughput
                         * si.emission(active && is_emitter);
            }

            BSDFPtr bsdf = si.bsdf(ray);

            // TODO: handle medium transitions.

            /* ============================================================== */
            /*                  Direct illumination sampling                  */
            /* ============================================================== */

            /* Sample a light source to estimate direct illumination,
               except for perfectly specular BSDFs. */
            auto bsdf_flags = bsdf->flags();
            DirectionSample ds;
            Spectrum value;

            auto not_specular = active && eq(bsdf_flags & BSDF::EDelta, 0u);
            if (any(not_specular)) {
                std::tie(ds, value) = scene->sample_emitter_direction(
                        si, rs.next_2d(active), true, not_specular);
                Assert(all_nested(value >= 0));

                Mask enabled = active && not_specular && neq(ds.pdf, 0.0f);
                // Prepare a BSDF query.
                auto wo = si.to_local(ds.d);
                if (m_strict_normals) {
                    Mask same_side = (dot(si.n, ds.d) * Frame::cos_theta(wo)) > 0;
                    enabled = enabled && same_side;
                }

                if (any(enabled)) {
                    Spectrum bsdf_val = bsdf->eval(si, ctx, wo, enabled);
                    enabled = enabled && any(neq(bsdf_val, 0.0f));

                    /* Calculate the probability of sampling that direction
                       using BSDF sampling. */
                    Value bsdf_pdf = bsdf->pdf(si, ctx, wo, enabled);
                    // Weight using the power heuristic
                    Value weight = mi_weight(ds.pdf, bsdf_pdf);

                    masked(Li, enabled) = Li + throughput * value
                                               * bsdf_val * weight;
                }
            }

            /* ============================================================== */
            /*                            BSDF sampling                       */
            /* ============================================================== */

            /* Sample the BSDF (including foreshortening factor) to get the
               next path direction. */
            BSDFSample bs;
            Spectrum bsdf_val;  // Used to update `throughput`
            std::tie(bs, bsdf_val) = bsdf->sample(si, ctx, rs.next_1d(),
                                                  rs.next_2d(), active);

            // Update ray for next path segment
            active = active && any(bsdf_val > 0.0f) && (bs.pdf > 0.0f);
            scattered = neq(bsdf_flags & BSDF::EDelta, 0u);

            // Prevent light leaks due to the use of shading normals
            active = active && !(Mask(m_strict_normals)
                                 && (dot(si.sh_frame.n, si.to_world(bs.wo))
                                     * Frame::cos_theta(bs.wo) <= 0));

            // Trace the ray with this new direction
            ray = RayDifferential(si.p, si.to_world(bs.wo), si.time, ray.wavelengths);
            // Intersect against the scene
            SurfaceInteraction3 &si_next = rs.ray_intersect(ray, active);
            hit = si_next.is_valid();

            // Account for any emitted light (from path vertex or the environment)
            Spectrum value_emitter(0.0f);
            Mask hit_emitter = hit && active && si_next.is_emitter();
            // For emitters
            if (any(hit_emitter)) {
                masked(value_emitter, hit_emitter) =
                    value_emitter + si_next.emission(hit_emitter);
                ds.set_query(ray, si_next);
            }
            // For stray rays
            auto hit_environment = Mask(scene->has_environment_emitter())
                                   && active && !hit;
            if (any(hit_environment)) {
                masked(value_emitter, hit_environment) =
                    value_emitter + scene->eval_environment(ray, hit_environment);
                hit_emitter = hit_emitter || hit_environment;
            }
            active = active && hit;

            /* Keep track of the throughput and relative refractive index along
               the path. */
            throughput *= bsdf_val;
            eta *= bs.eta;

            /* If a emitter was hit, estimate the local illumination and
               weight using the power heuristic. */
            if (any(hit_emitter)) {
                /* Compute the prob. of generating that direction using the
                   implemented direct illumination sampling technique. */
                Value lum_pdf = select(
                    neq(bs.sampled_type & BSDF::EDelta, 0u),
                    0.0f,
                    scene->pdf_emitter_direction(si_next, ds, hit_emitter)
                );
                masked(Li, hit_emitter) =
                    Li + throughput * value_emitter * mi_weight(bs.pdf, lum_pdf);
            }

            /* ============================================================== */
            /*                         Path termination                       */
            /* ============================================================== */

            /* Possibly terminate the path with Russian roulette, first factoring
               out radiance scaling due to refraction. */
            if (bounces > m_rr_depth) {
                auto q = min(hmax(throughput) * eta * eta, 0.95f);
                // Russian roulette termination
                active = active && (rs.next_1d() < q);

                // Adjust throughput for the surviving paths
                throughput /= q;
                Assert(none(active && any(enoki::isinf(throughput))));
            }
            active = active && any(throughput > 0.0f);

            if (none(active))
                break;  // All paths have terminated
        }

        rs.alpha = select(hit_anything, Value(1.0f), Value(0.0f));
        return Li;
    }

    MTS_IMPLEMENT_INTEGRATOR()

    //! @}
    // =============================================================

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "PathIntegrator[]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()

protected:
};


MTS_IMPLEMENT_CLASS(PathIntegrator, MonteCarloIntegrator);
MTS_EXPORT_PLUGIN(PathIntegrator, "Path Tracer integrator");
NAMESPACE_END(mitsuba)
