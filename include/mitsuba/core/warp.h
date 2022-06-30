#pragma once

#include <mitsuba/core/frame.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Implements common warping techniques that map from the unit
 * square [0, 1]^2 to other domains such as spheres, hemispheres, etc.
 *
 * The main application of this class is to generate uniformly
 * distributed or weighted point sets in certain common target domains.
 */
NAMESPACE_BEGIN(warp)

// =======================================================================
//! @{ \name Warping techniques that operate in the plane
// =======================================================================

template <typename Value>
Value circ(Value x) { return dr::safe_sqrt(dr::fnmadd(x, x, 1.f)); }

/// Uniformly sample a vector on a 2D disk
template <typename Value>
MI_INLINE Point<Value, 2> square_to_uniform_disk(const Point<Value, 2> &sample) {
    Value r = dr::sqrt(sample.y());
    auto [s, c] = dr::sincos(dr::TwoPi<Value> * sample.x());
    return { c * r, s * r };
}

/// Inverse of the mapping \ref square_to_uniform_disk
template <typename Value>
MI_INLINE Point<Value, 2> uniform_disk_to_square(const Point<Value, 2> &p) {
    Value phi = dr::atan2(p.y(), p.x()) * dr::InvTwoPi<Value>;
    return { dr::select(phi < 0.f, phi + 1.f, phi), dr::squared_norm(p) };
}

/// Density of \ref square_to_uniform_disk per unit area
template <bool TestDomain = false, typename Value>
MI_INLINE Value square_to_uniform_disk_pdf(const Point<Value, 2> &p) {
    DRJIT_MARK_USED(p);
    if constexpr (TestDomain)
        return dr::select(dr::squared_norm(p) > 1.f, 0.f, dr::InvPi<Value>);
    else
        return dr::InvPi<Value>;
}

// =======================================================================

/// Low-distortion concentric square to disk mapping by Peter Shirley
template <typename Value>
MI_INLINE Point<Value, 2> square_to_uniform_disk_concentric(const Point<Value, 2> &sample) {
    using Mask   = dr::mask_t<Value>;

    Value x = dr::fmsub(2.f, sample.x(), 1.f),
          y = dr::fmsub(2.f, sample.y(), 1.f);

    /* Modified concentric map code with less branching (by Dave Cline), see
       http://psgraphics.blogspot.ch/2011/01/improved-code-for-concentric-map.html

      Original non-vectorized version:

        Value phi, r;
        if (x == 0 && y == 0) {
            r = phi = 0;
        } else if (x * x > y * y) {
            r = x;
            phi = (dr::Pi / 4.f) * (y / x);
        } else {
            r = y;
            phi = (dr::Pi / 2.f) - (x / y) * (dr::Pi / 4.f);
        }
    */

    Mask is_zero         = dr::eq(x, 0.f) &&
                           dr::eq(y, 0.f),
         quadrant_1_or_3 = dr::abs(x) < dr::abs(y);

    Value r  = dr::select(quadrant_1_or_3, y, x),
          rp = dr::select(quadrant_1_or_3, x, y);

    Value phi = 0.25f * dr::Pi<Value> * rp / r;
    dr::masked(phi, quadrant_1_or_3) = 0.5f * dr::Pi<Value> - phi;
    dr::masked(phi, is_zero) = 0.f;

    auto [s, c] = dr::sincos(phi);
    return { r * c, r * s };
}

/// Inverse of the mapping \ref square_to_uniform_disk_concentric
template <typename Value>
MI_INLINE Point<Value, 2> uniform_disk_to_square_concentric(const Point<Value, 2> &p) {
    using Mask = dr::mask_t<Value>;

    Mask quadrant_0_or_2 = dr::abs(p.x()) > dr::abs(p.y());
    Value r_sign = dr::select(quadrant_0_or_2, p.x(), p.y());
    Value r = dr::copysign(dr::norm(p), r_sign);

    Value phi = dr::atan2(dr::mulsign(p.y(), r_sign),
                          dr::mulsign(p.x(), r_sign));

    Value t = 4.f / dr::Pi<Value> * phi;
    t = dr::select(quadrant_0_or_2, t, 2.f - t) * r;

    Value a = dr::select(quadrant_0_or_2, r, t);
    Value b = dr::select(quadrant_0_or_2, t, r);

    return { (a + 1.f) * 0.5f, (b + 1.f) * 0.5f };
}

/// Density of \ref square_to_uniform_disk per unit area
template <bool TestDomain = false, typename Value>
MI_INLINE Value square_to_uniform_disk_concentric_pdf(const Point<Value, 2> &p) {
    DRJIT_MARK_USED(p);
    if constexpr (TestDomain)
        return dr::select(dr::squared_norm(p) > 1.f, 0.f, dr::InvPi<Value>);
    else
        return dr::InvPi<Value>;
}

// =======================================================================

/**
 * \brief Low-distortion concentric square to square mapping (meant to be used
 * in conjunction with another warping method that maps to the sphere)
 */
template <typename Value> MI_INLINE Point<Value, 2>
square_to_uniform_square_concentric(const Point<Value, 2> &sample) {
    using Mask = dr::mask_t<Value>;

    Value x = dr::fmsub(2.f, sample.x(), 1.f),
          y = dr::fmsub(2.f, sample.y(), 1.f);

    Mask quadrant_1_or_3 = dr::abs(x) < dr::abs(y);

    Value r  = dr::select(quadrant_1_or_3, y, x),
          rp = dr::select(quadrant_1_or_3, x, y);

    Value phi = rp / r * 0.125f;
    dr::masked(phi, quadrant_1_or_3) = 0.25f - phi;
    dr::masked(phi, r < 0.f) += 0.5f;
    dr::masked(phi, phi < 0.f) += 1.f;

    return { phi, dr::sqr(r) };
}

// =======================================================================

/// Convert an uniformly distributed square sample into barycentric coordinates
template <typename Value>
MI_INLINE Point<Value, 2> square_to_uniform_triangle(const Point<Value, 2> &sample) {
    Value t = dr::safe_sqrt(1.f - sample.x());
    return { 1.f - t, t * sample.y() };
}

/// Inverse of the mapping \ref square_to_uniform_triangle
template <typename Value>
MI_INLINE Point<Value, 2> uniform_triangle_to_square(const Point<Value, 2> &p) {
    Value t = 1.f - p.x();
    return Point<Value, 2>(1.f - t * t, p.y() / t);
}

/// Density of \ref square_to_uniform_triangle per unit area.
template <bool TestDomain = false, typename Value>
MI_INLINE Value square_to_uniform_triangle_pdf(const Point<Value, 2> &p) {
    if constexpr (TestDomain) {
        return dr::select(p.x() < 0.f || p.y() < 0.f || (p.x() + p.y() > 1.f),
                          0.f, 2.f);
    } else {
        return 2.f;
    }
}

// =======================================================================

/// Sample a point on a 2D standard normal distribution. Internally uses the Box-Muller transformation
template <typename Value>
MI_INLINE Point<Value, 2> square_to_std_normal(const Point<Value, 2> &sample) {
    Value r   = dr::sqrt(-2.f * dr::log(1.f - sample.x())),
          phi = 2.f * dr::Pi<Value> * sample.y();

    auto [s, c] = dr::sincos(phi);
    return { c * r, s * r };
}

template <typename Value>
MI_INLINE Value square_to_std_normal_pdf(const Point<Value, 2> &p) {
    return dr::InvTwoPi<Value> * dr::exp(-.5f * dr::squared_norm(p));
}

// =======================================================================

/// Warp a uniformly distributed sample on [0, 1] to a tent distribution
template <typename Value>
Value interval_to_tent(Value sample) {
    sample -= 0.5f;
    return dr::copysign(1.f - dr::safe_sqrt(dr::fmadd(dr::abs(sample), -2.f, 1.f)), sample);
}

/// Warp a uniformly distributed sample on [0, 1] to a tent distribution
template <typename Value>
Value tent_to_interval(const Value &value) {
    return 0.5f * (1.f + value * (2.f - dr::abs(value)));
}

/// Warp a uniformly distributed sample on [0, 1] to a nonuniform tent distribution with nodes <tt>{a, b, c}</tt>
template <typename Value>
Value interval_to_nonuniform_tent(const Value &a, const Value &b, const Value &c, const Value &sample_) {
    auto mask = (sample_ * (c - a) < b - a);
    Value factor = dr::select(mask, a - b, c - b);
    Value sample = dr::select(mask, sample_ * ((a - c) / (a - b)),
                    ((a - c) / (b - c)) * (sample_ - ((a - b) / (a - c))));
    return b + factor * (1.f - dr::safe_sqrt(sample));
}

// =======================================================================

/// Warp a uniformly distributed square sample to a 2D tent distribution
template <typename Value>
Point<Value, 2> square_to_tent(const Point<Value, 2> &sample) {
    return interval_to_tent(sample);
}

/// Warp a uniformly distributed square sample to a 2D tent distribution
template <typename Value>
Point<Value, 2> tent_to_square(const Point<Value, 2> &p) {
    return tent_to_interval(p);
}

/// Density of \ref square_to_tent per unit area.
template <typename Value>
Value square_to_tent_pdf(const Point<Value, 2> &p_) {
    auto p = dr::abs(p_);
    return dr::select(p.x() <= 1 && p.y() <= 1,
                      (1.f - p.x()) * (1.f - p.y()),
                      0.f);
}

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Warping techniques related to spheres and subsets
// =======================================================================

/// Uniformly sample a vector on the unit sphere with respect to solid angles
template <typename Value>
MI_INLINE Vector<Value, 3> square_to_uniform_sphere(const Point<Value, 2> &sample) {
    Value z = dr::fnmadd(2.f, sample.y(), 1.f),
          r = circ(z);
    auto [s, c] = dr::sincos(2.f * dr::Pi<Value> * sample.x());
    return { r * c, r * s, z };
}

/// Inverse of the mapping \ref square_to_uniform_sphere
template <typename Value>
MI_INLINE Point<Value, 2> uniform_sphere_to_square(const Vector<Value, 3> &p) {
    Value phi = dr::atan2(p.y(), p.x()) * dr::InvTwoPi<Value>;
    return {
        dr::select(phi < 0.f, phi + 1.f, phi),
        (1.f - p.z()) * 0.5f
    };
}

/// Density of \ref square_to_uniform_sphere() with respect to solid angles
template <bool TestDomain = false, typename Value>
MI_INLINE Value square_to_uniform_sphere_pdf(const Vector<Value, 3> &v) {
    DRJIT_MARK_USED(v);
    if constexpr (TestDomain)
        return dr::select(dr::abs(dr::squared_norm(v) - 1.f) > math::RayEpsilon<Value>,
                          0.f, dr::InvFourPi<Value>);
    else
        return dr::InvFourPi<Value>;
}

// =======================================================================

/// Uniformly sample a vector on the unit hemisphere with respect to solid angles
template <typename Value>
MI_INLINE Vector<Value, 3> square_to_uniform_hemisphere(const Point<Value, 2> &sample) {
#if 0
    // Approach 1: warping method based on standard disk mapping
    Value z   = sample.y(),
          tmp = circ(z);
    auto [s, c] = dr::sincos(Scalar(2 * dr::Pi) * sample.x());
    return { c * tmp, s * tmp, z };
#else
    // Approach 2: low-distortion warping technique based on concentric disk mapping
    Point<Value, 2> p = square_to_uniform_disk_concentric(sample);
    Value z = 1.f - dr::squared_norm(p);
    p *= dr::sqrt(z + 1.f);
    return { p.x(), p.y(), z };
#endif
}

/// Inverse of the mapping \ref square_to_uniform_hemisphere
template <typename Value>
MI_INLINE Point<Value, 2> uniform_hemisphere_to_square(const Vector<Value, 3> &v) {
    Point<Value, 2> p(v.x(), v.y());
    return uniform_disk_to_square_concentric(p * dr::rsqrt(v.z() + 1.f));
}

/// Density of \ref square_to_uniform_hemisphere() with respect to solid angles
template <bool TestDomain = false, typename Value>
MI_INLINE Value square_to_uniform_hemisphere_pdf(const Vector<Value, 3> &v) {
    DRJIT_MARK_USED(v);
    if constexpr (TestDomain)
        return dr::select(dr::abs(dr::squared_norm(v) - 1.f) > math::RayEpsilon<Value> ||
                      v.z() < 0.f, 0.f, dr::InvTwoPi<Value>);
    else
        return dr::InvTwoPi<Value>;
}

// =======================================================================

/// Sample a cosine-weighted vector on the unit hemisphere with respect to solid angles
template <typename Value>
MI_INLINE Vector<Value, 3> square_to_cosine_hemisphere(const Point<Value, 2> &sample) {
    // Low-distortion warping technique based on concentric disk mapping
    Point<Value, 2> p = square_to_uniform_disk_concentric(sample);

    // Guard against numerical imprecisions
    Value z = dr::safe_sqrt(1.f - dr::squared_norm(p));

    return { p.x(), p.y(), z };
}

/// Inverse of the mapping \ref square_to_cosine_hemisphere
template <typename Value>
MI_INLINE Point<Value, 2> cosine_hemisphere_to_square(const Vector<Value, 3> &v) {
    return uniform_disk_to_square_concentric(Point<Value, 2>(v.x(), v.y()));
}

/// Density of \ref square_to_cosine_hemisphere() with respect to solid angles
template <bool TestDomain = false, typename Value>
MI_INLINE Value square_to_cosine_hemisphere_pdf(const Vector<Value, 3> &v) {
    if constexpr (TestDomain)
        return dr::select(dr::abs(dr::squared_norm(v) - 1.f) > math::RayEpsilon<Value> ||
                          v.z() < 0.f, 0.f, dr::InvPi<Value> * v.z());
    else
        return dr::InvPi<Value> * v.z();
}

/**
 * \brief Importance sample a linear interpolant
 *
 * Given a linear interpolant on the unit interval with boundary values \c v0,
 * \c v1 (where \c v1 is the value at <tt>x=1</tt>), warp a uniformly
 * distributed input sample \c sample so that the resulting probability
 * distribution matches the linear interpolant.
 */
template <typename Value>
MI_INLINE Value interval_to_linear(Value v0, Value v1, Value sample) {
    return dr::select(
        dr::abs(v0 - v1) > 1e-4f * (v0 + v1),
        (v0 - dr::safe_sqrt(dr::lerp(dr::sqr(v0), dr::sqr(v1), sample))) / (v0 - v1),
        sample
    );
}

/// Inverse of \ref interval_to_linear
template <typename Value>
MI_INLINE Value linear_to_interval(Value v0, Value v1, Value sample) {
    return dr::select(
        dr::abs(v0 - v1) > 1e-4f * (v0 + v1),
        sample * ((2.f - sample) * v0 + sample * v1) / (v0 + v1),
        sample
    );
}

/**
 * \brief Importance sample a bilinear interpolant
 *
 * Given a bilinear interpolant on the unit square with corner values \c v00,
 * \c v10, \c v01, \c v11 (where \c v10 is the value at (x,y) == (0, 0)), warp
 * a uniformly distributed input sample \c sample so that the resulting
 * probability distribution matches the linear interpolant.
 *
 * The implementation first samples the marginal distribution to obtain \c y,
 * followed by sampling the conditional distribution to obtain \c x.
 *
 * Returns the sampled point and PDF for convenience.
 */
template <typename Value>
MI_INLINE std::pair<Point<Value, 2>, Value>
square_to_bilinear(Value v00, Value v10, Value v01, Value v11,
                   Point<Value, 2> sample) {
    using Mask = dr::mask_t<Value>;

    // Invert marginal CDF in the 'y' parameter
    Value r0 = v00 + v10, r1 = v01 + v11;
    sample.y() = interval_to_linear(r0, r1, sample.y());

    // Invert conditional CDF in the 'y' parameter
    Value c0 = dr::lerp(v00, v01, sample.y()),
          c1 = dr::lerp(v10, v11, sample.y());
    sample.x() = interval_to_linear(c0, c1, sample.x());

    return { sample, dr::lerp(c0, c1, sample.x()) };
}

/// Inverse of \ref square_to_bilinear
template <typename Value>
MI_INLINE std::pair<Point<Value, 2>, Value>
bilinear_to_square(Value v00, Value v10, Value v01, Value v11,
                   Point<Value, 2> sample) {
    using Mask = dr::mask_t<Value>;

    Value r0  = v00 + v10,
          r1  = v01 + v11,
          c0  = dr::lerp(v00, v01, sample.y()),
          c1  = dr::lerp(v10, v11, sample.y()),
          pdf = dr::lerp(c0, c1, sample.x());

    sample.x() = linear_to_interval(c0, c1, sample.x());
    sample.y() = linear_to_interval(r0, r1, sample.y());

    return { sample, pdf };
}

template <typename Value> MI_INLINE Value
square_to_bilinear_pdf(Value v00, Value v10, Value v01, Value v11,
                       const Point<Value, 2> &sample) {
    return dr::lerp(dr::lerp(v00, v10, sample.x()),
                    dr::lerp(v01, v11, sample.x()),
                    sample.y());
}

// =======================================================================

/**
 * \brief Uniformly sample a vector that lies within a given
 * cone of angles around the Z axis
 *
 * \param cos_cutoff Cosine of the cutoff angle
 * \param sample A uniformly distributed sample on \f$[0,1]^2\f$
 */
template <typename Value>
MI_INLINE Vector<Value, 3> square_to_uniform_cone(const Point<Value, 2> &sample,
                                                   const Value &cos_cutoff) {
#if 0
    // Approach 1: warping method based on standard disk mapping
    Value cos_theta = (1.f - sample.y()) + sample.y() * cos_cutoff,
          sin_theta = circ(cos_theta);

    auto [s, c] = dr::sincos(dr::TwoPi<Value> * sample.x());
    return { c * sin_theta, s * sin_theta, cos_theta };
#else
    // Approach 2: low-distortion warping technique based on concentric disk mapping
    Value one_minus_cos_cutoff(1.f - cos_cutoff);
    Point<Value, 2> p = square_to_uniform_disk_concentric(sample);
    Value pn = dr::squared_norm(p);
    Value z = cos_cutoff + one_minus_cos_cutoff * (1.f - pn);
    p *= dr::safe_sqrt(one_minus_cos_cutoff * (2.f - one_minus_cos_cutoff * pn));
    return { p.x(), p.y(), z };
#endif
}

/// Inverse of the mapping \ref square_to_uniform_cone
template <typename Value>
MI_INLINE Point<Value, 2> uniform_cone_to_square(const Vector<Value, 3> &v,
                                                  const Value &cos_cutoff) {
    Point<Value, 2> p = Point<Value, 2>(v.x(), v.y());
    p *= dr::sqrt((1.f - v.z()) / (dr::squared_norm(p) * (1.f - cos_cutoff)));
    return uniform_disk_to_square_concentric(p);
}

/**
 * \brief Density of \ref square_to_uniform_cone per unit area.
 *
 * \param cos_cutoff Cosine of the cutoff angle
 */
template <bool TestDomain = false, typename Value>
MI_INLINE Value square_to_uniform_cone_pdf(const Vector<Value, 3> &v,
                                            const Value &cos_cutoff) {
    DRJIT_MARK_USED(v);
    if constexpr (TestDomain)
        return dr::select(dr::abs(dr::squared_norm(v) - 1.f) > math::RayEpsilon<Value> ||
                          v.z() < cos_cutoff,
                          0.f, dr::InvTwoPi<Value> / (1.f - cos_cutoff));
    else
        return dr::InvTwoPi<Value> / (1.f - cos_cutoff);
}

// =======================================================================

/// Warp a uniformly distributed square sample to a Beckmann distribution
template <typename Value>
MI_INLINE Vector<Value, 3> square_to_beckmann(const Point<Value, 2> &sample,
                                               const Value &alpha) {
#if 0
    // Approach 1: warping method based on standard disk mapping
    auto [s, c] = dr::sincos(dr::TwoPi<Value> * sample.x());

    Value tan_theta_m_sqr = -dr::sqr(alpha) * dr::log(1.f - sample.y());
    Value cos_theta_m = dr::rsqrt(1 + tan_theta_m_sqr),
          sin_theta_m = circ(cos_theta_m);

    return { sin_theta_m * c, sin_theta_m * s, cos_theta_m };
#else
    // Approach 2: low-distortion warping technique based on concentric disk mapping
    Point<Value, 2> p = square_to_uniform_disk_concentric(sample);
    Value r2 = dr::squared_norm(p);

    Value tan_theta_m_sqr = -dr::sqr(alpha) * dr::log(1.f - r2);
    Value cos_theta_m = dr::rsqrt(1.f + tan_theta_m_sqr);
    p *= dr::safe_sqrt(dr::fnmadd(cos_theta_m, cos_theta_m, 1.f) / r2);

    return { p.x(), p.y(), cos_theta_m };
#endif
}

/// Inverse of the mapping \ref square_to_uniform_cone
template <typename Value>
MI_INLINE Point<Value, 2> beckmann_to_square(const Vector<Value, 3> &v, const Value &alpha) {
    Point<Value, 2> p(v.x(), v.y());
    Value tan_theta_m_sqr = dr::rcp(dr::sqr(v.z())) - 1.f;

    Value r2 = 1.f - dr::exp(tan_theta_m_sqr * (-1.f / dr::sqr(alpha)));

    p *= dr::safe_sqrt(r2 / (1.f - dr::sqr(v.z())));

    return uniform_disk_to_square_concentric(p);
}

/// Probability density of \ref square_to_beckmann()
template <typename Value>
MI_INLINE Value square_to_beckmann_pdf(const Vector<Value, 3> &m,
                                        const Value &alpha) {
    using Frame = Frame<Value>;

    Value temp = Frame::tan_theta(m) / alpha,
          ct   = Frame::cos_theta(m);

    Value result = dr::exp(-dr::sqr(temp)) / (dr::Pi<Value> * dr::sqr(alpha * ct) * ct);

    return dr::select(ct < 1e-9f, 0.f, result);
}

// =======================================================================

/// Warp a uniformly distributed square sample to a von Mises Fisher distribution
template <typename Value>
MI_INLINE Vector<Value, 3> square_to_von_mises_fisher(const Point<Value, 2> &sample,
                                                       const Value &kappa) {
#if 1
    // Approach 1: warping method based on standard disk mapping

    #if 0
        /* Approach 1.1: standard inversion method algorithm for sampling the
           von Mises Fisher distribution (numerically unstable!) */
        Value cos_theta = dr::log(dr::exp(-kappa) + 2.f *
                              sample.y() * dr::sinh(kappa)) / kappa;
#else
        /* Approach 1.2: stable algorithm for sampling the von Mises Fisher
           distribution https://www.mitsuba-renderer.org/~wenzel/files/vmf.pdf */
        Value sy = dr::maximum(1.f - sample.y(), 1e-6f);
        Value cos_theta = 1.f +
            dr::log(dr::fmadd(1.f - sy, dr::exp(-2.f * kappa), sy)) / kappa;
#endif

    auto [s, c] = dr::sincos(dr::TwoPi<Value> * sample.x());
    Value sin_theta = dr::safe_sqrt(1.f - dr::sqr(cos_theta));
    Vector<Value, 3> result = { c * sin_theta, s * sin_theta, cos_theta };
#else
    // Approach 2: low-distortion warping technique based on concentric disk mapping
    Point<Value, 2> p = square_to_uniform_disk_concentric(sample);

    Value r2 = dr::squared_norm(p),
          sy = dr::maximum(1.f - r2, 1e-6f),
          cos_theta = 1.f + dr::log(sy + (1.f - sy) * dr::exp(-2.f * kappa)) / kappa;

    p *= dr::safe_sqrt((1.f - dr::sqr(cos_theta)) / r2);

    Vector<Value, 3> result = { p.x(), p.y(), cos_theta };
#endif

    dr::masked(result, dr::eq(kappa, 0.f)) = square_to_uniform_sphere(sample);

    return result;
}

/// Inverse of the mapping \ref von_mises_fisher_to_square
template <typename Value>
MI_INLINE Point<Value, 2> von_mises_fisher_to_square(const Vector<Value, 3> &v,
                                                      Value kappa) {
    Value expm2k = dr::exp(-2.f * kappa),
          t      = dr::exp((v.z() - 1.f) * kappa);

#if 0
    Value sy     = (expm2k - t) / (expm2k - 1.f),
          r2     = 1.f - sy;

    Point<Value, 2> p(v.x(), v.y());
    return uniform_disk_to_square_concentric(p * dr::safe_sqrt(r2 / dr::squared_norm(p)));
#else
    Value x = dr::atan2(v.y(), v.x()) * dr::InvTwoPi<Value>;
    dr::masked(x, x < 0.f) += 1.f;

    return { x, (1.f - t) / (1.f - expm2k) };
#endif
}

/// Probability density of \ref square_to_von_mises_fisher()
template <typename Value>
MI_INLINE Value square_to_von_mises_fisher_pdf(const Vector<Value, 3> &v, Value kappa) {
    /* Stable algorithm for evaluating the von Mises Fisher distribution
       https://www.mitsuba-renderer.org/~wenzel/files/vmf.pdf */

    Value res = dr::exp(kappa * (v.z() - 1.f)) * (kappa * dr::InvTwoPi<Value>) /
                (1.f - dr::exp(-2.f * kappa));
    return dr::select(kappa > 0, res, dr::InvFourPi<Value>);
}

// =======================================================================

/// Warp a uniformly distributed square sample to a rough fiber distribution
template <typename Value>
Vector<Value, 3> square_to_rough_fiber(const Point<Value, 3> &sample,
                                       const Vector<Value, 3> &wi_,
                                       const Vector<Value, 3> &tangent,
                                       dr::scalar_t<Value> kappa) {
    using Point2  = Point<Value, 2>;
    using Vector3 = Vector<Value, 3>;
    using Frame3  = Frame<Value>;

    Frame3 tframe(tangent);

    // Convert to local coordinate frame with Z = fiber tangent
    Vector3 wi = tframe.to_local(wi_);

    // Sample a point on the reflection cone
    auto [s, c] = dr::sincos(dr::TwoPi<Value> * sample.x());

    Value cos_theta = wi.z(),
          sin_theta = circ(cos_theta);

    Vector3 wo(c * sin_theta, s * sin_theta, -cos_theta);

    // Sample a roughness perturbation from a vMF distribution
    Vector3 perturbation =
        square_to_von_mises_fisher(Point2(sample.y(), sample.z()), Value(kappa));

    // Express perturbation relative to 'wo'
    wo = Frame3(wo).to_world(perturbation);

    // Back to global coordinate frame
    return tframe.to_world(wo);
}

/* Numerical approximations for the modified Bessel function
   of the first kind (I0) and its logarithm */
namespace detail {
    template <typename Value>
    Value i0(Value x) {
        Value result = 1.f, x2 = x * x, xi = x2;
        Value denom = 4.f;
        for (int i = 1; i <= 10; ++i) {
            Value factor = i + 1.f;
            result += xi / denom;
            xi *= x2;
            denom *= 4.f * dr::sqr(factor);
        }
        return result;
    }

    template <typename Value>
    Value log_i0(Value x) {
        return dr::select(x > 12.f,
                      x + 0.5f * (dr::log(dr::rcp(dr::TwoPi<Value> * x)) + dr::rcp(8.f * x)),
                      dr::log(i0(x)));
    }
}

/// Probability density of \ref square_to_rough_fiber()
template <typename Value, typename Vector3 = Vector<Value, 3>>
Value square_to_rough_fiber_pdf(const Vector3 &v, const Vector3 &wi, const Vector3 &tangent,
                                dr::scalar_t<Value> kappa) {
    /**
     * Analytic density function described in "An Energy-Conserving Hair Reflectance Model"
     * by Eugene d’Eon, Guillaume Francois, Martin Hill, Joe Letteri, and Jean-Marie Aubry
     *
     * Includes modifications for numerical robustness described here:
     * https://publons.com/publon/2803
     */

    Value cos_theta_i = dr::dot(wi, tangent),
          cos_theta_o = dr::dot(v, tangent),
          sin_theta_i = circ(cos_theta_i),
          sin_theta_o = circ(cos_theta_o);

    Value c = cos_theta_i * cos_theta_o * kappa,
          s = sin_theta_i * sin_theta_o * kappa;

    if (kappa > 10.f)
        return dr::exp(-c + detail::log_i0(s) - kappa + 0.6931f + dr::log(.5f * kappa)) * dr::InvTwoPi<Value>;
    else
        return dr::exp(-c) * detail::i0(s) * kappa / (2.f * dr::sinh(kappa)) * dr::InvTwoPi<Value>;
}

//! @}
// =======================================================================

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
