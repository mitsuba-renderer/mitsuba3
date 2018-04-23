#pragma once

#include <mitsuba/core/fwd.h>
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

/// Uniformly sample a vector on a 2D disk
template <typename Point2>
MTS_INLINE Point2 square_to_uniform_disk(Point2 sample) {
    using Scalar = scalar_t<Point2>;
    auto r = sqrt(sample.y());
    auto sc = sincos(Scalar(2 * math::Pi) * sample.x());
    return Point2(sc.second * r, sc.first * r);
}

/// Inverse of the mapping \ref square_to_uniform_disk
template <typename Point2>
MTS_INLINE Point2 uniform_disk_to_square(Point2 p) {
    using Scalar = scalar_t<Point2>;
    auto phi = atan2(p.y(), p.x()) * Scalar(math::InvTwoPi);
    return Point2(
        select(phi < Scalar(0), phi + Scalar(1), phi),
        squared_norm(p)
    );
}

/// Density of \ref square_to_uniform_disk per unit area
template <bool TestDomain = false, typename Point2, typename Value = value_t<Point2>>
MTS_INLINE Value square_to_uniform_disk_pdf(Point2 p) {
    using Scalar = scalar_t<Point2>;
    if (TestDomain)
        return select(squared_norm(p) > Scalar(1),
                      zero<Value>(), Value(Scalar(math::InvPi)));
    else
        return Value(Scalar(math::InvPi));
}

// =======================================================================

/// Low-distortion concentric square to disk mapping by Peter Shirley
template <typename Point2>
MTS_INLINE Point2 square_to_uniform_disk_concentric(Point2 sample) {
    using Value  = value_t<Point2>;
    using Mask   = mask_t<Value>;
    using Scalar = scalar_t<Value>;

    Value x = Scalar(2) * sample.x() - Scalar(1);
    Value y = Scalar(2) * sample.y() - Scalar(1);

    /* Modified concencric map code with less branching (by Dave Cline), see
       http://psgraphics.blogspot.ch/2011/01/improved-code-for-concentric-map.html

      Original non-vectorized version:

        Value phi, r;
        if (x == 0 && y == 0) {
            r = phi = 0;
        } else if (x * x > y * y) {
            r = x;
            phi = (math::Pi / 4.0f) * (y / x);
        } else {
            r = y;
            phi = (math::Pi / 2.0f) - (x / y) * (math::Pi / 4.0f);
        }
    */

    Mask is_zero = eq(x, zero<Value>()) && eq(y, zero<Value>());

    Mask quadrant_0_or_2 = (abs(x) > abs(y));

    Value r  = select(quadrant_0_or_2, x, y),
          rp = select(quadrant_0_or_2, y, x);

    Value phi = rp / r * Scalar(0.25f * math::Pi);
    phi = select(quadrant_0_or_2, phi, Scalar(0.5f * math::Pi) - phi);
    phi = select(is_zero, zero<Value>(), phi);

    auto sc = sincos(phi);

    return Point2(r * sc.second,
                  r * sc.first);
}

/// Inverse of the mapping \ref square_to_uniform_disk_concentric
template <typename Point2>
MTS_INLINE Point2 uniform_disk_to_square_concentric(Point2 p) {
    using Value  = value_t<Point2>;
    using Mask   = mask_t<Value>;
    using Scalar = scalar_t<Value>;

    Mask quadrant_0_or_2 = (abs(p.x()) > abs(p.y()));
    Value r_sign = select(quadrant_0_or_2, p.x(), p.y());
    Value r = copysign(norm(p), r_sign);

    Value phi = atan2(mulsign(p.y(), r_sign),
                      mulsign(p.x(), r_sign));

    Value t = phi * Scalar(4 / math::Pi);
    t = select(quadrant_0_or_2, t, Scalar(2) - t) * r;

    Value a = select(quadrant_0_or_2, r, t);
    Value b = select(quadrant_0_or_2, t, r);

    return Point2((a + Scalar(1)) * Scalar(.5),
                  (b + Scalar(1)) * Scalar(.5));
}

/// Density of \ref square_to_uniform_disk per unit area
template <bool TestDomain = false, typename Point2, typename Value = value_t<Point2>>
MTS_INLINE Value square_to_uniform_disk_concentric_pdf(Point2 p) {
    using Scalar = scalar_t<Point2>;
    if (TestDomain)
        return select(squared_norm(p) > Scalar(1),
                      zero<Value>(), Value(Scalar(math::InvPi)));
    else
        return Value(Scalar(math::InvPi));
}

// =======================================================================

/// Convert an uniformly distributed square sample into barycentric coordinates
template <typename Point2>
MTS_INLINE Point2 square_to_uniform_triangle(Point2 sample) {
    using Scalar = scalar_t<Point2>;
    auto t = safe_sqrt(Scalar(1) - sample.x());
    return Point2(Scalar(1) - t, t * sample.y());
}

/// Inverse of the mapping \ref square_to_uniform_triangle
template <typename Point2>
MTS_INLINE Point2 uniform_triangle_to_square(Point2 p) {
    using Scalar = scalar_t<Point2>;

    auto t = Scalar(1) - p.x();
    return Point2(Scalar(1) - t * t, p.y() / t);
}

/// Density of \ref square_to_uniform_triangle per unit area.
template <bool TestDomain = false, typename Point2, typename Value = value_t<Point2>>
MTS_INLINE Value square_to_uniform_triangle_pdf(Point2 p) {
    using Scalar = scalar_t<Point2>;
    if (TestDomain) {
        return select(
            (p.x() < zero<Value>()) || (p.y() < zero<Value>())
                                    || (p.x() + p.y() > Scalar(1)),
            zero<Value>(),
            Value(Scalar(2.f))
        );
    }
    else
        return Value(Scalar(2.0f));
}

// =======================================================================

/// Sample a point on a 2D standard normal distribution. Internally uses the Box-Muller transformation
template <typename Point2>
MTS_INLINE Point2 square_to_std_normal(Point2 sample) {
    using Scalar = scalar_t<Point2>;

    auto r   = sqrt(-Scalar(2) * log(Scalar(1) - sample.x())),
         phi = Scalar(2 * math::Pi) * sample.y();

    auto sc = sincos(phi);
    return Point2(sc.second, sc.first) * r;
}

template <typename Point2, typename Value = value_t<Point2>>
MTS_INLINE Value square_to_std_normal_pdf(Point2 p) {
    using Scalar = scalar_t<Point2>;
    return Scalar(math::InvTwoPi) *
           exp((p.x() * p.x() + p.y() * p.y()) * Scalar(-0.5));
}

// =======================================================================

/// Warp a uniformly distributed sample on [0, 1] to a tent distribution
template <typename Value>
Value interval_to_tent(Value sample) {
    using Scalar = scalar_t<Value>;

    sample -= Scalar(0.5);
    Value abs_sample = abs(sample);

    return copysign(
        Scalar(1) - safe_sqrt(Scalar(1) - (abs_sample + abs_sample)),
        sample
    );
}

/// Warp a uniformly distributed sample on [0, 1] to a tent distribution
template <typename Value>
Value tent_to_interval(Value value) {
    using Scalar = scalar_t<Value>;
    return Scalar(0.5) * (Scalar(1) + value * (Scalar(2) - abs(value)));
}

/// Warp a uniformly distributed sample on [0, 1] to a nonuniform tent distribution with nodes <tt>{a, b, c}</tt>
template <typename Value>
Value interval_to_nonuniform_tent(Value a, Value b, Value c, Value sample) {
    using Scalar = scalar_t<Value>;
    auto mask = (sample * (c - a) < b - a);
    Value factor = select(mask, a - b, c - b);
    sample = select(mask, sample * ((a - c) / (a - b)),
                    ((a - c) / (b - c)) * (sample - ((a - b) / (a - c))));
    return b + factor * (Scalar(1) - safe_sqrt(sample));
}

// =======================================================================

/// Warp a uniformly distributed square sample to a 2D tent distribution
template <typename Point2>
Point2 square_to_tent(Point2 sample) {
    return Point2(interval_to_tent(sample.x()),
                  interval_to_tent(sample.y()));
}

/// Warp a uniformly distributed square sample to a 2D tent distribution
template <typename Point2>
Point2 tent_to_square(Point2 p) {
    return Point2(tent_to_interval(p.x()),
                  tent_to_interval(p.y()));
}

/// Density of \ref square_to_tent per unit area.
template <typename Point2, typename Value = value_t<Point2>>
Value square_to_tent_pdf(Point2 p) {
    using Scalar = scalar_t<Point2>;
    p = abs(p);

    return select(p.x() <= 1 && p.y() <= 1,
                  (Scalar(1) - p.x()) * (Scalar(1) - p.y()),
                  zero<Value>());
}

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Warping techniques related to spheres and subsets
// =======================================================================

/// Uniformly sample a vector on the unit sphere with respect to solid angles
template <typename Point2>
MTS_INLINE vector3_t<Point2> square_to_uniform_sphere(Point2 sample) {
    using Scalar = scalar_t<Point2>;
    auto z = Scalar(1) - Scalar(2) * sample.y();
    auto r = safe_sqrt(Scalar(1) - z * z);
    auto sc = sincos(Scalar(2 * math::Pi) * sample.x());
    return vector3_t<Point2>(r * sc.second, r * sc.first, z);
}

/// Inverse of the mapping \ref square_to_uniform_sphere
template <typename Vector3, typename Point2 = point2_t<Vector3>>
MTS_INLINE Point2 uniform_sphere_to_square(Vector3 p) {
    using Scalar = scalar_t<Vector3>;
    auto phi = atan2(p.y(), p.x()) * Scalar(math::InvTwoPi);
    return Point2(
        select(phi < Scalar(0), phi + Scalar(1), phi),
        (Scalar(1) - p.z()) * Scalar(0.5)
    );
}

/// Density of \ref square_to_uniform_sphere() with respect to solid angles
template <bool TestDomain = false, typename Vector3, typename Value = value_t<Vector3>>
MTS_INLINE Value square_to_uniform_sphere_pdf(Vector3 v) {
    using Scalar = scalar_t<Vector3>;

    if (TestDomain)
        return select(abs(squared_norm(v) - Scalar(1)) > Scalar(math::Epsilon),
                      zero<Value>(), Value(Scalar(math::InvFourPi)));
    else
        return Value(Scalar(math::InvFourPi));
}

// =======================================================================

/// Uniformly sample a vector on the unit hemisphere with respect to solid angles
template <typename Point2, typename Vector3 = vector3_t<Point2>>
MTS_INLINE Vector3 square_to_uniform_hemisphere(Point2 sample) {
    using Scalar = scalar_t<Point2>;
#if 0
    /* Approach 1: warping method based on standard disk mapping */
    auto z = sample.y();
    auto tmp = safe_sqrt(Scalar(1) - z*z);
    auto sc = sincos(Scalar(2 * math::Pi) * sample.x());
    return Vector3(sc.second * tmp, sc.first * tmp, z);
#else
    /* Approach 2: low-distortion warping technique based on concentric disk mapping */
    Point2 p = square_to_uniform_disk_concentric(sample);
    auto z = Scalar(1) - squared_norm(p);
    p *= sqrt(z + Scalar(1));
    return Vector3(p.x(), p.y(), z);
#endif
}

/// Inverse of the mapping \ref square_to_uniform_hemisphere
template <typename Vector3, typename Point2 = point2_t<Vector3>>
MTS_INLINE Point2 uniform_hemisphere_to_square(Vector3 v) {
    using Scalar = scalar_t<Vector3>;
    Point2 p = Point2(v.x(), v.y()) * rsqrt(v.z() + Scalar(1));
    return uniform_disk_to_square_concentric(p);
}

/// Density of \ref square_to_uniform_hemisphere() with respect to solid angles
template <bool TestDomain = false, typename Vector3, typename Value = value_t<Vector3>>
MTS_INLINE Value square_to_uniform_hemisphere_pdf(Vector3 v) {
    using Scalar = scalar_t<Vector3>;
    if (TestDomain)
        return select((abs(squared_norm(v) - Scalar(1)) > Scalar(math::Epsilon)) ||
                      (Frame<Vector3>::cos_theta(v) < Scalar(0)), zero<Value>(),
                      Value(Scalar(math::InvTwoPi)));
    else
        return Value(Scalar(math::InvTwoPi));
}

// =======================================================================

/// Sample a cosine-weighted vector on the unit hemisphere with respect to solid angles
template <typename Point2, typename Vector3 = vector3_t<Point2>>
MTS_INLINE Vector3 square_to_cosine_hemisphere(Point2 sample) {
    using Scalar = scalar_t<Vector3>;

    /* Low-distortion warping technique based on concentric disk mapping */
    Point2 p = square_to_uniform_disk_concentric(sample);

    /* Guard against numerical imprecisions */
    auto z = safe_sqrt(Scalar(1) - p.x() * p.x() - p.y() * p.y());

    return Vector3(p.x(), p.y(), z);
}

/// Inverse of the mapping \ref square_to_cosine_hemisphere
template <typename Vector3, typename Point2 = point2_t<Vector3>>
MTS_INLINE Point2 cosine_hemisphere_to_square(Vector3 v) {
    return uniform_disk_to_square_concentric(Point2(v.x(), v.y()));
}

/// Density of \ref square_to_cosine_hemisphere() with respect to solid angles
template <bool TestDomain = false, typename Vector3, typename Value = value_t<Vector3>>
MTS_INLINE Value square_to_cosine_hemisphere_pdf(Vector3 v) {
    using Scalar = scalar_t<Vector3>;

    if (TestDomain)
        return select((abs(squared_norm(v) - Scalar(1)) > math::Epsilon) ||
                      (Frame<Vector3>::cos_theta(v) < Scalar(0)), zero<Value>(),
                      Scalar(math::InvPi) * Frame<Vector3>::cos_theta(v));
    else
        return Scalar(math::InvPi) * Frame<Vector3>::cos_theta(v);
}

// =======================================================================

/**
 * \brief Uniformly sample a vector that lies within a given
 * cone of angles around the Z axis
 *
 * \param cos_cutoff Cosine of the cutoff angle
 * \param sample A uniformly distributed sample on \f$[0,1]^2\f$
 */
template <typename Point2,
          typename Vector3 = vector3_t<Point2>,
          typename Value   = value_t<Point2>>
MTS_INLINE Vector3 square_to_uniform_cone(Point2 sample, Value cos_cutoff) {
#if 0
    /* Approach 1: warping method based on standard disk mapping */
    auto cos_theta = (1 - sample.y()) + sample.y() * cos_cutoff;
    auto sin_theta = safe_sqrt(1 - cos_theta * cos_theta);

    auto sc = sincos(2 * math::Pi * sample.x());
    return Vector3(sc.second * sin_theta,
                   sc.first * sin_theta,
                   cos_theta);
#else
    /* Approach 2: low-distortion warping technique based on concentric disk mapping */
    Value one_minus_cos_cutoff(1 - cos_cutoff);
    Point2 p = square_to_uniform_disk_concentric(sample);
    auto pn = squared_norm(p);
    auto z = cos_cutoff + one_minus_cos_cutoff * (1 - pn);
    p *= safe_sqrt(one_minus_cos_cutoff * (2 - one_minus_cos_cutoff * pn));
    return Vector3(p.x(), p.y(), z);
#endif
}

/// Inverse of the mapping \ref square_to_uniform_cone
template <typename Vector3,
          typename Point2 = point2_t<Vector3>,
          typename Value = value_t<Point2>>
MTS_INLINE Point2 uniform_cone_to_square(Vector3 v, Value cos_cutoff) {
    Point2 p = Point2(v.x(), v.y());
    p *= sqrt((1 - v.z()) / (squared_norm(p) * (1 - cos_cutoff)));
    return uniform_disk_to_square_concentric(p);
}

/**
 * \brief Density of \ref square_to_uniform_cone per unit area.
 *
 * \param cos_cutoff Cosine of the cutoff angle
 */
template <bool TestDomain = false,
          typename Vector3,
          typename Value = value_t<Vector3>>
MTS_INLINE Value square_to_uniform_cone_pdf(Vector3 v, Value cos_cutoff) {
    if (TestDomain)
        return select((abs(squared_norm(v) - 1) > math::Epsilon) ||
                           (Frame<Vector3>::cos_theta(v) < cos_cutoff),
                      zero<Value>(), Value(math::InvTwoPi / (1 - cos_cutoff)));
    else
        return math::InvTwoPi / (1 - cos_cutoff);
}

// =======================================================================

/// Warp a uniformly distributed square sample to a Beckmann distribution
template <typename Point2,
          typename Vector3 = vector3_t<Point2>,
          typename Value   = value_t<Point2>>
MTS_INLINE Vector3 square_to_beckmann(Point2 sample, Value alpha) {
#if 0
    /* Approach 1: warping method based on standard disk mapping */
    auto sc = sincos(2.0f * math::Pi * sample.x());

    Value tan_theta_m_sqr = -alpha * alpha * log(1 - sample.y());
    Value cos_theta_m = rsqrt(1 + tan_theta_m_sqr);
    Value sin_theta_m = safe_sqrt(1 - cos_theta_m * cos_theta_m);

    return Vector3(sin_theta_m * sc.second, sin_theta_m * sc.first, cos_theta_m);
#else
    /* Approach 2: low-distortion warping technique based on concentric disk mapping */
    Point2 p = square_to_uniform_disk_concentric(sample);
    Value r2 = squared_norm(p);

    Value tan_theta_m_sqr = -alpha * alpha * log(1 - r2);
    Value cos_theta_m = rsqrt(1 + tan_theta_m_sqr);
    p *= safe_sqrt((1 - cos_theta_m * cos_theta_m) / r2);

    return Vector3(p.x(), p.y(), cos_theta_m);
#endif
}

/// Inverse of the mapping \ref square_to_uniform_cone
template <typename Vector3,
          typename Point2 = point2_t<Vector3>,
          typename Value  = value_t<Vector3>>
MTS_INLINE Point2 beckmann_to_square(Vector3 v, Value alpha) {
    Point2 p(v.x(), v.y());
    Value tan_theta_m_sqr = rcp(v.z() * v.z()) - 1;

    Value r2 = 1 - exp(tan_theta_m_sqr * (-1 / (alpha * alpha)));

    p *= safe_sqrt(r2 / ((1 - v.z() * v.z())));

    return uniform_disk_to_square_concentric(p);
}

/// Probability density of \ref square_to_beckmann()
template <typename Vector3, typename Value = value_t<Vector3>>
MTS_INLINE Value square_to_beckmann_pdf(Vector3 m, Value alpha) {
    auto zero_mask = (m.z() < 1e-9f);

    Value temp = Frame<Vector3>::tan_theta(m) / alpha,
          ct   = Frame<Vector3>::cos_theta(m),
          ct2  = ct * ct;

    Value result = exp(-temp * temp) / (math::Pi * alpha * alpha * ct2 * ct);

    return select(zero_mask, zero<Value>(), result);
}

// =======================================================================

/// Warp a uniformly distributed square sample to a von Mises Fisher distribution
template <typename Point2, typename Vector3 = vector3_t<Point2>>
MTS_INLINE Vector3 square_to_von_mises_fisher(Point2 sample, Float kappa) {
    using Value = value_t<Point2>;
    using Scalar = scalar_t<Point2>;

    if (unlikely(kappa == 0))
        return square_to_uniform_sphere(sample);

    assert(kappa > 0.0f);

#if 0
    /* Approach 1: warping method based on standard disk mapping */

    #if 0
        /* Approach 1.1: standard inversion method algorithm for sampling the
           von Mises Fisher distribution (numerically unstable!) */
        Value cos_theta = log(exp(-kappa) + Scalar(2) *
                              sample.y() * sinh(kappa)) / Scalar(kappa);
    #else
        /* Approach 1.2: stable algorithm for sampling the von Mises Fisher
           distribution https://www.mitsuba-renderer.org/~wenzel/files/vmf.pdf */
        Value sy = max(Scalar(1) - sample.y(), Value(1e-6f));
        Value cos_theta = Scalar(1) + log(sy +
            (Scalar(1) - sy) * exp(Scalar(-2) * kappa)) / Scalar(kappa);
    #endif

    auto sc = sincos(2.0f * math::Pi * sample.x());
    Value sin_theta = safe_sqrt(1.0f - cos_theta * cos_theta);
    return select(mask, result, Vector3(sc.second * sin_theta, sc.first * sin_theta, cos_theta));
#else
    /* Approach 2: low-distortion warping technique based on concentric disk mapping */
    Point2 p = square_to_uniform_disk_concentric(sample);

    Value r2 = squared_norm(p);
    Value sy = max(Scalar(1) - r2, Value(Scalar(1e-6)));

    Value cos_theta = Scalar(1) + log(sy +
        (Scalar(1) - sy) * exp(Scalar(-2 * kappa))) / Scalar(kappa);

    p *= safe_sqrt((Scalar(1) - cos_theta * cos_theta) / r2);

    return Vector3(p.x(), p.y(), cos_theta);
#endif
}

/// Inverse of the mapping \ref von_mises_fisher_to_square
template <typename Vector3, typename Point2 = point2_t<Vector3>>
MTS_INLINE Point2 von_mises_fisher_to_square(Vector3 v, Float kappa) {
    using Value = value_t<Point2>;
    using Scalar = scalar_t<Point2>;

    Scalar expm2k = exp(Scalar(-2 * kappa));
    Value t = exp(v.z() * Scalar(kappa) - Scalar(kappa));
    Value sy = (expm2k - t) / (expm2k - Scalar(1));

    Value r2 = Scalar(1) - sy;
    Point2 p = Point2(v.x(), v.y());

    return uniform_disk_to_square_concentric(p * safe_sqrt(r2 / squared_norm(p)));
}

/// Probability density of \ref square_to_von_mises_fisher()
template <typename Vector3, typename Value = value_t<Vector3>>
MTS_INLINE Value square_to_von_mises_fisher_pdf(Vector3 v, Float kappa) {
    using Scalar = scalar_t<Vector3>;

    /* Stable algorithm for evaluating the von Mises Fisher distribution
       https://www.mitsuba-renderer.org/~wenzel/files/vmf.pdf */

    assert(kappa >= 0);
    if (unlikely(kappa == 0))
        return Value(Scalar(math::InvFourPi));
    else
        return exp(kappa * (v.z() - Scalar(1))) * Scalar(kappa * math::InvTwoPi) *
               (Scalar(1) - exp(Scalar(-2 * kappa)));
}

// =======================================================================

/// Warp a uniformly distributed square sample to a rough fiber distribution
template <typename Vector3, typename Point3>
Vector3 square_to_rough_fiber(Point3 sample, Vector3 wi_, Vector3 tangent, Float kappa) {
    using Scalar = value_t<Vector3>;
    using Point2  = Point<Scalar, 2>;

    Frame<Vector3> tframe(tangent);

    /* Convert to local coordinate frame with Z = fiber tangent */
    Vector3 wi = tframe.to_local(wi_);

    /* Sample a point on the reflection cone */
    auto sc = sincos(2.0f * math::Pi * sample.x());

    Scalar cos_theta = wi.z();
    Scalar sin_theta = safe_sqrt(1 - cos_theta * cos_theta);

    Vector3 wo(sc.second * sin_theta, sc.first * sin_theta, -cos_theta);

    /* Sample a roughness perturbation from a vMF distribution */
    Vector3 perturbation =
        square_to_von_mises_fisher(Point2(sample.y(), sample.z()), kappa);

    /* Express perturbation relative to 'wo' */
    wo = Frame<Vector3>(wo).to_world(perturbation);

    /* Back to global coordinate frame */
    return tframe.to_world(wo);
}

/* Numerical approximations for the modified Bessel function
   of the first kind (I0) and its logarithm */
namespace detail {
    template <typename Scalar>
    Scalar i0(Scalar x) {
        Scalar result = Scalar(1.0f), x2 = x * x, xi = x2;
        Scalar denom = Scalar(4.0f);
        for (int i = 1; i <= 10; ++i) {
            Scalar factor = Scalar(i + 1.0f);
            result += xi / denom;
            xi *= x2;
            denom *= 4.0f * factor * factor;
        }
        return result;
    }

    template <typename Scalar>
    Scalar log_i0(Scalar x) {
        return select(x > 12.0f,
                      x + 0.5f * (log(rcp(2.0f * math::Pi * x)) + rcp(8.0f * x)),
                      log(i0(x)));
    }
}

/// Probability density of \ref square_to_rough_fiber()
template <typename Vector3, typename Scalar = value_t<Vector3>>
Scalar square_to_rough_fiber_pdf(Vector3 v, Vector3 wi, Vector3 tangent, Float kappa) {
    /**
     * Analytic density function described in "An Energy-Conserving Hair Reflectance Model"
     * by Eugene d’Eon, Guillaume Francois, Martin Hill, Joe Letteri, and Jean-Marie Aubry
     *
     * Includes modifications for numerical robustness described here:
     * https://publons.com/publon/2803
     */

    Scalar cos_theta_i = dot(wi, tangent);
    Scalar cos_theta_o = dot(v, tangent);
    Scalar sin_theta_i = safe_sqrt(1 - cos_theta_i * cos_theta_i);
    Scalar sin_theta_o = safe_sqrt(1 - cos_theta_o * cos_theta_o);

    Scalar c = cos_theta_i * cos_theta_o * kappa;
    Scalar s = sin_theta_i * sin_theta_o * kappa;

    if (kappa > 10.0f)
        return exp(-c + detail::log_i0(s) - kappa + 0.6931f + log(0.5f * kappa)) * math::InvTwoPi;
    else
        return exp(-c) * detail::i0(s) * kappa / (2.0f * sinh(kappa)) * math::InvTwoPi;
}

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Warping techniques for discrete data
// =======================================================================


/**
 * \brief Implements a hierarchical sample warping scheme for 2D distributions
 * with linear interpolation and an optional dependence on additional parameters
 *
 * This class takes a rectangular floating point array as input and constructs
 * internal data structures to efficiently map uniform variates from the unit
 * square <tt>[0, 1]^2</tt> to a function on <tt>[0, 1]^2</tt> that linearly
 * interpolates the input array.
 *
 * The mapping is constructed from a sequence of <tt>log2(hmax(res))</tt>
 * hierarchical sample warping steps, where <tt>res</tt> is the input array
 * resolution. It is bijective and generally very well-behaved (i.e. low
 * distortion), which makes it an ideal choice for structured point sets such
 * as the Halton or Sobol sequence.
 *
 * The implementation also supports <em>conditional distributions</em>, i.e. 2D
 * distributions that depend on an arbitrary number of parameters (indicated
 * via the \c Dimension template parameter).
 *
 * In this case, the input array should have dimensions <tt>N0 x N1 x ... x Nn
 * x res.x x res.y</tt>, and the <tt>param_res</tt> should be set to <tt>{ N0,
 * N1, ..., Nn }</tt>, and <tt>param_values</tt> should contain the parameter
 * values where the distribution is discretized. Linear interpolation is used
 * when sampling or evaluating the distribution for in-between parameter
 * values.
 *
 * \remark The Python API exposes explicitly instantiated versions of this
 * class named Linear2D0, Linear2D1, and Linear2D2 for data that depends on 0,
 * 1, and 2 parameters, respectively.
 */
template <size_t Dimension = 0>
class Linear2D {
private:
    using FloatStorage = std::unique_ptr<Float[], enoki::aligned_deleter>;

#if !defined(_MSC_VER)
    static constexpr size_t ArraySize = Dimension;
#else
    static constexpr size_t ArraySize = (Dimension != 0) ? Dimension : 1;
#endif

public:
    Linear2D(const Vector2u &size, const Float *data,
             uint32_t param_res[Dimension] = nullptr,
             const Float *param_values[Dimension] = nullptr) {
        if (any(size < 2))
            Throw("warp::Linear2D(): input array resolution must be >= 2!");

        /* The linear interpolant has 'size-1' patches */
        Vector2u patches = size - 1u;

        /* Keep track of the dependence on additional parameters (optional) */
        uint32_t max_level   = math::log2i_ceil(hmax(patches)),
                 slices      = 1u;
        for (int i = (int) Dimension - 1; i >= 0; --i) {
            if (param_res[i] < 2)
                Throw("warp::Linear2D(): parameter resolution must be >= 2!");

            m_param_size[i] = param_res[i];
            m_param_values[i] = FloatStorage(enoki::alloc<Float>(param_res[i]));
            memcpy(m_param_values[i].get(), param_values[i], sizeof(Float) * param_res[i]);
            m_param_strides[i] = slices;
            slices *= m_param_size[i];
        }

        /* Allocate memory for input array and MIP hierarchy */
        m_levels.reserve(max_level + 2);
        m_levels.push_back(Level(size, slices));

        Vector2u level_size = patches;
        for (int level = max_level; level >= 0; --level) {
            level_size += level_size & 1u; // zero-pad
            m_levels.push_back(Level(level_size, slices));
            level_size = sri<1>(level_size);
        }

        for (uint32_t slice = 0; slice < slices; ++slice) {
            uint32_t offset0 = m_levels[0].size * slice,
                     offset1 = m_levels[1].size * slice;

            /* Integrate linear interpolant */
            const Float *in = data + offset0;

            double sum = 0.0;
            for (uint32_t y = 0; y < patches.y(); ++y) {
                for (uint32_t x = 0; x < patches.x(); ++x) {
                    Float avg = (in[0] + in[1] + in[size.x()] +
                                 in[size.x() + 1]) * .25f;
                    sum += (double) avg;
                    *(m_levels[1].ptr(Vector2u(x, y)) + offset1) = avg;
                    ++in;
                }
                ++in;
            }

            /* Copy and normalize fine resolution interpolant */
            Float scale = hprod(patches) / (Float) sum;
            for (uint32_t i = 0; i < m_levels[0].size; ++i)
                m_levels[0].data[offset0 + i] = data[offset0 + i] * scale;
            for (uint32_t i = 0; i < m_levels[1].size; ++i)
                m_levels[1].data[offset1 + i] *= scale;

            /* Build a MIP hierarchy */
            level_size = patches;
            for (uint32_t level = 2; level <= max_level + 1; ++level) {
                const Level &l0 = m_levels[level - 1];
                Level &l1 = m_levels[level];
                offset0 = l0.size * slice;
                offset1 = l1.size * slice;
                level_size = sri<1>(level_size + 1u);

                /* Downsample */
                for (uint32_t y = 0; y < level_size.y(); ++y) {
                    for (uint32_t x = 0; x < level_size.x(); ++x) {
                        Float *d1 = l1.ptr(Vector2u(x, y)) + offset1;
                        const Float *d0 = l0.ptr(Vector2u(x*2, y*2)) + offset0;
                        *d1 = d0[0] + d0[1] + d0[2] + d0[3];
                    }
                }
            }
        }

        m_patch_size = 1.f / patches;
        m_inv_patch_size = patches;
        m_max_patch_index = patches - 1;
    }

    /**
     * \brief Given a uniformly distributed 2D sample, draw a sample from the
     * distribution (parameterized by \c param if applicable)
     *
     * Returns the warped sample and associated probability density.
     */
    template <typename Vector2f, typename Value = value_t<Vector2f>>
    std::pair<Vector2f, Value> sample(Vector2f sample, const Value *param = nullptr,
                                       mask_t<Value> active = true) const {
        using Vector2u = uint32_array_t<Vector2f>;
        using UInt32   = value_t<Vector2u>;

        /* Look up parameter-related indices and weights (if Dimension != 0) */
        Value param_weight[ArraySize];
        UInt32 slice_offset = zero<UInt32>();
        for (size_t dim = 0; dim < Dimension; ++dim) {
            UInt32 param_index = math::find_interval(
                m_param_size[dim],
                [&](UInt32 idx, mask_t<Value> active) {
                    return gather<Value>(m_param_values[dim].get(), idx, active) <= param[dim];
                },
                active
            );

            Value p0 = gather<Value>(m_param_values[dim].get(), param_index, active),
                  p1 = gather<Value>(m_param_values[dim].get(), param_index + 1, active);

            param_weight[dim] = clamp((param[dim] - p0) / (p1 - p0), 0.f, 1.f);
            slice_offset += m_param_strides[dim] * param_index;
        }

        /* Hierarchical sample warping */
        Vector2u offset = zero<Vector2u>();
        for (uint32_t l = (uint32_t) m_levels.size() - 2; l != 0; --l) {
            const Level &level = m_levels[l];

            offset = sli<1>(offset);

            /* Fetch values from next MIP level */
            UInt32 offset_i = level.index(offset);
            if (Dimension != 0)
                offset_i += slice_offset * level.size;

            Value v00 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);
            offset_i += 1u;

            Value v10 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);
            offset_i += 1u;

            Value v01 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);
            offset_i += 1u;

            Value v11 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);

            /* Avoid issues with roundoff error */
            sample = clamp(sample, 0.f, 1.f);

            /* Select the row */
            Value r0 = v00 + v10,
                  r1 = v01 + v11;
            sample.y() *= r0 + r1;
            mask_t<Value> mask = sample.y() > r0;
            masked(offset.y(), mask) += 1;
            masked(sample.y(), mask) -= r0;
            sample.y() /= select(mask, r1, r0);

            /* Select the column */
            Value c0 = select(mask, v01, v00),
                  c1 = select(mask, v11, v10);
            sample.x() *= c0 + c1;
            mask = sample.x() > c0;
            masked(sample.x(), mask) -= c0;
            sample.x() /= select(mask, c1, c0);
            masked(offset.x(), mask) += 1;
        }

        const Level &level = m_levels[0];

        UInt32 offset_i = offset.x() + offset.y() * level.width;

        /* Fetch corners of bilinear patch */
        Value v00 = level.template lookup<Dimension>(
            offset_i, m_param_strides, param_weight, active);

        Value v10 = level.template lookup<Dimension>(
            offset_i + 1, m_param_strides, param_weight, active);

        Value v01 = level.template lookup<Dimension>(
            offset_i + level.width, m_param_strides, param_weight, active);

        Value v11 = level.template lookup<Dimension>(
            offset_i + level.width + 1, m_param_strides, param_weight, active);

        Value r0 = v00 + v10,
              r1 = v01 + v11,
              sum  = r0 + r1;

        /* Invert marginal CDF in the 'y' parameter */
        masked(sample.y(), abs(r0 - r1) > 1e-4f * (r0 + r1)) =
            (r0 - sqrt((sqr(r0) + sum * (r1 - r0) * sample.y()))) / (r0 - r1);

        /* Invert conditional CDF in the 'x' parameter */
        Value c0 = fmadd(1.f - sample.y(), v00, sample.y() * v01),
              c1 = fmadd(1.f - sample.y(), v10, sample.y() * v11);

        masked(sample.x(), abs(c0 - c1) > 1e-4f * (c0 + c1)) =
            (c0 - sqrt(sqr(c0) * (1.f - sample.x()) + sqr(c1) * sample.x())) /
            (c0 - c1);

        return {
            (Vector2f(offset) + sample) * m_patch_size,
            fmadd(1.f - sample.x(), c0, sample.x() * c1)
        };
    }

    /// Inverse of the mapping implemented in \c sample()
    template <typename Vector2f, typename Value = value_t<Vector2f>>
    Vector2f invert(Vector2f sample, const Value *param = nullptr,
                    mask_t<Value> active = true) const {
        using Vector2u = uint32_array_t<Vector2f>;
        using Vector2i = int32_array_t<Vector2f>;
        using UInt32   = value_t<Vector2u>;
        using Mask     = mask_t<Value>;

        const Level &level = m_levels[0];

        /* Look up parameter-related indices and weights (if Dimension != 0) */
        Value param_weight[ArraySize];
        UInt32 slice_offset = zero<UInt32>();
        for (size_t dim = 0; dim < Dimension; ++dim) {
            UInt32 param_index = math::find_interval(
                m_param_size[dim],
                [&](UInt32 idx, mask_t<Value> active) {
                    return gather<Value>(m_param_values[dim].get(), idx, active) <= param[dim];
                },
                active
            );

            Value p0 = gather<Value>(m_param_values[dim].get(), param_index, active),
                  p1 = gather<Value>(m_param_values[dim].get(), param_index + 1, active);

            param_weight[dim] = clamp((param[dim] - p0) / (p1 - p0), 0.f, 1.f);
            slice_offset += m_param_strides[dim] * param_index;
        }

        /* Fetch values at corners of bilinear patch */
        sample *= m_inv_patch_size;
        Vector2u offset = min(Vector2u(Vector2i(sample)), m_max_patch_index);
        UInt32 offset_i = offset.x() + offset.y() * level.width;

        Value v00 = level.template lookup<Dimension>(
            offset_i, m_param_strides, param_weight, active);

        Value v10 = level.template lookup<Dimension>(
            offset_i + 1, m_param_strides, param_weight, active);

        Value v01 = level.template lookup<Dimension>(
            offset_i + level.width, m_param_strides, param_weight, active);

        Value v11 = level.template lookup<Dimension>(
            offset_i + level.width + 1, m_param_strides, param_weight, active);

        sample -= Vector2f(Vector2i(offset));

        Value c0 = fmadd(1.f - sample.y(), v00, sample.y() * v01),
              c1 = fmadd(1.f - sample.y(), v10, sample.y() * v11),
              r0 = v00 + v10,
              r1 = v01 + v11;

        masked(sample.x(), abs(c1 - c0) > 1e-4f * (c0 + c1)) *=
            ((2.f * c0) + sample.x() * (c1 - c0)) / (c0 + c1);

        masked(sample.y(), abs(r1 - r0) > 1e-4f * (r0 + r1)) *=
            (2.f * r0 + sample.y() * (r1 - r0)) / (r0 + r1);

        /* Hierarchical sample warping */
        for (uint32_t l = 1; l < (uint32_t) m_levels.size() - 1; ++l) {
            const Level &level = m_levels[l];

            /* Fetch values from next MIP level */
            UInt32 offset_i = level.index(offset & ~1u);
            if (Dimension != 0)
                offset_i += slice_offset * level.size;

            Value v00 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);
            offset_i += 1u;

            Value v10 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);
            offset_i += 1u;

            Value v01 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);
            offset_i += 1u;

            Value v11 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);

            Mask x_mask = neq(offset.x() & 1u, 0u),
                 y_mask = neq(offset.y() & 1u, 0u);

            Value r0 = v00 + v10,
                  r1 = v01 + v11,
                  c0 = select(y_mask, v01, v00),
                  c1 = select(y_mask, v11, v10);

            sample.y() *= select(y_mask, r1, r0);
            masked(sample.y(), y_mask) += r0;
            sample.y() /= r0 + r1;

            sample.x() *= select(x_mask, c1, c0);
            masked(sample.x(), x_mask) += c0;
            sample.x() /= c0 + c1;

            /* Avoid issues with roundoff error */
            sample = clamp(sample, 0.f, 1.f);

            offset = sri<1>(offset);
        }

        return sample;
    }

    /**
     * \brief Evaluate the distribution at position \c pos. The distribution is
     * parameterized by \c param if applicable.
     */
    template <typename Vector2f, typename Value = value_t<Vector2f>>
    Value pdf(Vector2f pos, const Value *param = nullptr,
              mask_t<Value> active = true) const {
        using Vector2u = uint32_array_t<Vector2f>;
        using Vector2i = int32_array_t<Vector2f>;
        using UInt32 = value_t<Vector2u>;

        /* Look up parameter-related indices and weights (if Dimension != 0) */
        Value param_weight[ArraySize];
        UInt32 slice_offset = zero<UInt32>();
        for (size_t dim = 0; dim < Dimension; ++dim) {
            UInt32 param_index = math::find_interval(
                m_param_size[dim],
                [&](UInt32 idx, mask_t<Value> active) {
                    return gather<Value>(m_param_values[dim].get(), idx, active) <= param[dim];
                },
                active
            );

            Value p0 = gather<Value>(m_param_values[dim].get(), param_index, active),
                  p1 = gather<Value>(m_param_values[dim].get(), param_index + 1, active);

            param_weight[dim] = clamp((param[dim] - p0) / (p1 - p0), 0.f, 1.f);
            slice_offset += m_param_strides[dim] * param_index;
        }

        /* Compute linear interpolation weights */
        pos *= m_inv_patch_size;
        Vector2u offset = min(Vector2u(pos), m_max_patch_index);
        Vector2f w1 = pos - Vector2f(Vector2i(offset)),
                 w0 = 1.f - w1;

        const Level &level = m_levels[0];
        UInt32 offset_i = offset.x() + offset.y() * level.width;

        Value v00 = level.template lookup<Dimension>(
            offset_i, m_param_strides, param_weight, active);

        Value v10 = level.template lookup<Dimension>(
            offset_i + 1, m_param_strides, param_weight, active);

        Value v01 = level.template lookup<Dimension>(
            offset_i + level.width, m_param_strides, param_weight, active);

        Value v11 = level.template lookup<Dimension>(
            offset_i + level.width + 1, m_param_strides, param_weight, active);

        return select(all(pos >= 0 && pos <= m_inv_patch_size),
                fmadd(w0.y(),  fmadd(w0.x(), v00, w1.x() * v10),
                      w1.y() * fmadd(w0.x(), v01, w1.x() * v11)), 0.f);
    }

private:
    struct Level {
        uint32_t size;
        uint32_t width;
        FloatStorage data;

        Level() { }
        Level(Vector2u res, uint32_t slices) : size(hprod(res)), width(res.x()) {
            uint32_t alloc_size = size  * slices;
            data = FloatStorage(enoki::alloc<Float>(alloc_size));
            memset(data.get(), 0, alloc_size * sizeof(Float));
        }

        /**
         * \brief Convert from 2D pixel coordinates to an index indicating how the
         * data is laid out in memory.
         *
         * The implementation stores 2x2 patches contigously in memory to
         * improve cache locality during hierarchical traversals
         */
        template <typename Vector2i>
        MTS_INLINE value_t<Vector2i> index(const Vector2i &p) const {
            return ((p.x() & 1u) | sli<1>((p.x() & ~1u) | (p.y() & 1u))) +
                   ((p.y() & ~1u) * width);
        }

        MTS_INLINE Float *ptr(const Vector2i &p) const {
            return data.get() + index(p);
        }

        template <size_t Dim, typename Index, typename Value,
                  std::enable_if_t<Dim != 0, int> = 0>
        MTS_INLINE Value lookup(Index i0,
                                const uint32_t *param_strides,
                                const Value *param_weight,
                                mask_t<Value> active) const {
            Index i1 = i0 + param_strides[Dim - 1] * size;
            Value w1 = param_weight[Dim - 1],
                  w0 = 1.f - w1;

            Value v0 = lookup<Dim - 1>(i0, param_strides, param_weight, active);
            Value v1 = lookup<Dim - 1>(i1, param_strides, param_weight, active);
            return fmadd(v0, w0, v1 * w1);
        }

        template <size_t Dim, typename Index, typename Value,
                  std::enable_if_t<Dim == 0, int> = 0>
        MTS_INLINE Value lookup(Index index, const uint32_t *,
                                const Value *, mask_t<Value> active) const {
            return gather<Value>(data.get(), index, active);
        }
    };

    /// MIP hierarchy over linearly interpolated patches
    std::vector<Level> m_levels;

    /// Size of a bilinear patch in the unit square
    Vector2f m_patch_size;

    /// Inverse of the above
    Vector2f m_inv_patch_size;

    /// Resolution of the fine-resolution PDF data
    Vector2f m_vertex_count;

    /// Number of bilinear patches in the X/Y dimension - 1
    Vector2u m_max_patch_index;

    /// Resolution of each parameter (optional)
    uint32_t m_param_size[ArraySize];

    /// Stride per parameter in units of sizeof(Float)
    uint32_t m_param_strides[ArraySize];

    /// Discretization of each parameter domain
    FloatStorage m_param_values[ArraySize];
};

//! @}
// =======================================================================

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
