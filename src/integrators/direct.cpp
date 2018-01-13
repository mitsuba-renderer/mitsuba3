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

        /// Be strict about potential inconsistencies involving shading normals?
        m_strict_normals = props.bool_("strict_normals", false);

        if (m_emitter_samples + m_bsdf_samples == 0)
            Throw("Must have at least 1 BSDF or emitter sample!");

        size_t sum = m_emitter_samples + m_bsdf_samples;
        m_weight_bsdf = 1.0f / (Float) m_bsdf_samples;
        m_weight_lum = 1.0f / (Float) m_emitter_samples;
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
        using BSDFSample         = mitsuba::BSDFSample<Point3>;

        const Scene *scene = rs.scene;
        Spectrum result(0.0f);

        // Direct intersection
        auto &si = rs.ray_intersect(ray, active);
        active = active && si.is_valid();

        if (none(active))
            return result;

        result += si.emission(active);

        auto bsdf = si.bsdf(ray);

        // ---------------------------------------------------- Emitter sampling
        Spectrum result_partial = 0.0f;
        for (size_t i = 0; i < m_emitter_samples; ++i) {
            DirectionSample ds;
            Spectrum emitter_val;
            std::tie(ds, emitter_val) = scene->sample_emitter_direction(
                si, rs.next_2d(active), true, active);
            Mask valid = active && neq(ds.pdf, 0.0f);

            // Query the BSDF for that emitter-sampled direction
            BSDFSample bs(si, si.to_local(ds.d));
            Spectrum bsdf_val = bsdf->eval(bs, ESolidAngle, valid);

            /* Weight using the probability of sampling that direction through
               BSDF sampling. */
            Value bsdf_pdf = bsdf->pdf(bs, ESolidAngle, valid);

            masked(result_partial, valid) =
                result_partial + emitter_val * bsdf_val
                                 * mi_weight(ds.pdf, bsdf_pdf);
        }
        if (m_emitter_samples > 0)
            result += result_partial / m_emitter_samples;

        // ------------------------------------------------------- BSDF sampling
        result_partial = 0.0f;
        for (size_t i = 0; i < m_bsdf_samples; ++i) {
            BSDFSample bs(si, rs.sampler, ERadiance);

            Spectrum bsdf_val;
            Value bsdf_pdf;
            std::tie(bsdf_val, bsdf_pdf) = bsdf->sample(bs, rs.next_2d(), active);
            Mask valid = active && neq(bsdf_pdf, 0.0f);

            // Trace the ray in the sampled direction and intersect against the scene
            RayDifferential bdsf_ray(si.p, si.to_world(bs.wo), si.time, ray.wavelengths);
            SurfaceInteraction si_bsdf = scene->ray_intersect(bdsf_ray, active);

            // Retain only rays that hit an emitter
            valid = valid && si_bsdf.is_valid() && si_bsdf.is_emitter();
            if (any(valid)) {
                Spectrum emitter_val = si_bsdf.emission(valid);
                // TODO: support environment lights

                /* Weight using the probability of sampling that direction through
                   emitter sampling. */
                DirectionSample ds(si_bsdf, si);
                ds.object = si_bsdf.shape->emitter();
                ds.delta = neq(bs.sampled_type & BSDF::EDelta, 0u);

                Value emitter_pdf = select(
                    neq(bs.sampled_type & BSDF::EDelta, 0u),
                    Value(0.0f),
                    scene->pdf_emitter_direction(si_bsdf, ds, valid)
                );

                masked(result_partial, valid) =
                    result_partial + bsdf_val * emitter_val
                                     * mi_weight(bsdf_pdf, emitter_pdf);
            }
        }
        if (m_bsdf_samples > 0)
            result += result_partial / m_bsdf_samples;

        return result;
    }

    template <typename Value>
    Value mi_weight(Value pdf_a, Value pdf_b) const {
        pdf_a *= pdf_a;
        pdf_b *= pdf_b;
        return pdf_a / (pdf_a + pdf_b);
    }

    std::string to_string() const override {
        using string::indent;

        std::ostringstream oss;
        oss << "MIDirectIntegrator[" << std::endl
            << "  emitter_samples = " << m_emitter_samples << "," << std::endl
            << "  bsdf_samples = " << m_bsdf_samples << "," << std::endl
            << "  strict_normals = " << m_strict_normals << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_INTEGRATOR()
    MTS_DECLARE_CLASS()

private:
    size_t m_emitter_samples;
    size_t m_bsdf_samples;
    Float m_frac_bsdf, m_frac_lum;
    Float m_weight_bsdf, m_weight_lum;
    bool m_strict_normals;
};


MTS_IMPLEMENT_CLASS(MIDirectIntegrator, SamplingIntegrator);
MTS_EXPORT_PLUGIN(MIDirectIntegrator, "Direct integrator");
NAMESPACE_END(mitsuba)
