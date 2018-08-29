#pragma once

#include <mitsuba/render/fwd.h>
#include <mitsuba/render/reflection.h>
#include <enoki/matrix.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * This file provides a number of utility functions for constructing and
 * analyzing Mueller matrices. Mueller matrices describe how a scattering
 * interaction modifies the polarization state of light, which is assumed to be
 * encoded as a Stokes vector. Please also refer to the header file
 * <tt>mitsuba/render/stokes.h</tt>.
 */
NAMESPACE_BEGIN(mueller)

/**
* \brief Constructs the Mueller matrix of an ideal depolarizer
*
* \param value
*   The value of the (0, 0) element
*/
template <typename Value> MuellerMatrix<Value> depolarizer(Value value = 1.f) {
    MuellerMatrix<Value> result = zero<MuellerMatrix<Value>>();
    result(0, 0) = value;
    return result;
}

/**
* \brief Constructs the Mueller matrix of an ideal absorber
*
* \param value
*     The amount of absorption.
*/
template <typename Value> MuellerMatrix<Value> absorber(Value value) {
    return value;
}

/**
* \brief Constructs the Mueller matrix of a linear polarizer
* which transmits linear polarization at 0 degrees.
*
* \param value
*     The amount of attenuation of the transmitted component (1 corresponds
*     to an ideal polarizer).
*/
template <typename Value> MuellerMatrix<Value> linear_polarizer(Value value = 1.f) {
    Value a = value * .5f;
    return MuellerMatrix<Value>(
        a, a, 0, 0,
        a, a, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    );
}

/**
* \brief Constructs the Mueller matrix of a linear diattenuator, which
* attenuates the electric field components at 0 and 90 degrees by
* 'x' and 'y', * respectively.
*/
template <typename Value> MuellerMatrix<Value> diattenuator(Value x, Value y) {
    Value a = .5f * (x + y),
          b = .5f * (x - y),
          c = sqrt(x * y);

    return MuellerMatrix<Value>(
        a, b, 0, 0,
        b, a, 0, 0,
        0, 0, c, 0,
        0, 0, 0, c
    );
}

/**
  * \brief Constructs the Mueller matrix of an ideal rotator, which performs a
  * counter-clockwise rotation of the electric field by 'theta' radians.
  */
template <typename Value> MuellerMatrix<Value> rotator(Value theta) {
    Value s, c;
    std::tie(s, c) = sincos(2.f * theta);
    return MuellerMatrix<Value>(
        1, 0, 0, 0,
        0, c, -s, 0,
        0, s, c, 0,
        0, 0, 0, 1
    );
}

/**
  * \brief Applies a counter-clockwise rotation to the mueller matrix
  * of a given element.
  */
template <typename Value>
MuellerMatrix<Value> rotated_element(Value theta,
                                     const MuellerMatrix<Value> &M) {
    MuellerMatrix<Value> R = rotator(theta), Ri = transpose(R);
    return R * M * Ri;
}

/**
 * \brief Calculates the Mueller matrix of a specular reflection at an
 * interface between two dielectrics or conductors.
 *
 * \param cos_theta_i
 *      Cosine of the angle between the surface normal and the incident ray
 *
 * \param eta
 *      Complex-valued relative refractive index of the interface. In the real
 *      case, a value greater than 1.0 case means that the surface normal
 *      points into the region of lower density.
 */
template <typename Value, typename Eta>
MuellerMatrix<Value> specular_reflection(Value cos_theta_i, Eta eta) {
    Complex<Value> a_s, a_p;

    std::tie(a_s, a_p, std::ignore, std::ignore, std::ignore) =
        fresnel_polarized(cos_theta_i, eta);

    Value sin_delta, cos_delta;
    std::tie(sin_delta, cos_delta) = sincos_arg_diff(a_s, a_p);

    Value r_s = abs(sqr(a_s)),
          r_p = abs(sqr(a_p)),
          a = .5f * (r_s + r_p),
          b = .5f * (r_s - r_p),
          c = sqrt(r_s * r_p);

    masked(sin_delta, eq(c, 0.f)) = 0.f; // avoid issues with NaNs
    masked(cos_delta, eq(c, 0.f)) = 0.f;

    return MuellerMatrix<Value>(
        a, b, 0, 0,
        b, a, 0, 0,
        0, 0,  c * cos_delta, c * sin_delta,
        0, 0, -c * sin_delta, c * cos_delta
    );
}

/**
 * \brief Calculates the Mueller matrix of a specular transmission at an
 * interface between two dielectrics or conductors.
 *
 * \param cos_theta_i
 *      Cosine of the angle between the surface normal and the incident ray
 *
 * \param eta
 *      Complex-valued relative refractive index of the interface. A value
 *      greater than 1.0 in the real case means that the surface normal is
 *      pointing into the region of lower density.
 */
template <typename Value>
MuellerMatrix<Value> specular_transmission(Value cos_theta_i, Value eta) {
    Complex<Value> a_s, a_p;
    Value cos_theta_t, eta_it, eta_ti;

    std::tie(a_s, a_p, cos_theta_t, eta_it, eta_ti) =
        fresnel_polarized(cos_theta_i, eta);

    /* Unit conversion factor */
    Value factor = -eta_it * select(abs(cos_theta_i) > 1e-8f,
                                    cos_theta_t / cos_theta_i, 0.f);

    /* Compute transmission amplitudes */
    Value a_s_r = real(a_s) + 1.f,
          a_p_r = (1.f - real(a_p)) * eta_ti;

    Value t_s = sqr(a_s_r),
          t_p = sqr(a_p_r),
          a = .5f * factor * (t_s + t_p),
          b = .5f * factor * (t_s - t_p),
          c = factor * sqrt(t_s * t_p);

    return MuellerMatrix<Value>(
        a, b, 0, 0,
        b, a, 0, 0,
        0, 0, c, 0,
        0, 0, 0, c
    );
}

NAMESPACE_END(mueller)
NAMESPACE_END(mitsuba)
