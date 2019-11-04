#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class ThinDielectric final : public BSDF<Float, Spectrum> {
public:
    MTS_REGISTER_CLASS(ThinDielectric, BSDF);
    MTS_USING_BASE(BSDF, Base, m_flags, m_components)
    MTS_IMPORT_TYPES(ContinuousSpectrum)

    ThinDielectric(const Properties &props) : Base(props) {
        // Specifies the internal index of refraction at the interface
        ScalarFloat int_ior = lookup_ior(props, "int_ior", "bk7");
        // Specifies the external index of refraction at the interface
        ScalarFloat ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.f || ext_ior < 0.f)
            Throw("The interior and exterior indices of "
                  "refraction must be positive!");

        m_eta = int_ior / ext_ior;

        m_specular_reflectance   = props.spectrum<Float, Spectrum>("specular_reflectance", 1.f);
        m_specular_transmittance = props.spectrum<Float, Spectrum>("specular_transmittance", 1.f);

        m_components.push_back(BSDFFlags::DeltaReflection | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide);
        m_components.push_back(BSDFFlags::Null | BSDFFlags::FrontSide | BSDFFlags::BackSide);
        m_flags = m_components[0] | m_components[1];
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                           Float sample1, const Point2f & /*sample2*/,
                                           Mask active) const override {

        bool has_reflection   = ctx.is_enabled(BSDFFlags::DeltaReflection, 0),
             has_transmission = ctx.is_enabled(BSDFFlags::Null, 1);

        Float r = std::get<0>(fresnel(abs(Frame3f::cos_theta(si.wi)), Float(m_eta)));

        // Account for internal reflections: r' = r + trt + tr^3t + ..
        r *= 2.f / (1.f + r);

        Float t = 1.f - r;

        // Select the lobe to be sampled
        BSDFSample3f bs;
        Spectrum weight;
        Mask selected_r;
        if (likely(has_reflection && has_transmission)) {
            selected_r = sample1 <= r && active;
            weight = 1.f;
            bs.pdf = select(selected_r, r, t);
        } else {
            if (has_reflection || has_transmission) {
                selected_r = Mask(has_reflection) && active;
                weight = has_reflection ? r : t;
                bs.pdf = 1.f;
            } else {
                return { bs, 0.f };
            }
        }

        bs.wo = select(selected_r, reflect(si.wi), -si.wi);
        bs.eta = 1.f;
        bs.sampled_component = select(selected_r, UInt32(0), UInt32(1));
        bs.sampled_type      = select(selected_r, +BSDFFlags::DeltaReflection, +BSDFFlags::Null);

        if (any_or<true>(selected_r))
            weight[selected_r] *= m_specular_reflectance->eval(si, selected_r);

        Mask selected_t = !selected_r && active;
        if (any_or<true>(selected_t))
            weight[selected_t] *= m_specular_transmittance->eval(si, selected_t);

        return { bs, select(active, weight, 0.f) };
    }

    Spectrum eval(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
                  const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    Float pdf(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
              const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "ThinDielectric[" << std::endl
            << "  eta = "                    << string::indent(m_eta)                    << "," << std::endl
            << "  specular_reflectance = "   << string::indent(m_specular_reflectance)   << "," << std::endl
            << "  specular_transmittance = " << string::indent(m_specular_transmittance) << std::endl
            << "]";
        return oss.str();
    }

private:
    ScalarFloat m_eta;
    ref<ContinuousSpectrum> m_specular_transmittance;
    ref<ContinuousSpectrum> m_specular_reflectance;
};

MTS_IMPLEMENT_PLUGIN(ThinDielectric, BSDF, "Thin dielectric")
NAMESPACE_END(mitsuba)
