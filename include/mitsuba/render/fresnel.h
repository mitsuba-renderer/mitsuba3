#pragma once

#include <mitsuba/core/vector.h>
#include <enoki/complex.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Calculates the unpolarized Fresnel reflection coefficient
 * at a planar interface between two dielectrics
 *
 * \param cos_theta_i
 *      Cosine of the angle between the surface normal and the incident ray
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
 *     cos_theta_t Cosine of the angle between the surface normal and the transmitted ray
 *
 *     eta_it      Relative index of refraction in the direction of travel.
 *
 *     eta_ti      Reciprocal of the relative index of refraction in the
 *                 direction of travel. This also happens to be equal to
 *                 the scale factor that must be applied to the X and Y
 *                 component of the refracted direction.
 */
template <typename Float>
std::tuple<Float, Float, Float, Float> fresnel(Float cos_theta_i, Float eta) {
    auto outside_mask = cos_theta_i >= 0.f;

    Float rcp_eta = rcp(eta),
          eta_it = select(outside_mask, eta, rcp_eta),
          eta_ti = select(outside_mask, rcp_eta, eta);

    /* Using Snell's law, calculate the squared sine of the
       angle between the surface normal and the transmitted ray */
    Float cos_theta_t_sqr =
        fnmadd(fnmadd(cos_theta_i, cos_theta_i, 1.f), eta_ti * eta_ti, 1.f);

    /* Find the absolute cosines of the incident/transmitted rays */
    Float cos_theta_i_abs = abs(cos_theta_i);
    Float cos_theta_t_abs = safe_sqrt(cos_theta_t_sqr);

    auto index_matched = eq(eta, 1.f),
         special_case  = index_matched || eq(cos_theta_i_abs, 0.f);

    Float r_sc = select(index_matched, Float(0.f), Float(1.f));

    /* Amplitudes of reflected waves */
    Float a_s = fnmadd(eta_it, cos_theta_t_abs, cos_theta_i_abs) /
                 fmadd(eta_it, cos_theta_t_abs, cos_theta_i_abs);

    Float a_p = fnmadd(eta_it, cos_theta_i_abs, cos_theta_t_abs) /
                 fmadd(eta_it, cos_theta_i_abs, cos_theta_t_abs);

    Float r = .5f * (sqr(a_s) + sqr(a_p));

    masked(r, special_case) = r_sc;

    /* Adjust the sign of the transmitted direction */
    Float cos_theta_t = mulsign_neg(cos_theta_t_abs, cos_theta_i);

    return { r, cos_theta_t, eta_it, eta_ti };
}

/**
 * \brief Calculates the unpolarized Fresnel reflection coefficient at a planar
 * interface of a conductor, i.e. a surface with a complex-valued relative index
 * of refraction
 *
 * \remark
 *      The implementation assumes that cos_theta_i > 0, i.e. light enters
 *      from *outside* of the conducting layer (generally a reasonable
 *      assumption unless very thin layers are being simulated)
 *
 * \param cos_theta_i
 *      Cosine of the angle between the surface normal and the incident ray
 *
 * \param eta
 *      Relative refractive index (complex-valued)
 *
 * \return The unpolarized Fresnel reflection coefficient.
 */

template <typename Float>
Float fresnel_conductor(Float cos_theta_i, Complex<Float> eta) {
    // Modified from "Optics" by K.D. Moeller, University Science Books, 1988
    Float cos_theta_i_2 = cos_theta_i * cos_theta_i,
          sin_theta_i_2 = 1.f - cos_theta_i_2,
          sin_theta_i_4 = sin_theta_i_2 * sin_theta_i_2;

    auto eta_r = real(eta),
         eta_i = imag(eta);

    Float temp_1   = eta_r * eta_r - eta_i * eta_i - sin_theta_i_2,
          a_2_pb_2 = safe_sqrt(temp_1*temp_1 + 4.f * eta_i * eta_i * eta_r * eta_r),
          a        = safe_sqrt(.5f * (a_2_pb_2 + temp_1));

    Float term_1 = a_2_pb_2 + cos_theta_i_2,
          term_2 = 2.f * cos_theta_i * a;

    Float r_s = (term_1 - term_2) / (term_1 + term_2);

    Float term_3 = a_2_pb_2 * cos_theta_i_2 + sin_theta_i_4,
          term_4 = term_2 * sin_theta_i_2;

    Float r_p = r_s * (term_3 - term_4) / (term_3 + term_4);

    return .5f * (r_s + r_p);
}

/**
 * \brief Calculates the polarized Fresnel reflection coefficient at a planar
 * interface between two dielectrics. Returns complex values encoding the
 * amplitude and phase shift of the s- and p-polarized waves.
 *
 * \param cos_theta_i
 *      Cosine of the angle between the surface normal and the incident ray
 *
 * \param eta
 *      Real-valued relative refractive index of the interface.
 *      A value greater than 1.0 case means that the surface normal
 *      points into the region of lower density.
 *
 * \return A tuple (a_s, a_p, cos_theta_t, eta_it, eta_ti) consisting of
 *
 *     a_s         Perpendicularly polarized wave amplitude and phase shift.
 *
 *     a_p         Parallel polarized wave amplitude and phase shift.
 *
 *     cos_theta_t Cosine of the angle between the surface normal and the
 *                 transmitted ray. Zero in the case of total internal reflection.
 *
 *     eta_it      Relative index of refraction in the direction of travel
 *
 *     eta_ti      Reciprocal of the relative index of refraction in the
 *                 direction of travel. This also happens to be equal to the
 *                 scale factor that must be applied to the X and Y component
 *                 of the refracted direction.
 */
template <typename Float>
std::tuple<Complex<Float>, Complex<Float>, Float, Float, Float>
fresnel_polarized(Float cos_theta_i, Float eta) {
    auto outside_mask = cos_theta_i >= 0.f;

    Float rcp_eta = rcp(eta),
          eta_it  = select(outside_mask, eta, rcp_eta),
          eta_ti  = select(outside_mask, rcp_eta, eta);

    /* Using Snell's law, calculate the squared sine of the
       angle between the surface normal and the transmitted ray */
    Float cos_theta_t_sqr =
        fnmadd(fnmadd(cos_theta_i, cos_theta_i, 1.f), eta_ti * eta_ti, 1.f);

    /* Find the cosines of the incident/transmitted rays */
    Float cos_theta_i_abs = abs(cos_theta_i);
    Complex<Float> cos_theta_t = sqrtz(cos_theta_t_sqr);

    /* Choose the appropriate sign of the root (important when computing the
       phase difference under total internal reflection, see appendix A.2 of
       "Stellar Polarimetry" by David Clarke) */
    cos_theta_t = mulsign(Array<Float, 2>(cos_theta_t), cos_theta_t_sqr);

    /* Amplitudes of reflected waves. The sign convention of 'a_p' used here
       matches Fresnel's original paper from 1823 and is different from some
       contemporary references. See appendix A.1 of "Stellar Polarimetry" by
       David Clarke for a historical perspective. */
    Complex<Float> a_s = (-eta_it * cos_theta_t + cos_theta_i_abs) /
                         ( eta_it * cos_theta_t + cos_theta_i_abs);
    Complex<Float> a_p = (-eta_it * cos_theta_i_abs + cos_theta_t) /
                         ( eta_it * cos_theta_i_abs + cos_theta_t);

    auto index_matched = eq(eta, 1.f);
    auto invalid       = eq(eta, 0.f);
    masked(a_s, index_matched || invalid) = 0.f;
    masked(a_p, index_matched || invalid) = 0.f;

    /* Adjust the sign of the transmitted direction */
    Float cos_theta_t_signed =
        select(cos_theta_t_sqr >= 0.f,
               mulsign_neg(real(cos_theta_t), cos_theta_i), 0.f);

    return { a_s, a_p, cos_theta_t_signed, eta_it, eta_ti };
}

/**
 * \brief Calculates the polarized Fresnel reflection coefficient at a planar
 * interface between two dielectrics or conductors. Returns complex values
 * encoding the amplitude and phase shift of the s- and p-polarized waves.
 *
 * This is the most general version, which subsumes all others (at the cost of
 * transcendental function evaluations in the complex-valued arithmetic)
 *
 * \param cos_theta_i
 *      Cosine of the angle between the surface normal and the incident ray
 *
 * \param eta
 *      Complex-valued relative refractive index of the interface. In the real
 *      case, a value greater than 1.0 case means that the surface normal
 *      points into the region of lower density.
 *
 * \return A tuple (a_s, a_p, cos_theta_t, eta_it, eta_ti) consisting of
 *
 *     a_s         Perpendicularly polarized wave amplitude and phase shift.
 *
 *     a_p         Parallel polarized wave amplitude and phase shift.
 *
 *     cos_theta_t Cosine of the angle between the surface normal and the
 *                 transmitted ray. Zero in the case of total internal reflection.
 *
 *     eta_it      Relative index of refraction in the direction of travel
 *
 *     eta_ti      Reciprocal of the relative index of refraction in the
 *                 direction of travel. In the real-valued case, this
 *                 also happens to be equal to the scale factor that must
 *                 be applied to the X and Y component of the refracted
 *                 direction.
 */
template <typename Float>
std::tuple<Complex<Float>, Complex<Float>, Float, Complex<Float>, Complex<Float>>
fresnel_polarized(Float cos_theta_i, Complex<Float> eta) {
    auto outside_mask = cos_theta_i >= 0.f;

    Complex<Float> rcp_eta = rcp(eta),
                   eta_it  = select(outside_mask, eta, rcp_eta),
                   eta_ti  = select(outside_mask, rcp_eta, eta);

    /* Using Snell's law, calculate the squared sine of the
       angle between the surface normal and the transmitted ray */
    Complex<Float> cos_theta_t_sqr =
        1.f - sqr(eta_ti) * fnmadd(cos_theta_i, cos_theta_i, 1.f);

    /* Find the cosines of the incident/transmitted rays */
    Float cos_theta_i_abs = abs(cos_theta_i);
    Complex<Float> cos_theta_t = sqrt(cos_theta_t_sqr);

    /* Choose the appropriate sign of the root (important when computing the
       phase difference under total internal reflection, see appendix A.2 of
       "Stellar Polarimetry" by David Clarke) */
    cos_theta_t = mulsign(Array<Float, 2>(cos_theta_t), real(cos_theta_t_sqr));

    /* Amplitudes of reflected waves. The sign convention of 'a_p' used here
       matches Fresnel's original paper from 1823 and is different from some
       contemporary references. See appendix A.1 of "Stellar Polarimetry" by
       David Clarke for a historical perspective. */
    Complex<Float> a_s = (-eta_it * cos_theta_t + cos_theta_i_abs) /
                         ( eta_it * cos_theta_t + cos_theta_i_abs);
    Complex<Float> a_p = (-eta_it * cos_theta_i_abs + cos_theta_t) /
                         ( eta_it * cos_theta_i_abs + cos_theta_t);

    auto index_matched = eq(squared_norm(eta), 1.f) && eq(imag(eta), 0.f);
    auto invalid       = eq(squared_norm(eta), 0.f);
    masked(a_s, index_matched || invalid) = 0.f;
    masked(a_p, index_matched || invalid) = 0.f;

    /* Adjust the sign of the transmitted direction */
    Float cos_theta_t_signed =
        select(real(cos_theta_t_sqr) >= 0.f,
               mulsign_neg(real(cos_theta_t), cos_theta_i), 0.f);

    return { a_s, a_p, cos_theta_t_signed, eta_it, eta_ti };
}

/// Reflection in local coordinates
template <typename Float>
Vector<Float, 3> reflect(const Vector<Float, 3> &wi) {
    return Vector<Float, 3>(-wi.x(), -wi.y(), wi.z());
}

/// Reflect \c wi with respect to a given surface normal
template <typename Float>
Vector<Float, 3> reflect(const Vector<Float, 3> &wi, const Normal<Float, 3> &m) {
    return fmsub(Vector<Float, 3>(m), 2.f * dot(wi, m), wi);
}

/**
 * \brief Refraction in local coordinates
 *
 * The 'cos_theta_t' and 'eta_ti' parameters are given by the last two tuple
 * entries returned by the \ref fresnel and \ref fresnel_polarized functions.
 */
template <typename Float>
Vector<Float, 3> refract(const Vector<Float, 3> &wi, Float cos_theta_t, Float eta_ti) {
    return Vector<Float, 3>(-eta_ti * wi.x(), -eta_ti * wi.y(), cos_theta_t);
}

/**
 * \brief Refract \c wi with respect to a given surface normal
 *
 * \param wi
 *     Direction to refract
 * \param m
 *     Surface normal
 * \param cos_theta_t
 *     Cosine of the angle between the normal the transmitted
 *     ray, as computed e.g. by \ref fresnel.
 * \param eta_ti
 *     Relative index of refraction (transmitted / incident)
 */
template <typename Float>
Vector<Float, 3> refract(const Vector<Float, 3> &wi, const Normal<Float, 3> &m, Float cos_theta_t,
                         Float eta_ti) {
    return fmsub(m, fmadd(dot(wi, m), eta_ti, cos_theta_t), wi * eta_ti);
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
template <typename Float>
Float fresnel_diffuse_reflectance(Float eta) {
    /* Fast mode: the following code approximates the diffuse Frensel reflectance
       for the eta<1 and eta>1 cases. An evaluation of the accuracy led to the
       following scheme, which cherry-picks fits from two papers where they are
       best. */
    Float result(0.f);
    mask_t<Float> eta_l_1 = (eta < 1.f);
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
    Float inv_eta   = rcp(eta),
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
