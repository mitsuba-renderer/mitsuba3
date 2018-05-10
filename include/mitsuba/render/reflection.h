#pragma once

#include <mitsuba/core/vector.h>
#include <enoki/complex.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Calculates the polarized Fresnel reflection coefficient
 * at a planar interface between two dielectrics
 *
 * \param cos_theta_i
 *      Cosine of the angle between the normal and the incident ray
 *
 * \param eta
 *      Relative refractive index of the interface. A value greater than 1.0
 *      means that the surface normal is pointing into the region of lower
 *      density.
 *
 * \return A tuple (r_s, r_p, cos_theta_t, eta_it, eta_ti) consisting of
 *
 *     r_s         Perpendicularly polarized Fresnel reflectance ("senkrecht").
 *
 *     r_p         Parallel polarized Fresnel reflectance.
 *
 *     cos_theta_t Cosine of the angle between the normal and the transmitted ray
 *
 *     eta         Index of refraction in the direction of travel.
 *
 *     eta_it      Relative index of refraction in the direction of travel.
 *
 *     eta_ti      Reciprocal of the relative index of refraction in the
 *                 direction of travel. This also happens to be equal to
 *                 the scale factor that must be applied to the X and Y
 *                 component of the refracted direction.
 */
template <typename Value>
std::tuple<Value, Value, Value, Value, Value> fresnel_polarized(Value cos_theta_i, Value eta) {
    auto outside_mask = cos_theta_i >= 0.f;

    Value rcp_eta = rcp(eta),
          eta_it = select(outside_mask, eta, rcp_eta),
          eta_ti = select(outside_mask, rcp_eta, eta);

    /* Using Snell's law, calculate the squared sine of the
       angle between the normal and the transmitted ray */
    Value cos_theta_t_sqr =
        fnmadd(fnmadd(cos_theta_i, cos_theta_i, 1.f), eta_ti * eta_ti, 1.f);

    /* Find the absolute cosines of the incident/transmitted rays */
    Value cos_theta_i_abs = abs(cos_theta_i);
    Value cos_theta_t_abs = safe_sqrt(cos_theta_t_sqr);

    auto index_matched = eq(eta, 1.f),
         special_case  = index_matched || eq(cos_theta_i_abs, 0.f);

    Value a_sc = select(index_matched, Value(0.f), Value(1.f));

    /* Amplitudes of reflected waves */
    Value a_s = fnmadd(eta, cos_theta_t_abs, cos_theta_i_abs) /
                fmadd(eta, cos_theta_t_abs, cos_theta_i_abs);

    Value a_p = fmsub(eta, cos_theta_i_abs, cos_theta_t_abs) /
                fmadd(eta, cos_theta_i_abs, cos_theta_t_abs);

    masked(a_s, special_case) = a_sc;
    masked(a_p, special_case) = a_sc;

    /* Adjust the sign of the transmitted direction */
    Value cos_theta_t = -mulsign(cos_theta_t_abs, cos_theta_i);

    /* Convert from amplitudes to reflection coefficients */
    return { a_s * a_s, a_p * a_p, cos_theta_t, eta_it, eta_ti };
}

/**
 * \brief Calculates the unpolarized Fresnel reflection coefficient
 * at a planar interface between two dielectrics
 *
 * \param cos_theta_i
 *      Cosine of the angle between the normal and the incident ray
 *
 * \param eta
 *      Relative refractive index of the interface. A value greater than 1.0
 *      means that the surface normal is pointing into the region of lower
 *      density.
 *
 * \return A tuple (F, cos_theta_t, eta_it, eta_ti) consisting of
 *
 *     F           Fresnel reflection coefficient.
 *
 *     cos_theta_t Cosine of the angle between the normal and the transmitted ray
 *
 *     eta         Index of refraction in the direction of travel.
 *
 *     eta_it      Relative index of refraction in the direction of travel.
 *
 *     eta_ti      Reciprocal of the relative index of refraction in the
 *                 direction of travel. This also happens to be equal to
 *                 the scale factor that must be applied to the X and Y
 *                 component of the refracted direction.
 */
template <typename Value>
std::tuple<Value, Value, Value, Value> fresnel(Value cos_theta_i, Value eta) {
    Value r_s, r_p, cos_theta_t, eta_it, eta_ti;

    std::tie(r_s, r_p, cos_theta_t, eta_it, eta_ti) =
        fresnel_polarized(cos_theta_i, eta);

    return { .5f * (r_s + r_p), cos_theta_t, eta_it, eta_ti };
}

/**
 * \brief Calculates the polarized Fresnel reflection coefficient at a planar
 * interface having a complex-valued relative index of refraction (i.e. the
 * material conducts electrons)
 *
 * \remark
 *      The implementation assumes that cos_theta_i > 0, i.e. light enters
 *      from *outside* of the conducting layer (generally a reasonable
 *      assumption unless very thin layers are being simulated)
 *
 * \param cos_theta_i
 *      Cosine of the angle between the normal and the incident ray
 *
 * \param eta
 *      Relative refractive index (complex-valued)
 *
 * \return A pair (r_s, r_p) consisting of
 *
 *     r_s         Perpendicularly polarized Fresnel reflectance ("senkrecht").
 *
 *     r_p         Parallel polarized Fresnel reflectance.
 *
 */

template <typename Value>
std::pair<Value, Value>
fresnel_complex_polarized(Value cos_theta_i, Complex<Value> eta) {
    // Modified from "Optics" by K.D. Moeller, University Science Books, 1988
    Value cos_theta_i_2 = cos_theta_i * cos_theta_i,
          sin_theta_i_2 = 1.f - cos_theta_i_2,
          sin_theta_i_4 = sin_theta_i_2 * sin_theta_i_2;

    auto eta_r = real(eta),
         eta_i = imag(eta);

    Value temp_1   = eta_r * eta_r - eta_i * eta_i - sin_theta_i_2,
          a_2_pb_2 = safe_sqrt(temp_1*temp_1 + 4.f * eta_i * eta_i * eta_r * eta_r),
          a        = safe_sqrt(.5f * (a_2_pb_2 + temp_1));

    Value term_1 = a_2_pb_2 + cos_theta_i_2,
          term_2 = 2.f * cos_theta_i * a;

    Value r_s = (term_1 - term_2) / (term_1 + term_2);

    Value term_3 = a_2_pb_2 * cos_theta_i_2 + sin_theta_i_4,
          term_4 = term_2 * sin_theta_i_2;

    Value r_p = r_s * (term_3 - term_4) / (term_3 + term_4);

    return { r_s, r_p };
}

/**
 * \brief Calculates the unpolarized Fresnel reflection coefficient at a planar
 * interface having a complex-valued relative index of refraction (i.e. the
 * material conducts electrons)
 *
 * \remark
 *      The implementation assumes that cos_theta_i > 0, i.e. light enters
 *      from *outside* of the conducting layer (generally a reasonable
 *      assumption unless very thin layers are being simulated)
 *
 * \param cos_theta_i
 *      Cosine of the angle between the normal and the incident ray
 *
 * \param eta
 *      Relative refractive index (complex-valued)
 *
 * \return The unpolarized Fresnel reflection coefficient.
 */

template <typename Value>
Value fresnel_complex(Value cos_theta_i, Complex<Value> eta) {
    Value r_s, r_p;
    std::tie(r_s, r_p) = fresnel_complex_polarized(cos_theta_i, eta);
    return .5f * (r_s + r_p);
}

/// Reflect \c wi with respect to a given surface normal
template <typename Vector3, typename Normal3>
Vector3 reflect(const Vector3 &wi, const Normal3 &m) {
    return 2.f * dot(wi, m) * Vector3(m) - wi;
}

/// Reflection in local coordinates
template <typename Vector3>
Vector3 reflect(const Vector3 &wi) {
    return Vector3(-wi.x(), -wi.y(), wi.z());
}

/**
 * \brief Refract \c wi with respect to a given surface normal
 *
 * \param wi   Direction to refract
 * \param m    Surface normal
 * \param eta  Ratio of interior to exterior IORs at the interface.
 * \param cos_theta_t Cosine of the angle between the normal the the transmitted
 *                    ray, as computed e.g. by \ref fresnel_dielectric_ext.
 */
// TODO: this is not compatible with a wavelength-dependent `eta`
template <typename Vector3, typename Normal3, typename Value = value_t<Vector3>>
Vector3 refract(const Vector3 &wi, const Normal3 &m, const Value &eta, const Value &cos_theta_t) {
    Value scale = select(cos_theta_t < 0.0f, rcp(eta), eta);
    return m * (dot(wi, m) * scale + cos_theta_t) - wi * scale;
}

/**
 * \brief Refraction in local coordinates
 *
 * The 'cos_theta_t' and 'eta_ti' parameters are given by the last two tuple
 * entries returned by the \ref fresnel and \ref fresnel_polarized functions.
 */
template <typename Vector3, typename Value>
Vector3 refract(const Vector3 &wi, Value cos_theta_t, Value eta_ti) {
    return Vector3(-eta_ti * wi.x(), -eta_ti * wi.y(), cos_theta_t);
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
    Value result(0.f);
    mask_t<Value> eta_l_1 = (eta < 1.f);
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

NAMESPACE_END(mitsuba)
