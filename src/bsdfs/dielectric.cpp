#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/ior.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Smooth dielectric material.
 */
template <typename Float, typename Spectrum>
class SmoothDielectric final : public BSDF<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(SmoothDielectric, BSDF);
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture)

    SmoothDielectric(const Properties &props) : Base(props) {

        // Specifies the internal index of refraction at the interface
        ScalarFloat int_ior = lookup_ior(props, "int_ior", "bk7");

        // Specifies the external index of refraction at the interface
        ScalarFloat ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0 || ext_ior < 0)
            Throw("The interior and exterior indices of refraction must"
                  " be positive!");

        m_eta = int_ior / ext_ior;

        m_specular_reflectance   = props.texture<Texture>("specular_reflectance", 1.f);
        m_specular_transmittance = props.texture<Texture>("specular_transmittance", 1.f);

        m_components.push_back(BSDFFlags::DeltaReflection | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide);
        m_components.push_back(BSDFFlags::DeltaTransmission | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide | BSDFFlags::NonSymmetric);
        m_flags = m_components[0] | m_components[1];
    }

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
        bs.sampled_type      = select(selected_r, UInt32(+BSDFFlags::DeltaReflection),
                                                  UInt32(+BSDFFlags::DeltaTransmission));

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

    Spectrum eval(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
                  const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    Float pdf(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
              const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("eta", m_eta);
        callback->put_object("specular_reflectance", m_specular_reflectance.get());
        callback->put_object("specular_transmittance", m_specular_transmittance.get());
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
    ScalarFloat m_eta;
    ref<Texture> m_specular_reflectance;
    ref<Texture> m_specular_transmittance;
};

MTS_EXPORT_PLUGIN(SmoothDielectric, "Smooth dielectric")
NAMESPACE_END(mitsuba)
