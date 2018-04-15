#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/// Reflect \c wi with respect to a given surface normal
template <typename Vector3, typename Normal3>
Vector3 reflect(const Vector3 &wi, const Normal3 &m) {
    return 2.0f * dot(wi, m) * Vector3(m) - wi;
}

/// Reflection in local coordinates
template <typename Vector3>
Vector3 reflect(const Vector3 &wi) {
    return Vector3(-wi.x(), -wi.y(), wi.z());
}

/**
 * \brief Refraction in local coordinates
 *
 * \param wi   Direction to refract
 * \param eta  Ratio of interior to exterior IORs at the interface.
 * \param cos_theta_t Cosine of the angle between the normal and the transmitted
 *                    ray, as computed e.g. by \ref fresnel_dielectric_ext.
 */
// TODO: this is not compatible with a wavelength-dependent `eta`
template <typename Vector3, typename Value = value_t<Vector3>>
Vector3 refract(const Vector3 &wi, const Value &eta, const Value &cos_theta_t) {
    Value scale = select(cos_theta_t < 0.0f, Value(-rcp(eta)), Value(-eta));
    return Vector3(scale * wi.x(), scale * wi.y(), cos_theta_t);
}

/// Transmission in local coordinates
template <typename Vector3>
Vector3 transmit(const Vector3 &wi) {
    return -wi;
}

/**
 * \brief Computes the diffuse unpolarized Fresnel reflectance of a dielectric
 * material (sometimes referred to as "Fdr").
 *
 * This value quantifies what fraction of diffuse incident illumination
 * will, on average, be reflected at a dielectric material boundary
 *
 * \param eta
 *      Relative refraction coefficient
 * \return F, the unpolarized Fresnel coefficient.
 */
template <typename Value>
Value fresnel_diffuse_reflectance(Value eta) {
    /* Fast mode: the following code approximates the diffuse Frensel reflectance
       for the eta<1 and eta>1 cases. An evalution of the accuracy led to the
       following scheme, which cherry-picks fits from two papers where they are
       best. */
    Value result(0.0f);
    mask_t<Value> eta_l_1 = (eta < 1.0f);
    /* Fit by Egan and Hilgeman (1973). Works reasonably well for
       "normal" IOR values (<2).
       Max rel. error in 1.0 - 1.5 : 0.1%
       Max rel. error in 1.5 - 2   : 0.6%
       Max rel. error in 2.0 - 5   : 9.5%
    */
    masked(result, eta_l_1) = -1.4399f * (eta * eta)
                              + 0.7099f * eta
                              + 0.6681f
                              + 0.0636f / eta;
    /* Fit by d'Eon and Irving (2011)

       Maintains a good accuracy even for unrealistic IOR values.

       Max rel. error in 1.0 - 2.0   : 0.1%
       Max rel. error in 2.0 - 10.0  : 0.2%  */
    Value inv_eta   = rcp(eta),
          inv_eta_2 = inv_eta   * inv_eta,
          inv_eta_3 = inv_eta_2 * inv_eta,
          inv_eta_4 = inv_eta_3 * inv_eta,
          inv_eta_5 = inv_eta_4 * inv_eta;
    masked(result, !eta_l_1) = 0.919317f - 3.4793f * inv_eta
                               + 6.75335f * inv_eta_2
                               - 7.80989f * inv_eta_3
                               + 4.98554f * inv_eta_4
                               - 1.36881f * inv_eta_5;
    return result;
}

/**
 * \brief Computes the unpolarized Fresnel reflection coefficient at a planar
 * interface having a complex-valued relative index of refraction.
 *
 * The implementation of this function computes the exact unpolarized
 * Fresnel reflectance for a complex index of refraction change.
 *
 * The name of this function is a slight misnomer, since it supports
 * the general case of a complex-valued relative index of refraction
 * (rather than being restricted to conductors)
 *
 * \param cos_theta_i
 *      Cosine of the angle between the normal and the incident ray
 * \param eta
 *      Relative refractive index (real component)
 * \param k
 *      Relative refractive index (imaginary component)
 *
 * \return F, the unpolarized Fresnel reflection coefficient.
 */
template <typename Spectrum, typename Value = value_t<Spectrum>>
Spectrum fresnel_conductor_exact(Value cos_theta_i, Spectrum eta, Spectrum k) {
    // Modified from "Optics" by K.D. Moeller, University Science Books, 1988
    Value cos_theta_i_2 = cos_theta_i * cos_theta_i,
          sin_theta_i_2 = 1.f - cos_theta_i_2,
          sin_theta_i_4 = sin_theta_i_2 * sin_theta_i_2;

    Spectrum temp_1   = eta * eta - k * k - Spectrum(sin_theta_i_2),
             a_2_pb_2 = safe_sqrt(temp_1*temp_1 + 4.f * k * k * eta * eta),
             a        = safe_sqrt(0.5f * (a_2_pb_2 + temp_1));

    Spectrum term_1   = a_2_pb_2 + Spectrum(cos_theta_i_2),
             term_2   = 2.f * cos_theta_i * a;

    Spectrum rs_2     = (term_1 - term_2) / (term_1 + term_2);

    Spectrum term_3   = a_2_pb_2 * cos_theta_i_2 + Spectrum(sin_theta_i_4),
             term_4   = term_2 * sin_theta_i_2;

    Spectrum rp_2     = rs_2 * (term_3 - term_4) / (term_3 + term_4);

    return 0.5f * (rp_2 + rs_2);
}

/**
 * \brief Calculates the unpolarized Fresnel reflection coefficient
 * at a planar interface between two dielectrics (extended version)
 *
 * \param cos_theta_i_
 *      Cosine of the angle between the normal and the incident ray
 * \param eta
 *      Relative refractive index
 * \param active
 *      Mask for active lanes
 *
 * \return (F, cos_theta_t)
 *     F           Fresnel reflection coefficient.
 *     cos_theta_t Cosine of the angle between the normal and the transmitted
 *                 ray, as computed e.g. by \ref fresnel_dielectric_ext.
 *
 */
template <typename Spectrum, typename Value = value_t<Spectrum>>
std::pair<Value, Value> fresnel_dielectric_ext(Value cos_theta_i_, Spectrum eta,
                                               mask_t<Value> active = true) {
    active = active && neq(eta, 1);
    Value result(0.0f);
    Value cos_theta_t_ = -cos_theta_i_;

    if (unlikely(none(active)))
        return { result, cos_theta_t_ };

    /* Using Snell's law, calculate the squared sine of the
       angle between the normal and the transmitted ray */
    Value scale = select(cos_theta_i_ > 0.0f, rcp(eta), eta);

    Value cos_theta_t_sqr = 1.0f - (1.0f - cos_theta_i_ * cos_theta_i_) * (scale * scale);

    /* Check for total internal reflection */
    mask_t<Value> total_int_ref = (cos_theta_t_sqr <= 0.0f);
    masked(cos_theta_t_, active && total_int_ref) = 0.0f;
    masked(result,       active && total_int_ref) = 1.0f;

    active = active && !total_int_ref;
    if (none(active))
        return { result, cos_theta_t_ };

    /* Find the absolute cosines of the incident/transmitted rays */
    Value cos_theta_i = abs(cos_theta_i_);
    Value cos_theta_t = sqrt(cos_theta_t_sqr);

    Value Rs = (cos_theta_i - eta * cos_theta_t)
             / (cos_theta_i + eta * cos_theta_t);
    Value Rp = (eta * cos_theta_i - cos_theta_t)
             / (eta * cos_theta_i + cos_theta_t);

    /* No polarization -- return the unpolarized reflectance */
    masked(cos_theta_t_, active) = select(cos_theta_i_ > 0.0f, -cos_theta_t, cos_theta_t);
    masked(result,       active) = 0.5f * (Rs * Rs + Rp * Rp);
    return { result, cos_theta_t_ };
}

NAMESPACE_END(mitsuba)
