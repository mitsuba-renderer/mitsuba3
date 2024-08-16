#pragma once

#include <mitsuba/core/vector.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/interaction.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief The parameters of the SGGX phase function stored as a pair of 
 * 3D vectors [[S_xx, S_yy, S_zz], [S_xy, S_xz, S_yz]]
 */
template <typename Float>
struct SGGXPhaseFunctionParams {
    dr::Array<Float, 3> diag;
    dr::Array<Float, 3> off_diag;

    /**
     * \brief Construct from a pair of 3D vectors [S_xx, S_yy, S_zz] and 
     * [S_xy, S_xz, S_yz] that correspond to the entries of a symmetric positive
     * definite 3x3 matrix.
     */
    SGGXPhaseFunctionParams(const dr::Array<Float, 3>& diag, 
                            const dr::Array<Float, 3>& off_diag) :
        diag(diag),
        off_diag(off_diag) {}

    operator const dr::Array<Float, 6>() const { 
        return { 
            diag[0], diag[1], diag[2],
            off_diag[0], off_diag[1], off_diag[2]
        };
    }

    DRJIT_STRUCT(SGGXPhaseFunctionParams, diag, off_diag);
};

/// Return a string representation of SGGXPhaseFunction parameters
template <typename Float>
std::ostream &operator<<(std::ostream &os, const SGGXPhaseFunctionParams<Float> &s) {
    os << "SGGXPhaseFunctionParams[" << std::endl
       << "  [S_xx, S_yy, S_zz] = " << string::indent(s.diag, 6) << "," << std::endl
       << "  [S_xy, S_xz, S_yz] = " << string::indent(s.off_diag, 6) << "," << std::endl
       << "]";
    return os;
}

/**
 * \brief Samples the visible normal distribution of the SGGX
 * microflake distribution
 *
 * This function is based on the paper
 *
 *   "The SGGX microflake distribution", Siggraph 2015
 *   by Eric Heitz, Jonathan Dupuy, Cyril Crassin and Carsten Dachsbacher
 *
 * \param sh_frame
 *      Shading frame aligned with the incident direction,
 *      e.g. constructed as Frame3f(wi)
 *
 * \param sample
 *      A uniformly distributed 2D sample
 *
 * \param s
 *      The parameters of the SGGX phase function S_xx, S_yy, S_zz, S_xy, S_xz,
 *      and S_yz that describe the entries of a symmetric positive definite 3x3
 *      matrix. The user needs to ensure that the parameters indeed represent a
 *      positive definite matrix.
 *
 * \return A normal (in world space) sampled from the distribution
 *         of visible normals
 */
template <typename Float>
Normal<Float, 3> sggx_sample(const Frame<Float> &sh_frame,
                             const Point<Float, 2> &sample,
                             const dr::Array<Float, 6> &s) {
    constexpr size_t XX = 0, YY = 1, ZZ = 2, XY = 3, XZ = 4, YZ = 5;
    constexpr size_t k = 0, j = 1, i = 2;

    using Matrix3f = dr::Matrix<Float, 3>;
    using Vector3f = Vector<Float, 3>;

    Matrix3f m(sh_frame.s, sh_frame.t, sh_frame.n);
    m = dr::transpose(m);
    Matrix3f s_mat(s[XX], s[XY], s[XZ],
                   s[XY], s[YY], s[YZ],
                   s[XZ], s[YZ], s[ZZ]);
    Matrix3f s2 = m * s_mat * dr::transpose(m);
    Float inv_sqrt_s_ii = dr::safe_rsqrt(s2(i, i));
    Float tmp = dr::safe_sqrt(s2(j, j) * s2(i, i) - s2(j, i) * s2(j, i));
    Vector3f m_k(dr::safe_sqrt(dr::abs(dr::det(s2))) / tmp, 0.f, 0.f);
    Vector3f m_j(-inv_sqrt_s_ii * (s2(k, i) * s2(j, i) - s2(k, j) * s2(i, i)) / tmp,
                 inv_sqrt_s_ii * tmp, 0.f);
    Vector3f m_i = inv_sqrt_s_ii * Vector3f(s2(k, i), s2(j, i), s2(i, i));
    Vector3f uvw = warp::square_to_cosine_hemisphere(sample);
    return sh_frame.to_world(
        dr::normalize(uvw.x() * m_k + uvw.y() * m_j + uvw.z() * m_i));
}

template <typename Float>
Normal<Float, 3> sggx_sample(const Vector<Float, 3> &wi,
                             const Point<Float, 2> &sample,
                             const dr::Array<Float, 6> &s) {
    return sggx_sample(Frame<Float>(wi), sample, s);
}

/**
 * \brief Evaluates the probability of sampling a given normal
 * using the SGGX microflake distribution
 *
 * \param wm
 *      The microflake normal
 *
 * \param s
 *      The parameters of the SGGX phase function S_xx, S_yy, S_zz, S_xy, S_xz,
 *      and S_yz that describe the entries of a symmetric positive definite 3x3
 *      matrix. The user needs to ensure that the parameters indeed represent a
 *      positive definite matrix.
 *
 * \return The probability of sampling a certain normal
 */
template <typename Float>
Float sggx_pdf(const Vector<Float, 3> &wm, const dr::Array<Float, 6> &s) {
    const size_t XX = 0, YY = 1, ZZ = 2, XY = 3, XZ = 4, YZ = 5;

    Float det_s = dr::abs(s[XX] * s[YY] * s[ZZ] - s[XX] * s[YZ] * s[YZ] -
                          s[YY] * s[XZ] * s[XZ] - s[ZZ] * s[XY] * s[XY] +
                          2.f * s[XY] * s[XZ] * s[YZ]);
    Float den   = wm.x() * wm.x() * (s[YY] * s[ZZ] - s[YZ] * s[YZ]) +
                  wm.y() * wm.y() * (s[XX] * s[ZZ] - s[XZ] * s[XZ]) +
                  wm.z() * wm.z() * (s[XX] * s[YY] - s[XY] * s[XY]) +
                  2.f * (wm.x() * wm.y() * (s[XZ] * s[YZ] - s[ZZ] * s[XY]) +
                         wm.x() * wm.z() * (s[XY] * s[YZ] - s[YY] * s[XZ]) +
                         wm.y() * wm.z() * (s[XY] * s[XZ] - s[XX] * s[YZ]));
    return dr::maximum(det_s, 0.f) * dr::safe_sqrt(det_s) /
           (dr::Pi<Float> * dr::square(den));
}

/**
 * \brief Evaluates the projected area of the SGGX microflake distribution
 *
 * \param wi
 *      A 3D direction
 *
 * \param s
 *      The parameters of the SGGX phase function S_xx, S_yy, S_zz, S_xy, S_xz,
 *      and S_yz that describe the entries of a symmetric positive definite 3x3
 *      matrix. The user needs to ensure that the parameters indeed represent a
 *      positive definite matrix.
 *
 * \return The projected area of the SGGX microflake distribution
 */
template <typename Float>
MI_INLINE Float sggx_projected_area(const Vector<Float, 3> &wi,
                                    const dr::Array<Float, 6> &s) {
    const size_t XX = 0, YY = 1, ZZ = 2, XY = 3, XZ = 4, YZ = 5;

    // Computes sqrt(wi^T * S * wi)
    Float sigma2 = wi.x() * wi.x() * s[XX] + wi.y() * wi.y() * s[YY] +
                   wi.z() * wi.z() * s[ZZ] +
                   2.f * (wi.x() * wi.y() * s[XY] + wi.x() * wi.z() * s[XZ] +
                          wi.y() * wi.z() * s[YZ]);
    return dr::safe_sqrt(sigma2);
}

NAMESPACE_END(mitsuba)
