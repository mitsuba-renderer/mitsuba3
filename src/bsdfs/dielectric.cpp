#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

namespace {
// TODO: move to some utils.h
template <typename Value>
std::pair<Value, Value> fresnel_dielectric_ext(Value cos_theta_i_, Float eta) {
    if (unlikely(eta == 1.0f))
        return { Value(0.0f), -cos_theta_i_ };

    /* Using Snell's law, calculate the squared sine of the angle between the
       normal and the transmitted ray */
    Value scale = select(cos_theta_i_ > 0.0f, Value(rcp(eta)), Value(eta)),
          cos_theta_t_sqr = 1.0f - (1.0f - cos_theta_i_ * cos_theta_i_)
                                   * (scale * scale);

    // Check for total internal reflection.
    mask_t<Value> total_reflection = cos_theta_t_sqr <= 0.0f;
    if (all(total_reflection))
        return { Value(1.0f), Value(0.0f) };

    // Find the absolute cosines of the incident/transmitted rays.
    Value cos_theta_i = abs(cos_theta_i_),
          cos_theta_t = sqrt(cos_theta_t_sqr);

    Value Rs = (cos_theta_i - eta * cos_theta_t)
               / (cos_theta_i + eta * cos_theta_t),
          Rp = (eta * cos_theta_i - cos_theta_t)
               / (eta * cos_theta_i + cos_theta_t);

    return {
        select(total_reflection,
            Value(1.0f),
            // No polarization -- return the unpolarized reflectance.
            0.5f * (Rs * Rs + Rp * Rp)),
        select(total_reflection,
            Value(0.0f),
            select(cos_theta_i_ > 0.0f, -cos_theta_t, cos_theta_t))
    };
}

}  // namespace

/**
 * Smooth dielectric material.
 */
class SmoothDielectric final : public BSDF {
public:
    SmoothDielectric(const Properties &props) {
        m_flags = EDeltaReflection | EFrontSide | EBackSide
                  | EDeltaTransmission | EFrontSide | EBackSide | ENonSymmetric;

        // Specifies the internal index of refraction at the interface
        Float int_ior = lookup_ior(props, "int_ior", "bk7");

        // Specifies the external index of refraction at the interface
        Float ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0 || ext_ior < 0)
            Log(EError, "The interior and exterior indices of "
                "refraction must be positive!");

        m_eta = int_ior / ext_ior;
        m_inv_eta = rcp(m_eta);

        m_specular_reflectance = props.spectrum("specular_reflectance", .5f);
        m_specular_transmittance = props.spectrum("specular_transmittance", .5f);

        // Verify the input parameters and fix them if necessary
        // TODO: when replacing this with textures, implement `ensure_energy_conservation`.
        // if (any((m_specular_reflectance > 1.0f) || (m_specular_transmittance > 1.0f)))
        //     Throw("Energy conservation not satisfied, reflectance and "
        //           "transmittance must be <= 1");

        // m_components.clear();
        // m_components.push_back(EDeltaReflection | EFrontSide | EBackSide
        //     | (m_specular_reflectance->isConstant() ? 0 : ESpatiallyVarying));
        // m_components.push_back(EDeltaTransmission | EFrontSide | EBackSide | ENonSymmetric
        //     | (m_specular_transmittance->isConstant() ? 0 : ESpatiallyVarying));

        m_needs_differentials = false;
            // m_specular_reflectance->uses_ray_differentials() ||
            // m_specular_transmittance->uses_ray_differentials();
    }

    /// Reflection in local coordinates
    template <typename Vector3>
    Vector3 reflect(const Vector3 &wi) const {
        return Vector3(-wi.x(), -wi.y(), wi.z());
    }

    /// Refraction in local coordinates
    template <typename Vector3, typename Value = value_t<Float>>
    Vector3 refract(const Vector3 &wi, const Value &cos_theta_t) const {
        Value scale = select(cos_theta_t < 0.0f, Value(-m_inv_eta), Value(-m_eta));
        return Vector3(scale * wi.x(), scale * wi.y(), cos_theta_t);
    }

    template <typename BSDFSample,
              typename Value  = typename BSDFSample::Value,
              typename Point2 = typename BSDFSample::Point2,
              typename Spectrum = Spectrum<Value>>
    std::pair<Spectrum, Value> sample_impl(BSDFSample &bs, const Point2 &sample,
                                           const mask_t<Value> &/*active_*/) const {
        using Index   = typename BSDFSample::Index;
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        Mask sample_reflection   = neq(bs.type_mask & EDeltaReflection, 0u)
                                   && (bs.component == -1 || bs.component == 0);
        Mask sample_transmission = neq(bs.type_mask & EDeltaTransmission, 0u)
                                   && (bs.component == -1 || bs.component == 1);

        if (none(sample_reflection || sample_transmission))
            return { Spectrum(0.0f), 1.0f };


        Value cos_theta_i = Frame::cos_theta(bs.wi);
        Value F, cos_theta_t;
        std::tie(F, cos_theta_t) = fresnel_dielectric_ext(cos_theta_i, m_eta);

        Value threshold = select(sample_transmission && sample_reflection, F, 1.0f);
        Mask selected_reflection = sample_reflection && (sample.x() <= threshold);
        bs.sampled_component = select(selected_reflection, Index(0), Index(1));
        bs.sampled_type      = select(selected_reflection,
                                      Index(EDeltaReflection),
                                      Index(EDeltaTransmission));
        bs.wo                = select(selected_reflection,
                                      reflect(bs.wi), refract(bs.wi, cos_theta_t));
        bs.eta               = select(selected_reflection,
                                      Value(1.0f),
                                      select(cos_theta_t < 0,
                                             Value(m_eta), Value(m_inv_eta))
                               );

        Value pdf = 1.0f;
        masked(pdf, sample_transmission &&  selected_reflection) = F;
        masked(pdf, sample_reflection   && !selected_reflection) = 1.0f - F;

        /* For transmission, radiance must be scaled to account for the solid
           angle compression that occurs when crossing the interface. */
        Value factor = (bs.mode == ERadiance)
            ? select(cos_theta_t < 0.0f, Value(m_inv_eta), Value(m_eta)) : 1.0f;

        Spectrum radiance = pdf * select(selected_reflection,
            m_specular_reflectance->eval(bs.si.wavelengths),
            (factor * factor) * m_specular_transmittance->eval(bs.si.wavelengths)
        );

        return { radiance, pdf };
    }

    template <typename BSDFSample,
              typename Value = typename BSDFSample::Value,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFSample &bs, EMeasure measure,
                       const mask_t<Value> &active_) const {
        using Frame   = Frame<typename BSDFSample::Vector3>;
        using Mask    = mask_t<Value>;

        Mask sample_reflection   = neq(bs.type_mask & EDeltaReflection, 0u)
                                   && (bs.component == -1 || bs.component == 0)
                                   && measure == EDiscrete;
        Mask sample_transmission = neq(bs.type_mask & EDeltaTransmission, 0u)
                                   && (bs.component == -1 || bs.component == 1)
                                   && measure == EDiscrete;

        Value cos_theta_i = Frame::cos_theta(bs.wi);
        Value F, cos_theta_t;
        std::tie(F, cos_theta_t) = fresnel_dielectric_ext(cos_theta_i, m_eta);

        Mask is_reflection = cos_theta_i * Frame::cos_theta(bs.wo) >= 0;
        Mask has_reflection = sample_reflection
            && (abs(dot(reflect(bs.wi), bs.wo) - 1.0f) > math::DeltaEpsilon);
        Mask has_transmission = sample_transmission
            && (abs(dot(refract(bs.wi, cos_theta_t), bs.wo) - 1.0f) > math::DeltaEpsilon);
        Mask active = active_ && ((is_reflection && has_reflection)
                                  || (!is_reflection && has_transmission));

        Spectrum result = 1.0f;
        masked(result, !active) = Spectrum(0.0f);
        masked(result, active && is_reflection && sample_transmission) =
            m_specular_reflectance->eval(bs.si.wavelengths) * F;
        /* Radiance must be scaled to account for the solid angle compression
           that occurs when crossing the interface. */
        Value factor = bs.mode == ERadiance
            ? select(cos_theta_t < 0.0f, Value(m_inv_eta), Value(m_eta)) : 1.0f;
        masked(result, active && !is_reflection && sample_reflection) =
            m_specular_transmittance->eval(bs.si.wavelengths)
            * factor * factor * (1.0f - F);

        return result;
    }

    template <typename BSDFSample, typename Value = typename BSDFSample::Value>
    Value pdf_impl(const BSDFSample &bs, EMeasure measure,
                   const mask_t<Value> &active_) const {
        using Frame   = Frame<typename BSDFSample::Vector3>;
        using Mask    = mask_t<Value>;

        Mask sample_reflection   = neq(bs.type_mask & EDeltaReflection, 0u)
                                   && (bs.component == -1 || bs.component == 0)
                                   && measure == EDiscrete;
        Mask sample_transmission = neq(bs.type_mask & EDeltaTransmission, 0u)
                                   && (bs.component == -1 || bs.component == 1)
                                   && measure == EDiscrete;

        Value cos_theta_i = Frame::cos_theta(bs.wi);
        Value F, cos_theta_t;
        std::tie(F, cos_theta_t) = fresnel_dielectric_ext(cos_theta_i, m_eta);

        Mask is_reflection = cos_theta_i * Frame::cos_theta(bs.wo) >= 0;
        Mask has_reflection = sample_reflection
            && (abs(dot(reflect(bs.wi), bs.wo) - 1.0f) > math::DeltaEpsilon);
        Mask has_transmission = sample_transmission
            && (abs(dot(refract(bs.wi, cos_theta_t), bs.wo) - 1.0f) > math::DeltaEpsilon);
        Mask active = active_ && ((is_reflection && has_reflection)
                                  || (!is_reflection && has_transmission));

        Value result = 1.0f;
        masked(result, !active) = 0.0f;
        masked(result, active &&  is_reflection && sample_transmission) = F;
        masked(result, active && !is_reflection && sample_reflection) = 1.0f - F;

        return result;
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

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()
private:
    Float m_eta, m_inv_eta;
    ref<ContinuousSpectrum> m_specular_reflectance, m_specular_transmittance;
};

MTS_IMPLEMENT_CLASS(SmoothDielectric, BSDF)
MTS_EXPORT_PLUGIN(SmoothDielectric, "Smooth dielectric BSDF")

NAMESPACE_END(mitsuba)
