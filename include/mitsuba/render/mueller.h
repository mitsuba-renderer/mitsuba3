#pragma once

#include <mitsuba/render/fwd.h>
#include <mitsuba/render/fresnel.h>
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
 * \brief Constructs the Mueller matrix of a linear retarder which has its fast
 * aligned vertically.
 *
 * This implements the general case with arbitrary phase shift and can be used
 * to construct the common special cases of quarter-wave and half-wave plates.
 *
 * \param phase
 *     The phase difference between the fast and slow axis
 *
 */
template <typename Value> MuellerMatrix<Value> linear_retarder(Value phase) {
    Value s, c;
    std::tie(s, c) = sincos(phase);
    return MuellerMatrix<Value>(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, c, -s,
        0, 0, s, c
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
    auto [s, c] = sincos(2.f * theta);
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
 * \brief Reverse direction of propagation of the electric field. Also used for
 * reflecting reference frames.
 */
template <typename Value>
MuellerMatrix<Value> reverse(const MuellerMatrix<Value> &M) {
    return MuellerMatrix<Value>(
        1, 0,  0,  0,
        0, 1,  0,  0,
        0, 0, -1,  0,
        0, 0,  0, -1
    ) * M;
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

    // Unit conversion factor
    Value factor = -eta_it * select(abs(cos_theta_i) > 1e-8f,
                                    cos_theta_t / cos_theta_i, 0.f);

    // Compute transmission amplitudes
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

/**
 * \brief Gives the reference frame basis for a Stokes vector
 *
 * \param w
 *      Direction of travel for Stokes vector (normalized)
 *
 * \return
 *      The (implicitly defined) reference coordinate system basis for the
 *      Stokes vector travelling along \ref w.
 *
 */
template <typename Vector3>
Vector3 stokes_basis(const Vector3 &w) {
    auto [s, t] = coordinate_system(w);
    return s;
}

/**
 * \brief Gives the Mueller matrix that alignes the reference frames (defined by
 * their respective basis vectors) of two collinear stokes vectors.
 *
 * \param forward
 *      Direction of travel for Stokes vector (normalized)
 *
 * \param basis_current
 *      Current (normalized) Stokes basis. Must be orthogonal to \c forward.
 *
 * \param basis_target
 *      Target (normalized) Stokes basis. Must be orthogonal to \c forward.
 *
 * \return
 *      Mueller matrix that performs the desired change of reference frames.
 */
template <typename Vector3,
          typename Value = value_t<Vector3>,
          typename MuellerMatrix = MuellerMatrix<Value>>
MuellerMatrix rotate_stokes_basis(const Vector3 &forward,
                                  const Vector3 &basis_current,
                                  const Vector3 &basis_target) {
    Value theta = unit_angle(normalize(basis_current),
                             normalize(basis_target));

    masked(theta, dot(forward, cross(basis_current, basis_target)) < 0) *= -1.f;
    return rotator(theta);
}

/**
 * \brief Return the Mueller matrix for some new reference frames.
 * This version rotates the input/output frames independently.
 *
 * \param M
 *      The current Mueller matrix that operates from \c in_basis_current to \c out_basis_current.
 *
 * \param in_forward
 *      Direction of travel for input Stokes vector (normalized)
 *
 * \param in_basis_current
 *      Current (normalized) input Stokes basis. Must be orthogonal to \c in_forward.
 *
 * \param in_basis_target
 *      Target (normalized) input Stokes basis. Must be orthogonal to \c in_forward.
 *
 * \param out_forward
 *      Direction of travel for input Stokes vector (normalized)
 *
 * \param out_basis_current
 *      Current (normalized) input Stokes basis. Must be orthogonal to \c out_forward.
 *
 * \param out_basis_target
 *      Target (normalized) input Stokes basis. Must be orthogonal to \c out_forward.
 *
 * \return
 *      New Mueller matrix that operates from \c in_basis_target to \c out_basis_target.
 */
template <typename Vector3,
          typename Value = value_t<Vector3>,
          typename MuellerMatrix = MuellerMatrix<Value>>
MuellerMatrix rotate_mueller_basis(const MuellerMatrix &M,
                                   const Vector3 &in_forward,
                                   const Vector3 &in_basis_current,
                                   const Vector3 &in_basis_target,
                                   const Vector3 &out_forward,
                                   const Vector3 &out_basis_current,
                                   const Vector3 &out_basis_target) {
    MuellerMatrix R_in  = rotate_stokes_basis(in_forward, in_basis_current, in_basis_target);
    MuellerMatrix R_out = rotate_stokes_basis(out_forward, out_basis_current, out_basis_target);
    return R_out * M * transpose(R_in);
}

/**
 * \brief Return the Mueller matrix for some new reference frames.
 * This version applies the same rotation to the input/output frames.
 *
 * \param M
 *      The current Mueller matrix that operates from \c basis_current to \c basis_current.
 *
 * \param forward
 *      Direction of travel for input Stokes vector (normalized)
 *
 * \param basis_current
 *      Current (normalized) input Stokes basis. Must be orthogonal to \c forward.
 *
 * \param basis_target
 *      Target (normalized) input Stokes basis. Must be orthogonal to \c forward.
 *
 * \return
 *      New Mueller matrix that operates from \c basis_target to \c basis_target.
 */
template <typename Vector3,
          typename Value = value_t<Vector3>,
          typename MuellerMatrix = MuellerMatrix<Value>>
MuellerMatrix rotate_mueller_basis_collinear(const MuellerMatrix &M,
                                             const Vector3 &forward,
                                             const Vector3 &basis_current,
                                             const Vector3 &basis_target) {
    MuellerMatrix R = rotate_stokes_basis(forward, basis_current, basis_target);
    return R * M * transpose(R);
}

NAMESPACE_END(mueller)
NAMESPACE_END(mitsuba)
