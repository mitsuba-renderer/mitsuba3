#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/ior.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Smooth dielectric material.
 */
template <typename Float, typename Spectrum>
class SmoothDielectric final : public BSDF<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN();
    MTS_USING_BASE(BSDF, Base, m_flags, m_components)
    using ContinuousSpectrum = typename Aliases::ContinuousSpectrum;

    SmoothDielectric(const Properties &props) : Base(props) {

        // Specifies the internal index of refraction at the interface
        sFloat int_ior = lookup_ior(props, "int_ior", "bk7");

        // Specifies the external index of refraction at the interface
        sFloat ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0 || ext_ior < 0)
            Throw("The interior and exterior indices of refraction must"
                  " be positive!");

        m_eta = int_ior / ext_ior;

        m_specular_reflectance   = props.spectrum<Float, Spectrum>("specular_reflectance", 1.f);
        m_specular_transmittance = props.spectrum<Float, Spectrum>("specular_transmittance", 1.f);

        m_components.push_back(BSDFFlags::DeltaReflection | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide);
        m_components.push_back(BSDFFlags::DeltaTransmission | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide | BSDFFlags::NonSymmetric);
        m_flags = m_components[0] | m_components[1];
    }

    MTS_INLINE
    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                           Float sample1, const Point2f & /*sample2*/,
                                           Mask active) const override {
        bool has_reflection   = ctx.is_enabled(BSDFFlags::DeltaReflection, 0),
             has_transmission = ctx.is_enabled(BSDFFlags::DeltaTransmission, 1);

        // Evaluate the Fresnel equations for unpolarized illumination
        Float cos_theta_i = Frame3f::cos_theta(si.wi);

        auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));
        Float t_i = 1.f - r_i;

        // Lobe selection
        BSDFSample3f bs;
        Spectrum weight;
        Mask selected_r;
        if (likely(has_reflection && has_transmission)) {
            selected_r = sample1 <= r_i && active;
            weight = 1.f;
            bs.pdf = select(selected_r, r_i, t_i);
        } else {
            if (has_reflection || has_transmission) {
                selected_r = Mask(has_reflection) && active;
                weight = has_reflection ? r_i : t_i;
                bs.pdf = 1.f;
            } else {
                return { bs, 0.f };
            }
        }

        bs.sampled_component = select(selected_r, UInt32(0), UInt32(1));
        bs.sampled_type =
            select(selected_r, +BSDFFlags::DeltaReflection, +BSDFFlags::DeltaTransmission);

        bs.wo = select(selected_r,
                       reflect(si.wi),
                       refract(si.wi, cos_theta_t, eta_ti));

        bs.eta = select(selected_r, Float(1.f), eta_it);

        if (any_or<true>(selected_r))
            weight[selected_r] *=
                m_specular_reflectance->eval(si, selected_r);

        Mask selected_t = !selected_r && active;
        if (any_or<true>(selected_t)) {
            /* For transmission, radiance must be scaled to account for the solid
               angle compression that occurs when crossing the interface. */
            Float factor = (ctx.mode == TransportMode::Radiance) ? eta_ti : Float(1.f);

            weight[selected_t] *=
                m_specular_transmittance->eval(si, selected_t) * sqr(factor);
        }

        return { bs, select(active, weight, 0.f) };
    }

    MTS_INLINE
    Spectrum eval(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
                  const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    MTS_INLINE
    Float pdf(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
              const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    std::vector<ref<Object>> children() override {
        return { m_specular_reflectance.get(), m_specular_transmittance.get() };
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothDielectric[" << std::endl
            << "  eta = " << m_eta << "," << std::endl
            << "  specular_reflectance = " << string::indent(m_specular_reflectance) << "," << std::endl
            << "  specular_transmittance = " << string::indent(m_specular_transmittance) << std::endl
            << "]";
        return oss.str();
    }

private:
    sFloat m_eta;
    ref<ContinuousSpectrum> m_specular_reflectance;
    ref<ContinuousSpectrum> m_specular_transmittance;
};

MTS_IMPLEMENT_PLUGIN(SmoothDielectric, BSDF, "Smooth dielectric")
NAMESPACE_END(mitsuba)
