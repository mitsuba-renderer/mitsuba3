NAMESPACE_BEGIN(mitsuba)
/**
 * \brief GTR1_isotropic Microfacet Distribution class
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
     * \param m_alpha
     *     The roughness of the surface.
     */
    GTR1Isotropic(Float alpha) : m_alpha(alpha){};

    Float eval(const Vector3f &m) const {
        Float cos_theta  = Frame3f::cos_theta(m),
        cos_theta2 = dr::square(cos_theta), alpha2 = dr::square(m_alpha);

        Float result = (alpha2 - 1.f) / (dr::Pi<Float> * dr::log(alpha2) *
                (1.f + (alpha2 - 1.f) * cos_theta2));

        return dr::select(result * cos_theta > 1e-20f, result, 0.f);
    }

    Float pdf(const Vector3f &m) const {
        return dr::select(m.z() < 0.f, 0.f, Frame3f::cos_theta(m) * eval(m));
    }

    Normal3f sample(const Point2f &sample) const {
        auto [sin_phi, cos_phi] = dr::sincos((2.f * dr::Pi<Float>) *sample.x());
        Float alpha2            = dr::square(m_alpha);

        Float cos_theta2 =
                (1.f - dr::pow(alpha2, 1.f - sample.y())) / (1.f - alpha2);

        Float sin_theta = dr::sqrt(dr::maximum(0.f, 1.f - cos_theta2)),
        cos_theta = dr::sqrt(dr::maximum(0.f, cos_theta2));

        return Normal3f(cos_phi * sin_theta, sin_phi * sin_theta, cos_theta);
    }

private:
    Float m_alpha;
};

/**
 * \brief Separable shadowing-masking for GGX. Mitsuba does not have a GGX1
 * support in microfacet so it is added in principled material plugin.
 * \param wi
 *     Incident Direction.
 * \param wo
 *     Outgoing direction.
 * \param wh
 *     Halfway vector.
 * \param alpha
 *     Roughness of the clearcoat lobe.
 * \return Shadowing-Masking term for GGX. Used in clearcoat lobe.
 */
template<typename Float>
Float clearcoat_G(const Vector<Float,3> &wi, const Vector<Float,3> &wo,
                  const Vector<Float,3> &wh, const Float &alpha) {
    return smith_ggx1(wi, wh, alpha) * smith_ggx1(wo, wh, alpha);
}

/**
 * \brief Calculates Smith ggx shadowing-masking function. Used in
 * separable masking-shadowing term calculation.
 * \param v
 *     Direction for the calculation of the function.
 * \param wh
 *     Halfway vector.
 * \param alpha
 *     Roughness of the clearcoat lobe.
 * \return Smith ggx1 shadowing-masking function.
 */
template<typename Float>
Float smith_ggx1(const Vector<Float,3> &v, const Vector<Float,3> &wh,
                 const Float &alpha) {
    using Frame3f     = Frame<Float>;
    Float alpha_2     = dr::square(alpha),
    cos_theta   = dr::abs(Frame3f::cos_theta(v)),
    cos_theta_2 = dr::square(cos_theta),
    tan_theta_2 = (1.0f - cos_theta_2) / cos_theta_2;

    Float result =
            2.0f * dr::rcp(1.0f + dr::sqrt(1.0f + alpha_2 * tan_theta_2));

    // Perpendicular incidence -- no shadowing/masking
    dr::masked(result, v.z() == 1.f) = 1.f;
    /* Ensure consistent orientation (can't see the back
       of the microfacet from the front and vice versa) */
    dr::masked(result, dr::dot(v, wh) * Frame3f::cos_theta(v) <= 0.f) = 0.f;
    return result;
}

/**
 * \brief Get the flag which determines whether the corresponding
 * feature is going to be implemented or not.
 * \param name
 *     Name of the feature.
 * \param props
 *     Given properties.
 * \return the flag of the feature.
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
 * \brief Computes the schlick weight for Fresnel Schlick approximation.
 * \param cos_i
 *     Incident angle of the ray based on microfacet normal.
 * \return schlick weight
 */
template <typename Float>
Float schlick_weight(Float cos_i) {
    Float m = dr::clip(1.0f - cos_i, 0.0f, 1.0f);
    return dr::square(dr::square(m)) * m;
}

/**
 * \brief Schlick Approximation for Fresnel Reflection coefficient F = R0 +
 * (1-R0) (1-cos^5(i)). Transmitted ray's angle should be used for eta<1.
 * \param R0
 *     Incident specular. (Fresnel term when incident ray is aligned with
 *     the surface normal.)
 * \param cos_theta_i
 *     Incident angle of the ray based on microfacet normal.
 * \return Schlick approximation result.
 */
template <typename T,typename Float>
T calc_schlick(T R0, Float cos_theta_i,Float eta){
    dr::mask_t<Float> outside_mask = cos_theta_i >= 0.0f;
    Float rcp_eta     = dr::rcp(eta),
    eta_it      = dr::select(outside_mask, eta, rcp_eta),
    eta_ti      = dr::select(outside_mask, rcp_eta, eta);

    Float cos_theta_t_sqr = dr::fnmadd(
            dr::fnmadd(cos_theta_i, cos_theta_i, 1.0f), dr::square(eta_ti), 1.0f);
    Float cos_theta_t = dr::safe_sqrt(cos_theta_t_sqr);
    return dr::select(
            eta_it > 1.0f,
            dr::lerp(schlick_weight(dr::abs(cos_theta_i)), 1.0f, R0),
            dr::lerp(schlick_weight(cos_theta_t), 1.0f, R0));
}

/**
 * \brief Approximation of incident specular based on relative index of
 * refraction.
 * \param eta
 *     Relative index of refraction.
 * \return Incident specular
 */
template <typename Float>
Float schlick_R0_eta(Float eta){
    return dr::square((eta - 1.0f) / (eta + 1.0f));
}

/**
 * \brief Computes a mask for macro-micro surface incompatibilities.
 * \param m
 *     Micro surface normal.
 * \param wi
 *     Incident direction.
 * \param wo
 *     Outgoing direction.
 * \param cos_theta_i
 *     Incident angle
 * \param reflection
 *     Flag for determining reflection or refraction case.
 * \return  Macro-micro surface compatibility mask.
 */
template <typename Float>
dr::mask_t<Float> mac_mic_compatibility(const Vector<Float,3> &m,
                                        const Vector<Float,3> &wi,
                                        const Vector<Float,3> &wo,
                                        const Float &cos_theta_i,
                                        bool reflection) {
    if (reflection) {
        return (dr::dot(wi, dr::mulsign(m, cos_theta_i)) > 0.0f) &&
        (dr::dot(wo, dr::mulsign(m, cos_theta_i)) > 0.0f);
    } else {
        return (dr::dot(wi, dr::mulsign(m, cos_theta_i)) > 0.0f) &&
        (dr::dot(wo, dr::mulsign_neg(m, cos_theta_i)) > 0.0f);
    }
}

/**
 * \brief  Modified fresnel function for the principled material. It blends
 * metallic and dielectric responses (not true metallic). spec_tint portion
 * of the dielectric response is tinted towards base_color. Schlick
 * approximation is used for spec_tint and metallic parts whereas dielectric
 * part is calculated with the true fresnel dielectric implementation.
 * \param F_dielectric
 *     True dielectric response.
 * \param metallic
 *     Metallic weight.
 * \param spec_tint
 *     Specular tint weight.
 * \param base_color
 *     Base color of the material.
 * \param lum
 *     Luminance of the base color.
 * \param cos_theta_i
 *     Incident angle of the ray based on microfacet normal.
 * \param front_side
 *     Mask for front side of the macro surface.
 * \param bsdf
 *     Weight of the BSDF major lobe.
 * \return Fresnel term of principled BSDF with metallic and dielectric response
 * combined.
 */
template<typename Float,typename T>
T principled_fresnel(const Float &F_dielectric, const Float &metallic,
                     const Float &spec_tint,
                     const T &base_color,
                     const Float &lum, const Float &cos_theta_i,
                     const dr::mask_t<Float> &front_side,
                     const Float &bsdf, const Float &eta,
                     bool has_metallic, bool has_spec_tint) {
    // Outside mask based on micro surface
    dr::mask_t<Float> outside_mask = cos_theta_i >= 0.0f;
    Float rcp_eta = dr::rcp(eta);
    Float eta_it  = dr::select(outside_mask, eta, rcp_eta);
    T F_schlick(0.0f);

    // Metallic component based on Schlick.
    if (has_metallic) {
        F_schlick += metallic * calc_schlick<T>(
                base_color, cos_theta_i,eta);
    }

    // Tinted dielectric component based on Schlick.
    if (has_spec_tint) {
        T c_tint       =
                dr::select(lum > 0.0f, base_color / lum, 1.0f);
        T F0_spec_tint =
                c_tint * schlick_R0_eta(eta_it);
        F_schlick +=
                (1.0f - metallic) * spec_tint *
                calc_schlick<T>(F0_spec_tint, cos_theta_i,eta);
    }

    // Front side fresnel.
    T F_front =
            (1.0f - metallic) * (1.0f - spec_tint) * F_dielectric + F_schlick;
    /* For back side there is no tint or metallic, just true dielectric
       fresnel.*/
    return dr::select(front_side, F_front, bsdf * F_dielectric);
}

/**
* \brief  Modified Fresnel function for thin film approximation. It
* calculates the tinted Fresnel factor with Schlick Approximation.
* \param F_dielectric
*     True dielectric response.
* \param spec_tint
*     Specular tint weight.
* \param base_color
*     Base color of the material.
* \param lum
*     Luminance of the base color.
* \param cos_theta_i
*     Incident angle of the ray based on microfacet normal.
* \param eta_t
*     Relative index of Refraction of the thin Film
* \return Fresnel term of the thin BSDF with normal and tinted response
* combined.
*/
template<typename Float,typename T>
T thin_fresnel(const Float &F_dielectric, const Float &spec_tint,
             const T &base_color, const Float &lum,
             const Float &cos_theta_i, const Float &eta_t,
             bool has_spec_tint) {
    T F_schlick(0.0f);
    // Tinted dielectric component based on Schlick.
    if (has_spec_tint) {
        T c_tint       = dr::select(lum > 0.0f, base_color / lum, 1.0f);
        T F0_spec_tint = c_tint * schlick_R0_eta(eta_t);
        F_schlick = calc_schlick<T>(F0_spec_tint, cos_theta_i,eta_t);
    }
    return dr::lerp(F_dielectric,F_schlick,spec_tint);
}

/**
 * \brief Calculates the microfacet distribution parameters based on
 * Disney Course Notes.
 * \param anisotropic
 *     Anisotropy weight.
 * \param roughness
 *     Roughness parameter of the material.
 * \return Microfacet Distribution roughness parameters: alpha_x, alpha_y.
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
    Float aspect = dr::sqrt(1.0f - 0.9f * anisotropic);
    return { dr::maximum(0.001f, roughness_2 / aspect),
             dr::maximum(0.001f, roughness_2 * aspect) };
}
NAMESPACE_END(mitsuba)
