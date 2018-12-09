#include <random>
#include <enoki/stl.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

class PathIntegrator : public MonteCarloIntegrator {
public:
    PathIntegrator(const Properties &props) : MonteCarloIntegrator(props) { }

    template <typename RayDifferential,
              typename Point3 = typename RayDifferential::Point,
              typename RadianceSample = RadianceSample<Point3>,
              typename Value = typename RayDifferential::Value>
    auto eval_impl(RayDifferential ray, RadianceSample &rs,
                   mask_t<Value> active) const {
        using Mask                = mask_t<Value>;
        using Spectrum            = mitsuba::Spectrum<Value>;
        using DirectionSample     = mitsuba::DirectionSample<Point3>;
        using SurfaceInteraction3 = SurfaceInteraction<Point3>;
        using BSDFPtr             = replace_scalar_t<Value, const BSDF *>;
        using EmitterPtr          = replace_scalar_t<Value, const Emitter *>;
        using Vector3             = vector3_t<Point3>;

        const Scene *scene = rs.scene;

        /* Tracks radiance scaling due to index of refraction changes */
        Value eta(1.f);

        /* MIS weight for intersected emitters (set by prev. iteration) */
        Value emission_weight(1.f);

        Spectrum throughput(1.f), result(0.f);

        /* ---------------------- First intersection ---------------------- */

        SurfaceInteraction3 si = rs.ray_intersect(ray, active);
        rs.alpha = select(si.is_valid(), Value(1.f), Value(0.f));
        EmitterPtr emitter = si.emitter(scene);

        for (int depth = 0;; ++depth) {

            /* ---------------- Intersection with emitters ---------------- */

            if (any(neq(emitter, nullptr)))
                result[active] += emission_weight * throughput * emitter->eval(si, active);

            active &= si.is_valid();

            /* Russian roulette: try to keep path weights equal to one,
               while accounting for the solid angle compression at refractive
               index boundaries. Stop with at least some probability to avoid
               getting stuck (e.g. due to total internal reflection) */
            if (depth > m_rr_depth) {
                Value q = min(hmax(throughput) * sqr(eta), .95f);
                active &= rs.next_1d(active) < q;
                throughput *= rcp(q);
            }

            if (none(active) || (uint32_t) depth >= (uint32_t) m_max_depth)
                break;

            /* --------------------- Emitter sampling --------------------- */

            BSDFContext ctx;
            BSDFPtr bsdf = si.bsdf(ray);
            Mask active_e = active && neq(bsdf->flags() & BSDF::ESmooth, 0u);

            if (likely(any(active_e))) {
                auto [ds, emitter_val] = scene->sample_emitter_direction(
                    si, rs.next_2d(active_e), true, active_e);
                active_e &= neq(ds.pdf, 0.f);

                /* Query the BSDF for that emitter-sampled direction */
                Vector3 wo = si.to_local(ds.d);
                Spectrum bsdf_val = bsdf->eval(ctx, si, wo, active_e);

                /* Determine probability of having sampled that same
                   direction using BSDF sampling. */
                Value bsdf_pdf = bsdf->pdf(ctx, si, wo, active_e);

                result[active_e] += throughput *
                    emitter_val * bsdf_val * mis_weight(ds.pdf, bsdf_pdf);
            }

            /* ----------------------- BSDF sampling ---------------------- */

            /* Sample BSDF * cos(theta) */
            auto [bs, bsdf_val] = bsdf->sample(ctx, si, rs.next_1d(active),
                                               rs.next_2d(active), active);
            throughput *= bsdf_val;
            active &= any(neq(throughput, 0.f));
            if (none(active))
                break;

            eta *= bs.eta;

            /* Intersect the BSDF ray against the scene geometry */
            ray = si.spawn_ray(si.to_world(bs.wo));
            SurfaceInteraction3 si_bsdf = scene->ray_intersect(ray, active);

            /* Determine probability of having sampled that same
               direction using emitter sampling. */
            emitter = si_bsdf.emitter(scene, active);
            DirectionSample ds(si_bsdf, si);
            ds.object = emitter;

            if (any(neq(emitter, nullptr))) {
                Value emitter_pdf =
                    select(neq(bs.sampled_type & BSDF::EDelta, 0u), 0.f,
                           scene->pdf_emitter_direction(si, ds, active));

                emission_weight = mis_weight(bs.pdf, emitter_pdf);
            }

            si = si_bsdf;
        }

        return result;
    }

    //! @}
    // =============================================================

    std::string to_string() const override {
        return tfm::format("PathIntegrator[\n"
            "  max_depth = %i,\n"
            "  rr_depth = %i\n"
            "]", m_max_depth, m_rr_depth);
    }

    MTS_IMPLEMENT_INTEGRATOR()
    MTS_DECLARE_CLASS()

protected:
    template <typename Value> Value mis_weight(Value pdf_a, Value pdf_b) const {
        pdf_a *= pdf_a;
        pdf_b *= pdf_b;
        return select(pdf_a > 0.0f, pdf_a / (pdf_a + pdf_b), Value(0.0f));
    };
};


MTS_IMPLEMENT_CLASS(PathIntegrator, MonteCarloIntegrator);
MTS_EXPORT_PLUGIN(PathIntegrator, "Path Tracer integrator");
NAMESPACE_END(mitsuba)
