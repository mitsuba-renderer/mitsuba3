NAMESPACE_BEGIN(mitsuba)
/**
 * GTR1_isotropic Microfacet Distribution class
 *
 * This class implements GTR1 Microfacet Distribution Methods
 * for sampling routines of clearcoat lobe in the pricipled BSDF.
 *
 * Based on the paper
 *
 *   "Physically Based Shading at Disney"
 *    by Burley Brent
 *
 * Although it is a Microfacet distribution, it is not added in Microfacet
 * Plugin of Mitsuba since only the Principled BSDF uses it. Also,
 * visible normal sampling procedure is not applied as in Microfacet Plugin
 * because the clearcoat lobe of the principled BSDF has low energy compared to the other
 * lobes and visible normal sampling would not increase the sampling performance
 * considerably.
 */
template <typename Float, typename Spectrum>
class GTR1Isotropic {
public:
    MI_IMPORT_TYPES();
    /**
     * Create an isotropic microfacet distribution for clearcoat lobe
     * based on GTR1.
     *
     * Args:
     *     m_alpha: The roughness of the surface.
     */
    GTR1Isotropic() = default;

    GTR1Isotropic(Float alpha) {
        Float alpha2 = dr::square(alpha);
        m_alpha2_m1  = alpha2 - 1.f;
        m_log_alpha2 = dr::log(alpha2);
        m_norm       = m_alpha2_m1 / (dr::Pi<Float> * m_log_alpha2);
    }

    Float eval(const Vector3f &m) const {
        Float cos_theta = Frame3f::cos_theta(m);

        Float result =
            m_norm / dr::fmadd(m_alpha2_m1, dr::square(cos_theta), 1.f);

        return dr::select(result * cos_theta > 1e-20f, result, 0.f);
    }

    Float pdf(const Vector3f &m) const {
        return dr::select(m.z() < 0.f, 0.f, Frame3f::cos_theta(m) * eval(m));
    }

    Normal3f sample(const Point2f &sample) const {
        auto [sin_phi, cos_phi] = dr::sincos((2.f * dr::Pi<Float>) *sample.x());

        // cos^2(theta) = (1 - alpha2^(1 - u)) / (1 - alpha2)
        Float cos_theta2 =
            (dr::exp(dr::fnmadd(sample.y(), m_log_alpha2, m_log_alpha2)) - 1.f) /
            m_alpha2_m1;

        Float sin_theta = dr::safe_sqrt(1.f - cos_theta2),
              cos_theta = dr::safe_sqrt(cos_theta2);

        return Normal3f(cos_phi * sin_theta, sin_phi * sin_theta, cos_theta);
    }

private:
    /// alpha^2 - 1, log(alpha^2), and the normalization constant
    Float m_alpha2_m1, m_log_alpha2, m_norm;
};

/**
 * Get the flag which determines whether the corresponding
 * feature is going to be implemented or not.
 *
 * Args:
 *     name: Name of the feature.
 *
 *     props: Given properties.
 *
 * Returns:
 *     the flag of the feature.
 */
bool get_flag(const std::string &name, const Properties &props) {
    if (props.has_property(name)) {
        if (props.type(name) == Properties::Type::Float &&
        std::stof(props.as_string(name)) == 0.0f)
            return false;
        else
            return true;
    } else {
        return false;
    }
}

/**
 * Computes the schlick weight for Fresnel Schlick approximation.
 *
 * Args:
 *     cos_i: Incident angle of the ray based on microfacet normal.
 *
 * Returns:
 *     schlick weight
 */
template <typename Float>
Float schlick_weight(Float cos_i) {
    Float m = dr::clip(1.0f - cos_i, 0.0f, 1.0f);
    return dr::square(dr::square(m)) * m;
}

/**
 * Schlick Approximation for Fresnel Reflection coefficient F = R0 +
 * (1-R0) (1-cos^5(i)). Transmitted ray's angle should be used for eta<1.
 *
 * Args:
 *     R0: Incident specular. (Fresnel term when incident ray is aligned with
 *         the surface normal.)
 *
 *     cos_theta_i: Incident angle of the ray based on microfacet normal.
 *
 * Returns:
 *     Schlick approximation result.
 */
template <typename T,typename Float>
T calc_schlick(T R0, Float cos_theta_i,Float eta) {
    dr::mask_t<Float> outside_mask = cos_theta_i >= 0.0f;
    Float rcp_eta = dr::rcp(eta),
    eta_it = dr::select(outside_mask, eta, rcp_eta),
    eta_ti = dr::select(outside_mask, rcp_eta, eta);

    Float cos_theta_t_sqr = dr::fnmadd(
            dr::fnmadd(cos_theta_i, cos_theta_i, 1.0f), dr::square(eta_ti), 1.0f);
    Float cos_theta_t = dr::safe_sqrt(cos_theta_t_sqr);
    return dr::select(
            eta_it > 1.0f,
            dr::lerp(schlick_weight(dr::abs(cos_theta_i)), 1.0f, R0),
            dr::lerp(schlick_weight(cos_theta_t), 1.0f, R0));
}

/**
 * Approximation of incident specular based on relative index of
 * refraction.
 *
 * Args:
 *     eta: Relative index of refraction.
 *
 * Returns:
 *     Incident specular
 */
template <typename Float>
Float schlick_R0_eta(Float eta){
    return dr::square((eta - 1.0f) / (eta + 1.0f));
}

/**
 * Fresnel term of the principled material. It blends the dielectric
 * response with Schlick-based metallic and tinted reflections.
 *
 * Args:
 *     F_dielectric: True dielectric response.
 *
 *     cos_theta_i: Cosine between the incident direction and the
 *         microfacet normal.
 *
 *     cos_theta_t: Absolute cosine of the transmitted direction.
 *
 *     eta_it: Relative index of refraction along the direction of travel.
 *
 *     r0: Schlick reflectance of the dielectric at normal incidence.
 *
 *     metallic: Metallic weight.
 *
 *     spec_tint: Specular tint weight.
 *
 *     base_color: Base color of the material.
 *
 *     c_tint: Base color normalized by its luminance.
 *
 *     front_side: Mask for front side of the macro surface.
 *
 *     bsdf: Weight of the BSDF major lobe.
 *
 * Returns:
 *     Fresnel term of principled BSDF with metallic and dielectric response
 *     combined.
 */
template<typename Float,typename T>
T principled_fresnel(const Float &F_dielectric, const Float &cos_theta_i,
                     const Float &cos_theta_t, const Float &eta_it,
                     const Float &r0, const Float &metallic,
                     const Float &spec_tint, const T &base_color,
                     const T &c_tint, const dr::mask_t<Float> &front_side,
                     const Float &bsdf, bool has_metallic,
                     bool has_spec_tint) {
    Float one_minus_metallic = 1.0f - metallic;
    Float F_front = one_minus_metallic * (1.0f - spec_tint) * F_dielectric;
    T result = F_front;

    if (has_metallic || has_spec_tint) {
        // The Schlick weight uses the transmitted angle when entering a
        // less dense medium
        Float w = dr::select(eta_it > 1.0f,
                             schlick_weight(dr::abs(cos_theta_i)),
                             schlick_weight(cos_theta_t));
        Float weight(0.0f);
        T R0(0.0f);

        if (has_metallic) {
            weight = metallic;
            R0 = metallic * base_color;
        }

        if (has_spec_tint) {
            Float t = one_minus_metallic * spec_tint;
            weight += t;
            R0 = dr::fmadd(c_tint, t * r0, R0);
        }

        // Schlick term with the lobe weights folded in. It blends from R0 at
        // normal incidence to the full weight at grazing angles.
        result = F_front + dr::lerp(R0, weight, w);
    }

    // The back side has no tint or metallic response
    return dr::select(front_side, result, bsdf * F_dielectric);
}

/**
 * Fresnel term of the thin principled material. It blends the dielectric
 * response with a tinted Schlick approximation.
 *
 * Args:
 *     F_dielectric: True dielectric response.
 *
 *     spec_tint: Specular tint weight.
 *
 *     c_tint: Base color normalized by its luminance.
 *
 *     cos_theta_i: Incident angle of the ray based on microfacet normal.
 *
 *     eta_t: Relative index of Refraction of the thin Film
 *
 * Returns:
 *     Fresnel term of the thin BSDF with normal and tinted response
 *     combined.
 */
template<typename Float,typename T>
T thin_fresnel(const Float &F_dielectric, const Float &spec_tint,
               const T &c_tint, const Float &cos_theta_i,
               const Float &eta_t, bool has_spec_tint) {
    T F_schlick(0.0f);
    // Tinted dielectric component based on Schlick.
    if (has_spec_tint) {
        T F0_spec_tint = c_tint * schlick_R0_eta(eta_t);
        F_schlick = calc_schlick<T>(F0_spec_tint, cos_theta_i, eta_t);
    }
    return dr::lerp(F_dielectric, F_schlick, spec_tint);
}

/**
 * Calculates the microfacet distribution parameters based on
 * Disney Course Notes.
 *
 * Args:
 *     anisotropic: Anisotropy weight.
 *
 *     roughness: Roughness parameter of the material.
 *
 * Returns:
 *     Microfacet Distribution roughness parameters: alpha_x, alpha_y.
 */
template<typename Float>
std::pair<Float, Float> calc_dist_params(Float anisotropic,
                                         Float roughness,
                                         bool has_anisotropic){
    Float roughness_2 = dr::square(roughness);
    if (!has_anisotropic) {
        Float a = dr::maximum(0.001f, roughness_2);
        return { a, a };
    }
    Float aspect = dr::safe_sqrt(dr::fnmadd(0.9f, anisotropic, 1.0f));
    return { dr::maximum(0.001f, roughness_2 / aspect),
             dr::maximum(0.001f, roughness_2 * aspect) };
}
NAMESPACE_END(mitsuba)
