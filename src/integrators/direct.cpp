#include <enoki/stl.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

class MIDirectIntegrator : public SamplingIntegrator {
public:
    // =============================================================
    //! @{ \name Constructors
    // =============================================================
    MIDirectIntegrator(const Properties &props) : SamplingIntegrator(props) {
        if (props.has_property("shading_samples")
            && (props.has_property("emitter_samples") ||
                props.has_property("bsdf_samples"))) {
            Throw("Cannot specify both 'shading_samples' and"
                  " ('emitter_samples' and/or 'bsdf_samples').");
        }

        /// Number of shading samples -- this parameter is a shorthand notation
        /// to set both 'emitter_samples' and 'bsdf_samples' at the same time
        size_t shading_samples = props.size_("shading_samples", 1);

        /// Number of samples to take using the emitter sampling technique
        m_emitter_samples = props.size_("emitter_samples", shading_samples);

        /// Number of samples to take using the BSDF sampling technique
        m_bsdf_samples = props.size_("bsdf_samples", shading_samples);

        if (m_emitter_samples + m_bsdf_samples == 0)
            Throw("Must have at least 1 BSDF or emitter sample!");

        size_t sum = m_emitter_samples + m_bsdf_samples;
        m_weight_bsdf = 1.f / (Float) m_bsdf_samples;
        m_weight_lum = 1.f / (Float) m_emitter_samples;
        m_frac_bsdf = m_bsdf_samples / (Float) sum;
        m_frac_lum = m_emitter_samples / (Float) sum;
    }

    template <typename RayDifferential,
              typename Value           = typename RayDifferential::Value,
              typename Point3          = typename RayDifferential::Point,
              typename Spectrum        = mitsuba::Spectrum<Value>,
              typename Mask            = mask_t<Value>>
    Spectrum eval_impl(const RayDifferential &ray, RadianceSample<Point3> &rs,
                       Mask active) const {
        using DirectionSample    = mitsuba::DirectionSample<Point3>;
        using SurfaceInteraction = mitsuba::SurfaceInteraction<Point3>;
        using Vector3            = typename SurfaceInteraction::Vector3;
        using EmitterPtr         = typename SurfaceInteraction::EmitterPtr;
        using BSDFPtr            = typename SurfaceInteraction::BSDFPtr;

        const Scene *scene = rs.scene;

        SurfaceInteraction &si = rs.ray_intersect(ray, active);
        rs.alpha = select(si.is_valid(), Value(1.f), Value(0.f));

        Spectrum result(0.f);

        /* ----------------------- Visible emitters ----------------------- */

        EmitterPtr emitter_vis = si.emitter(scene, active);
        if (any(neq(emitter_vis, nullptr)))
            result += emitter_vis->eval(si, active);

        active &= si.is_valid();
        if (none(active))
            return result;

        /* ----------------------- Emitter sampling ----------------------- */

        BSDFContext ctx;
        BSDFPtr bsdf = si.bsdf(ray);
        Mask sample_emitter = active && neq(bsdf->flags() & BSDF::ESmooth, 0u);

        if (any(sample_emitter)) {
            for (size_t i = 0; i < m_emitter_samples; ++i) {
                Mask active_e = sample_emitter;
                DirectionSample ds;
                Spectrum emitter_val;
                std::tie(ds, emitter_val) = scene->sample_emitter_direction(
                    si, rs.next_2d(active_e), true, active_e);
                active_e &= neq(ds.pdf, 0.f);

                /* Query the BSDF for that emitter-sampled direction */
                Vector3 wo = si.to_local(ds.d);
                Spectrum bsdf_val = bsdf->eval(ctx, si, wo, active_e);

                /* Determine probability of having sampled that same
                   direction using BSDF sampling. */
                Value bsdf_pdf = bsdf->pdf(ctx, si, wo, active_e);

                result[active_e] +=
                    emitter_val * bsdf_val *
                    mis_weight(ds.pdf * m_frac_lum, bsdf_pdf * m_frac_bsdf) *
                    m_weight_lum;
            }
        }

        /* ------------------------ BSDF sampling ------------------------- */

        for (size_t i = 0; i < m_bsdf_samples; ++i) {
            auto [bs, bsdf_val] = bsdf->sample(ctx, si, rs.next_1d(active),
                                               rs.next_2d(active), active);

            Mask active_b = active && any(neq(bsdf_val, 0.f));

            // Trace the ray in the sampled direction and intersect against the scene
            SurfaceInteraction si_bsdf =
                scene->ray_intersect(si.spawn_ray(si.to_world(bs.wo)), active_b);

            // Retain only rays that hit an emitter
            EmitterPtr emitter = si_bsdf.emitter(scene, active_b);
            active_b &= neq(emitter, nullptr);

            if (any(active_b)) {
                Spectrum emitter_val = emitter->eval(si_bsdf, active_b);
                Mask delta = neq(bs.sampled_type & BSDF::EDelta, 0u);

                /* Determine probability of having sampled that same
                   direction using Emitter sampling. */
                DirectionSample ds(si_bsdf, si);
                ds.object = emitter;

                Value emitter_pdf = select(delta, 0.f,
                    scene->pdf_emitter_direction(si, ds, active_b));

                result[active_b] +=
                    bsdf_val * emitter_val *
                    mis_weight(bs.pdf * m_frac_bsdf, emitter_pdf * m_frac_lum) *
                    m_weight_bsdf;
            }
        }

        return result;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "MIDirectIntegrator[" << std::endl
            << "  emitter_samples = " << m_emitter_samples << "," << std::endl
            << "  bsdf_samples = " << m_bsdf_samples << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_INTEGRATOR()
    MTS_DECLARE_CLASS()

protected:
    template <typename Value> Value mis_weight(Value pdf_a, Value pdf_b) const {
        pdf_a *= pdf_a;
        pdf_b *= pdf_b;
        return select(pdf_a > 0.f, pdf_a / (pdf_a + pdf_b), Value(0.f));
    }

private:
    size_t m_emitter_samples;
    size_t m_bsdf_samples;
    Float m_frac_bsdf, m_frac_lum;
    Float m_weight_bsdf, m_weight_lum;
};


MTS_IMPLEMENT_CLASS(MIDirectIntegrator, SamplingIntegrator);
MTS_EXPORT_PLUGIN(MIDirectIntegrator, "Direct integrator");
NAMESPACE_END(mitsuba)
