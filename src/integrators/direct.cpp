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
        m_weight_bsdf = 1 / (Float) m_bsdf_samples;
        m_weight_lum = 1 / (Float) m_emitter_samples;
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
        using DirectionSample = mitsuba::DirectionSample<Point3>;
        using BSDFSample      = mitsuba::BSDFSample<Point3>;

        const Scene *scene = rs.scene;
        Spectrum result = 0.f;

        auto &si = rs.ray_intersect(ray, active);
        active &= si.is_valid();

        if (none(active))
            return result;

        result += si.emission(active);

        auto bsdf = si.bsdf(ray);

        for (size_t i = 0; i < m_emitter_samples; ++i) {
            DirectionSample ds;
            Spectrum spec;
            std::tie(ds, spec) = scene->sample_emitter_direction(
                si, rs.next_2d(active), true, active);
            Mask valid = active && neq(ds.pdf, 0.0f);

            BSDFSample bs(si, si.to_local(ds.d));

            Spectrum bsdf_val = bsdf->eval(bs, ESolidAngle, valid);
            masked(result, valid) = result + spec * bsdf_val;
        }
        result *= 1.f / m_emitter_samples;

        // TODO: implement MIS (emitter sampling, BSDF sampling)

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
