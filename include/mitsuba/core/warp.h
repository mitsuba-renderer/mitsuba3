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
template <typename Point2f>
MTS_INLINE Point2f square_to_uniform_disk(Point2f sample) {
    auto r = sqrt(sample.y());
    auto sc = sincos(2.f * math::Pi * sample.x());
    return Point2f(sc.second * r, sc.first * r);
}

/// Density of \ref square_to_uniform_disk per unit area
template <bool TestDomain = false, typename Point2f, typename Scalar = value_t<Point2f>>
MTS_INLINE Scalar square_to_uniform_disk_pdf(Point2f p) {
    if (TestDomain)
        return select(squared_norm(p) > 1, zero<Scalar>(), Scalar(math::InvPi));
    else
        return Scalar(math::InvPi);
}

// =======================================================================

/// Low-distortion concentric square to disk mapping by Peter Shirley (PDF: 1/PI)
template <typename Point2f>
MTS_INLINE Point2f square_to_uniform_disk_concentric(Point2f sample) {
    using Scalar = value_t<Point2f>;
    Scalar r1 = 2.f * sample.x() - 1.f;
    Scalar r2 = 2.f * sample.y() - 1.f;

    /* Modified concencric map code with less branching (by Dave Cline), see
      http://psgraphics.blogspot.ch/2011/01/improved-code-for-concentric-map.html

      Original non-vectorized version:

        Scalar phi, r;
        if (r1 == 0 & r2 == 0) {
            r = phi = 0;
        } else if (r1 * r1 > r2 * r2) {
            r = r1;
            phi = (math::Pi / 4.f) * (r2 / r1);
        } else {
            r = r2;
            phi = (math::Pi / 2.f) - (r1 / r2) * (math::Pi / 4.f);
        }
    */

    auto zmask = eq(r1, zero<Scalar>()) & eq(r2, zero<Scalar>());
    auto mask = r1 * r1 > r2 * r2;

    auto r  = select(mask, r1, r2),
         rp = select(mask, r2, r1);

    auto phi = rp / r * Scalar(math::Pi / 4.f);
    phi = select(mask, phi, (math::Pi / 2.f) - phi);
    phi = select(zmask, zero<Scalar>(), phi);

    auto sc = sincos(phi);
    return Point2f(r * sc.second, r * sc.first);
}

/// Density of \ref square_to_uniform_disk per unit area
template <bool TestDomain = false, typename Point2f, typename Scalar = value_t<Point2f>>
MTS_INLINE Scalar square_to_uniform_disk_concentric_pdf(Point2f p) {
    if (TestDomain)
        return select(squared_norm(p) > 1, zero<Scalar>(), Scalar(math::InvPi));
    else
        return Scalar(math::InvPi);
}

// =======================================================================

/// Convert an uniformly distributed square sample into barycentric coordinates
template <typename Point2f>
MTS_INLINE Point2f square_to_uniform_triangle(Point2f sample) {
    auto a = safe_sqrt(1.f - sample.x());
    return Point2f(1.f - a, a * sample.y());
}

/// Density of \ref square_to_uniform_triangle per unit area.
template <bool TestDomain = false, typename Point2f, typename Scalar = value_t<Point2f>>
MTS_INLINE Scalar square_to_uniform_triangle_pdf(Point2f p) {
    if (TestDomain)
        return select(p.x() < 0.f | p.y() < 0.f | p.x() + p.y() > 1.f,
                      zero<Scalar>(), Scalar(2.f));
    else
        return Scalar(2.f);
}

// =======================================================================

/// Sample a point on a 2D standard normal distribution. Internally uses the Box-Muller transformation
template <typename Point2f>
MTS_INLINE Point2f square_to_std_normal(Point2f sample) {
    auto r   = sqrt(-2.f * log(1.f-sample.x())),
         phi = 2.f * math::Pi * sample.y();
    Point2f result;
    auto sc = sincos(phi);
    return Point2f(sc.second, sc.first) * r;
}

template <typename Point2f, typename Scalar = value_t<Point2f>>
MTS_INLINE Scalar square_to_std_normal_pdf(Point2f p) {
    return math::InvTwoPi * exp((p.x() * p.x() + p.y() * p.y()) * -0.5f);
}

// =======================================================================

/// Warp a uniformly distributed sample on [0, 1] to a tent distribution
template <typename Scalar>
Scalar interval_to_tent(Scalar sample) {
    Scalar sign = select(sample < 0.5f, Scalar(1.f), Scalar(-1.f));
    sample = select(sample < 0.5f, 2.f * sample, 2.f - 2.f * sample);
    return sign * (1.f - safe_sqrt(sample));
}

/// Warp a uniformly distributed sample on [0, 1] to a nonuniform tent distribution with nodes <tt>{a, b, c}</tt>
template <typename Scalar>
Scalar interval_to_nonuniform_tent(Scalar a, Scalar b, Scalar c, Scalar sample) {
    auto mask = sample * (c - a) < b - a;
    Scalar factor = select(mask, a - b, c - b);
    sample = select(mask, sample * (a - c) / (a - b),
                    (a - c) / (b - c) * (sample - (a - b) / (a - c)));
    return b + factor * (1.f - safe_sqrt(sample));
}

// =======================================================================

/// Warp a uniformly distributed square sample to a 2D tent distribution
template <typename Point2f>
Point2f square_to_tent(Point2f sample) {
    return Point2f(interval_to_tent(sample.x()), interval_to_tent(sample.y()));
}

/// Density of \ref square_to_tent per unit area.
template <typename Point2f, typename Scalar = value_t<Point2f>>
Scalar square_to_tent_pdf(Point2f p) {
    return select(p.x() >= -1 & p.x() <= 1 & p.y() >= -1 & p.y() <= 1,
                  (1 - abs(p.x())) * (1 - abs(p.y())),
                  zero<Scalar>());
}

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Warping techniques related to spheres and subsets
// =======================================================================

/// Uniformly sample a vector on the unit sphere with respect to solid angles
template <typename Point2f>
MTS_INLINE vector3_t<Point2f> square_to_uniform_sphere(Point2f sample) {
    auto z = 1.f - 2.f * sample.y();
    auto r = safe_sqrt(1.f - z*z);
    auto sc = sincos(2.f * math::Pi * sample.x());
    return vector3_t<Point2f>(r * sc.second, r * sc.first, z);
}

/// Density of \ref square_to_uniform_sphere() with respect to solid angles
template <bool TestDomain = false, typename Vector3f, typename Scalar = value_t<Vector3f>>
MTS_INLINE Scalar square_to_uniform_sphere_pdf(Vector3f v) {
    if (TestDomain)
        return select(abs(squared_norm(v) - 1.f) > math::Epsilon,
                      Scalar(0.0f), Scalar(math::InvFourPi));
    else
        return math::InvFourPi;
}

// =======================================================================

/// Uniformly sample a vector on the unit hemisphere with respect to solid angles
template <typename Point2f>
MTS_INLINE vector3_t<Point2f> square_to_uniform_hemisphere(Point2f sample) {
#if 0
    /* Approach 1: standard warping method without concentric disk mapping */
    auto z = sample.y();
    auto tmp = safe_sqrt(1.f - z*z);
    auto sc = sincos(2.f * math::Pi * sample.x());
    return vector3_t<Point2f>(sc.second * tmp, sc.first * tmp, z);
#else
    /* Approach 2: low-distortion warping technique based on concentric disk mapping */
    Point2f p = square_to_uniform_disk_concentric(sample);
    auto z = 1 - squared_norm(p);
    p *= sqrt(z + 1);
    return vector3_t<Point2f>(p.x(), p.y(), z);
#endif
}

/// Density of \ref square_to_uniform_hemisphere() with respect to solid angles
template <bool TestDomain = false, typename Vector3f, typename Scalar = value_t<Vector3f>>
MTS_INLINE Scalar square_to_uniform_hemisphere_pdf(Vector3f v) {
    if (TestDomain)
        return select(abs(squared_norm(v) - 1.f) > math::Epsilon | Frame<Vector3f>::cos_theta(v) < 0.f,
                      Scalar(0.0f), Scalar(math::InvTwoPi));
    else
        return math::InvTwoPi;
}

// =======================================================================

/// Sample a cosine-weighted vector on the unit hemisphere with respect to solid angles
template <typename Point2f>
MTS_INLINE vector3_t<Point2f> square_to_cosine_hemisphere(Point2f sample) {
    /* Low-distortion warping technique based on concentric disk mapping */
    Point2f p = square_to_uniform_disk_concentric(sample);

    /* Guard against numerical imprecisions */
    auto z = safe_sqrt(1.f - p.x() * p.x() - p.y() * p.y());

    return vector3_t<Point2f>(p.x(), p.y(), z);
}

/// Density of \ref square_to_cosine_hemisphere() with respect to solid angles
template <bool TestDomain = false, typename Vector3f, typename Scalar = value_t<Vector3f>>
MTS_INLINE Scalar square_to_cosine_hemisphere_pdf(Vector3f v) {
    if (TestDomain)
        return select(abs(squared_norm(v) - 1.f) > math::Epsilon | Frame<Vector3f>::cos_theta(v) < 0.f,
                      zero<Scalar>(), math::InvPi * Frame<Vector3f>::cos_theta(v));
    else
        return math::InvPi * Frame<Vector3f>::cos_theta(v);
}

// =======================================================================

/**
 * \brief Uniformly sample a vector that lies within a given
 * cone of angles around the Z axis
 *
 * \param cos_cutoff Cosine of the cutoff angle
 * \param sample A uniformly distributed sample on \f$[0,1]^2\f$
 */
template <typename Point2f>
MTS_INLINE vector3_t<Point2f> square_to_uniform_cone(Point2f sample, Float cos_cutoff) {
    auto cos_theta = (1.f - sample.y()) + sample.y() * cos_cutoff;
    auto sin_theta = safe_sqrt(1.f - cos_theta * cos_theta);

    auto sc = sincos(2.f * math::Pi * sample.x());
    return vector3_t<Point2f>(sc.second * sin_theta,
                              sc.first * sin_theta,
                              cos_theta);
}

/**
 * \brief Density of \ref square_to_uniform_cone per unit area.
 *
 * \param cos_cutoff Cosine of the cutoff angle
 */
template <bool TestDomain = false, typename Vector3f, typename Scalar = value_t<Vector3f>>
MTS_INLINE Scalar square_to_uniform_cone_pdf(Vector3f v, Float cos_cutoff) {
    if (TestDomain)
        return select(abs(squared_norm(v) - 1.f) > math::Epsilon |
                          Frame<Vector3f>::cos_theta(v) < cos_cutoff,
                      zero<Scalar>(), Scalar(math::InvTwoPi / (1.f - cos_cutoff)));
    else
        return Scalar(math::InvTwoPi / (1.f - cos_cutoff));
}

// =======================================================================

/// Warp a uniformly distributed square sample to a Beckmann distribution *
/// cosine for the given 'alpha' parameter
template <typename Point2f>
MTS_INLINE vector3_t<Point2f> square_to_beckmann(Point2f sample, Float alpha) {
    using Scalar = value_t<Point2f>;
    using Vector3f = vector3_t<Point2f>;

#if 0
    /* Approach 1: standard warping method without concentric disk mapping */
    auto sc = sincos(2.f * math::Pi * sample.x());

    Scalar tan_thetaMSqr = -alpha * alpha * log(1.f - sample.y());
    Scalar cos_thetaM = rsqrt(1.f + tan_thetaMSqr);
    Scalar sin_thetaM = safe_sqrt(1.f - cos_thetaM * cos_thetaM);

    return Vector3f(sin_thetaM * sc.second, sin_thetaM * sc.first, cos_thetaM);
#else
    /* Approach 2: low-distortion warping technique based on concentric disk mapping */
    Point2f p = square_to_uniform_disk_concentric(sample);
    Scalar r2 = squared_norm(p);

    Scalar tan_theta_m_sqr = -alpha * alpha * log(1.f - r2);
    Scalar cos_theta_m = rsqrt(1.f + tan_theta_m_sqr);
    p *= safe_sqrt((1.f - cos_theta_m * cos_theta_m) / r2);

    return Vector3f(p.x(), p.y(), cos_theta_m);
#endif
}

/// Probability density of \ref square_to_beckmann()
template <typename Vector3f, typename Scalar = value_t<Vector3f>>
MTS_INLINE Scalar square_to_beckmann_pdf(Vector3f m, Float alpha) {
    auto mask = m.z() < 1e-9f;

    Scalar temp = Frame<Vector3f>::tan_theta(m) / alpha,
           ct   = Frame<Vector3f>::cos_theta(m),
           ct2  = ct*ct;

    Scalar result = exp(-temp*temp) / (math::Pi * alpha * alpha * ct2 * ct);
    return select(mask, zero<Scalar>(), result);
}

// =======================================================================

/// Warp a uniformly distributed square sample to a von Mises Fisher distribution
template <typename Point2f>
MTS_INLINE vector3_t<Point2f> square_to_von_mises_fisher(Point2f sample, Float kappa) {
    using Scalar = value_t<Point2f>;
    using Vector3f = vector3_t<Point2f>;

    if (unlikely(kappa == 0))
        return square_to_uniform_sphere(sample);

    assert(kappa > 0.f);
#if 0

    /* Approach 1: standard warping method without concentric disk mapping */
    #if 0
        /* Approach 1.1: standard inversion method algorithm for sampling the
           von Mises Fisher distribution (numerically unstable!) */
        Scalar cos_theta = log(exp(-m_kappa) + 2 *
                            sample.y() * sinh(m_kappa)) / m_kappa;
    #else
        /* Approach 1.2: stable algorithm for sampling the von Mises Fisher
           distribution https://www.mitsuba-renderer.org/~wenzel/files/vmf.pdf */
        Scalar sy = max(1.f - sample.y(), Scalar(1e-6f));
        Scalar cos_theta = 1.f + log(sy +
            (1.f - sy) * exp(-2.f * kappa)) / kappa;
    #endif

    auto sc = sincos(2.f * math::Pi * sample.x());
    Scalar sin_theta = safe_sqrt(1.f - cos_theta * cos_theta);
    return select(mask, result, Vector3f(sc.second * sin_theta, sc.first * sin_theta, cos_theta));
#else
    /* Approach 2: low-distortion warping technique based on concentric disk mapping */
    Point2f p = square_to_uniform_disk_concentric(sample);

    Scalar r2 = squared_norm(p);
    Scalar sy = max(1.f - r2, (Scalar) 1e-6f);

    Scalar cos_theta = 1.f + log(sy +
        (1.f - sy) * exp(Scalar(-2.f * kappa))) / kappa;

    p *= safe_sqrt((1.f - cos_theta * cos_theta) / r2);

    return Vector3f(p.x(), p.y(), cos_theta);
#endif
}

/// Probability density of \ref square_to_von_mises_fisher()
template <typename Vector3f, typename Scalar = value_t<Vector3f>>
MTS_INLINE Scalar square_to_von_mises_fisher_pdf(Vector3f v, Float kappa) {
    /* Stable algorithm for evaluating the von Mises Fisher distribution
       https://www.mitsuba-renderer.org/~wenzel/files/vmf.pdf */

    assert(kappa >= 0.f);
    if (unlikely(kappa == 0.f))
        return Scalar(math::InvFourPi);
    else
        return exp(kappa * (v.z() - 1.f)) * kappa
                  / (2.f * math::Pi * (1.f - exp(-2.f * kappa)));
}

// =======================================================================

/// Warp a uniformly distributed square sample to a rough fiber distribution
template <typename Vector3f, typename Point3>
Vector3f square_to_rough_fiber(Point3 sample, Vector3f wi_, Vector3f tangent, Float kappa) {
    using Scalar = value_t<Vector3f>;
    using Point2f  = Point<Scalar, 2>;

    Frame<Vector3f> tframe(tangent);

    /* Convert to local coordinate frame with Z = fiber tangent */
    Vector3f wi = tframe.to_local(wi_);

    /* Sample a point on the reflection cone */
    auto sc = sincos(2.f * math::Pi * sample.x());

    Scalar cos_theta = wi.z();
    Scalar sin_theta = safe_sqrt(1 - cos_theta * cos_theta);

    Vector3f wo(sc.second * sin_theta, sc.first * sin_theta, -cos_theta);

    /* Sample a roughness perturbation from a vMF distribution */
    Vector3f perturbation =
        square_to_von_mises_fisher(Point2f(sample.y(), sample.z()), kappa);

    /* Express perturbation relative to 'wo' */
    wo = Frame<Vector3f>(wo).to_world(perturbation);

    /* Back to global coordinate frame */
    return tframe.to_world(wo);
}

/* Numerical approximations for the modified Bessel function
   of the first kind (I0) and its logarithm */
namespace detail {
    template <typename Scalar>
    Scalar i0(Scalar x) {
        Scalar result = Scalar(1.f), x2 = x * x, xi = x2;
        Scalar denom = Scalar(4.f);
        for (int i = 1; i <= 10; ++i) {
            Scalar factor = Scalar(i + 1.f);
            result += xi / denom;
            xi *= x2;
            denom *= 4.f * factor * factor;
        }
        return result;
    }

    template <typename Scalar>
    Scalar log_i0(Scalar x) {
        return select(x > 12.f,
                      x + 0.5f * (log(rcp(2.f * math::Pi * x)) + rcp(8.f * x)),
                      log(i0(x)));
    }
}

/// Probability density of \ref square_to_rough_fiber()
template <typename Vector3f, typename Scalar = value_t<Vector3f>>
Scalar square_to_rough_fiber_pdf(Vector3f v, Vector3f wi, Vector3f tangent, Float kappa) {
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

    if (kappa > 10.f)
        return exp(-c + detail::log_i0(s) - kappa + 0.6931f + log(kappa / 2.f)) * math::InvTwoPi;
    else
        return exp(-c) * detail::i0(s) * kappa / (2.f * sinh(kappa)) * math::InvTwoPi;
}

//! @}
// =============================================================



#if 0
/*
*   TODO: This function has way too many branches. Need to implement a shorter version
*   similar to David Cline's implementation of \ref square_to_uniform_disk_concentric.
*   Reference:
*   http://psgraphics.blogspot.ch/2011/01/improved-code-for-concentric-map.html
*   https://pdfs.semanticscholar.org/4322/6a3916a85025acbb3a58c17f6dc0756b35ac.pdf
*/
/// Inverse of the mapping \ref square_to_uniform_disk_concentric
template <typename Point2f>
Point2f disk_to_uniform_square_concentric(Point2f p) {
    using Scalar = value_t<Point2f>;
    Scalar r = sqrt(p.x() * p.x() + p.y() * p.y()),
        phi = atan2(p.y(), p.x()),
        a, b;

/*
    Original non - vectorized version :

    if (phi < -math::Pi / 4) {
        phi += 2 * math::Pi;
    }

    if (phi < math::Pi / 4) { //region #1
        a = r;
        b = phi * a / (math::Pi / 4);
    }
    else if (phi < 3 * math::Pi / 4) { //region #2
        b = r;
        a = -(phi - math::Pi / 2) * b / (math::Pi / 4);
    }
    else if (phi < 5 * math::Pi / 4) { //region #3
        a = -r;
        b = (phi - math::Pi) * a / (math::Pi / 4);
    }
    else { //region #4
        b = -r;
        a = -(phi - 3 * math::Pi / 2) * b / (math::Pi / 4);
    }
*/

    Float pi_div_4 = math::Pi / 4.f;
    Float pi_div_2 = math::Pi / 2.f;

    phi = select(phi < -pi_div_4, phi + 2.f * math::Pi, phi);

    auto mask1 = phi < pi_div_4;
    auto mask2 = phi < 3.f * pi_div_4;
    auto mask3 = phi < 5.f * pi_div_4;

    a = select(mask1, r, -(phi - 3 * pi_div_2) * (-r) / pi_div_4);
    a = select(mask2 & !mask1, -(phi - pi_div_2) * r / pi_div_4, a);
    a = select(mask3 & !mask2, -r, a);

    b = select(mask1, phi * r / pi_div_4, -r);
    b = select(mask2 & !mask1, r, b);
    b = select(mask3 & !mask2, (phi - math::Pi) * (-r) / pi_div_4, b);

    return Point2f(0.5f * (a + 1), 0.5f * (b + 1));
}
#endif


NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
