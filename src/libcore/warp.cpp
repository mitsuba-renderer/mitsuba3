#include <mitsuba/core/warp.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(warp)

template MTS_EXPORT_CORE Vector3f  square_to_uniform_sphere(Point2f  sample);
template MTS_EXPORT_CORE Vector3fP square_to_uniform_sphere(Point2fP sample);

template MTS_EXPORT_CORE Float  square_to_uniform_sphere_pdf<true>(Vector3f  sample);
template MTS_EXPORT_CORE FloatP square_to_uniform_sphere_pdf<true>(Vector3fP sample);
template MTS_EXPORT_CORE Float  square_to_uniform_sphere_pdf<false>(Vector3f  sample);
template MTS_EXPORT_CORE FloatP square_to_uniform_sphere_pdf<false>(Vector3fP sample);

#if 0

Vector3f square_to_uniform_hemisphere(const Point2f &sample) {
#if 0
    /* Approach 1: standard warping method without concentric disk mapping */
    Float z = sample.y();
    Float tmp = safe_sqrt(1.f - z*z);
    Float sinPhi, cosPhi;
    math::sincos(2.f * math::Pi * sample.x(), &sinPhi, &cosPhi);
    return Vector3f(cosPhi * tmp, sinPhi * tmp, z);
#else
    /* Approach 2: low-distortion warping technique based on concentric disk mapping */
    Point2f p = square_to_uniform_disk_concentric(sample);
    Float z = 1 - squared_norm(p);
    p *= sqrt(z + 1);
    return Vector3f(p.x(), p.y(), z);
#endif
}

Vector3f square_to_cosine_hemisphere(const Point2f &sample) {
    /* Low-distortion warping technique based on concentric disk mapping */
    Point2f p = square_to_uniform_disk_concentric(sample);
    /* Guard against numerical imprecisions */
    Float z = safe_sqrt(1.f - p.x() * p.x() - p.y() * p.y());

    return Vector3f(p.x(), p.y(), z);
}

Vector3f square_to_uniform_cone(const Point2f &sample, Float cosCutoff) {
    Float cos_theta = (1 - sample.y()) + sample.y() * cosCutoff;
    Float sin_theta = safe_sqrt(1.f - cos_theta * cos_theta);

    auto sc = sincos(2.f * math::Pi * sample.x());
    return Vector3f(sc.second * sin_theta, sc.first * sin_theta, cos_theta);
}

Point2f square_to_uniform_disk(const Point2f &sample) {
    Float r = std::sqrt(sample.y());
    auto sc = sincos(2.f * math::Pi * sample.x());
    return Point2f(sc.second * r, sc.first * r);
}

Point2f square_to_uniform_triangle(const Point2f &sample) {
    Float a = safe_sqrt(1.f - sample.x());
    return Point2f(1 - a, a * sample.y());
}

Point2f square_to_uniform_disk_concentric(const Point2f &sample) {
    using Scalar = Float;

    Float r1 = 2.f * sample.x() - 1.f;
    Float r2 = 2.f * sample.y() - 1.f;

    /* Modified concencric map code with less branching (by Dave Cline), see
      http://psgraphics.blogspot.ch/2011/01/improved-code-for-concentric-map.html

      Original non-vectorized version:

        Float phi, r;
        if (r1 == 0 && r2 == 0) {
            r = phi = 0;
        } else if (r1 * r1 > r2 * r2) {
            r = r1;
            phi = (math::Pi / 4.f) * (r2 / r1);
        } else {
            r = r2;
            phi = (math::Pi / 2.f) - (r1 / r2) * (math::Pi / 4.f);
        }
    */

    auto zmask = eq(r1, zero<Scalar>()) && eq(r2, zero<Scalar>());
    auto mask = r1 * r1 > r2 * r2;

    auto r  = select(mask, r1, r2),
         rp = select(mask, r2, r1);

    auto phi = rp / r * Scalar(math::Pi / 4.f);
    phi = select(mask, phi, (math::Pi / 2.f) - phi);
    phi = select(zmask, zero<Scalar>(), phi);

    auto sc = sincos(phi);
    return Point2f(r * sc.second, r * sc.first);
}

Point2f disk_to_uniform_square_concentric(const Point2f &p) {
    Float r   = sqrt(p.x() * p.x() + p.y() * p.y()),
          phi = atan2(p.y(), p.x()),
          a, b;

    if (phi < -math::Pi/4) {
        /* in range [-pi/4,7pi/4] */
        phi += 2*math::Pi;
    }

    if (phi < math::Pi/4) { /* region 1 */
        a = r;
        b = phi * a / (math::Pi/4);
    } else if (phi < 3*math::Pi/4) { /* region 2 */
        b = r;
        a = -(phi - math::Pi/2) * b / (math::Pi/4);
    } else if (phi < 5*math::Pi/4) { /* region 3 */
        a = -r;
        b = (phi - math::Pi) * a / (math::Pi/4);
    } else { /* region 4 */
        b = -r;
        a = -(phi - 3*math::Pi/2) * b / (math::Pi/4);
    }

    return Point2f(0.5f * (a+1), 0.5f * (b+1));
}

Point2f square_to_std_normal(const Point2f &sample) {
    Float r   = std::sqrt(-2 * std::log(1-sample.x())),
          phi = 2 * math::Pi * sample.y();
    Point2f result;
    auto sc = sincos(phi);
    return Point2f(sc.second, sc.first) * r;
}

Float square_to_std_normalPdf(const Point2f &p) {
    return math::InvTwoPi * std::exp(-(p.x() * p.x() + p.y() * p.y()) / 2.f);
}

static Float interval_to_tent(Float sample) {
    Float sign;

    if (sample < 0.5f) {
        sign = 1;
        sample *= 2;
    } else {
        sign = -1;
        sample = 2 - 2 * sample;
    }

    return sign * (1 - std::sqrt(sample));
}

Point2f square_to_tent(const Point2f &sample) {
    return Point2f(interval_to_tent(sample.x()), interval_to_tent(sample.y()));
}

Float square_to_tent_pdf(const Point2f &p) {
    if (p.x() >= -1 && p.x() <= 1 && p.y() >= -1 && p.y() <= 1)
        return (1 - std::abs(p.x())) * (1 - std::abs(p.y()));
    return 0.f;
}

Float interval_to_nonuniform_tent(Float a, Float b, Float c, Float sample) {
    Float factor;

    if (sample * (c - a) < b - a) {
        factor = a - b;
        sample *= (a - c) / (a - b);
    } else {
        factor = c - b;
        sample = (a - c) / (b - c) * (sample - (a - b) / (a - c));
    }

    return b + factor * (1 - safe_sqrt(sample));
}

Vector3f square_to_beckmann(const Point2f &sample, Float alpha) {
#if 0
    /* Approach 1: standard warping method without concentric disk mapping */
    Float sinPhi, cosPhi;
    math::sincos(2.f * math::Pi * sample.x(), &sinPhi, &cosPhi);

    Float tan_thetaMSqr = -alpha * alpha * std::log(1.f - sample.y());
    Float cos_thetaM = 1.f / std::sqrt(1 + tan_thetaMSqr);
    Float sin_thetaM = safe_sqrt(1.f - cos_thetaM * cos_thetaM);

    return Vector3f(sin_thetaM * cosPhi, sin_thetaM * sinPhi, cos_thetaM);
#else
    /* Approach 2: low-distortion warping technique based on concentric disk mapping */
    Point2f p = square_to_uniform_disk_concentric(sample);
    Float r2 = squared_norm(p);

    Float tan_thetaMSqr = -alpha * alpha * std::log(1 - r2);
    Float cos_thetaM = 1.f / safe_sqrt(1 + tan_thetaMSqr);
    p *= safe_sqrt((1.f - cos_thetaM * cos_thetaM) / r2);

    return Vector3f(p.x(), p.y(), cos_thetaM);
#endif
}

Float square_to_beckmann_pdf(const Vector3f &m, Float alpha) {
    if (m.z() < 1e-9f)
        return 0.f;

    Float temp = Frame3f::tan_theta(m) / alpha,
          ct = Frame3f::cos_theta(m), ct2 = ct*ct;

    return std::exp(-temp*temp)
        / (math::Pi * alpha * alpha * ct2 * ct);
}

Vector3f square_to_von_mises_fisher(const Point2f &sample, Float kappa) {
    if (kappa == 0)
        return square_to_uniform_sphere(sample);

    assert(kappa >= 0.f);
#if 0
    /* Approach 1: standard warping method without concentric disk mapping */
    #if 0
        /* Approach 1.1: standard inversion method algorithm for sampling the
           von Mises Fisher distribution (numerically unstable!) */
        Float cos_theta = std::log(std::exp(-m_kappa) + 2 *
                            sample.y() * std::sinh(m_kappa)) / m_kappa;
    #else
        /* Approach 1.2: stable algorithm for sampling the von Mises Fisher
           distribution https://www.mitsuba-renderer.org/~wenzel/files/vmf.pdf */
        Float sy = std::max(1 - sample.y(), (Float) 1e-6f);
        Float cos_theta = 1 + std::log(sy +
            (1 - sy) * std::exp(-2 * kappa)) / kappa;
    #endif

    Float sinPhi, cosPhi;
    math::sincos(2.f * math::Pi * sample.x(), &sinPhi, &cosPhi);

    Float sin_theta = safe_sqrt(1.f - cos_theta * cos_theta);
    return Vector3f(cosPhi * sin_theta, sinPhi * sin_theta, cos_theta);
#else
    /* Approach 2: low-distortion warping technique based on concentric disk mapping */
    Point2f p = square_to_uniform_disk_concentric(sample);
    Float r2 = squared_norm(p);
    Float sy = std::max(1 - r2, (Float) 1e-6f);

    Float cos_theta = 1 + std::log(sy +
        (1 - sy) * std::exp(-2 * kappa)) / kappa;

    p *= safe_sqrt((1.f - cos_theta * cos_theta) / r2);

    return Vector3f(p.x(), p.y(), cos_theta);
#endif
}

Float square_to_von_mises_fisher_pdf(const Vector3f &v, Float kappa) {
    /* Stable algorithm for evaluating the von Mises Fisher distribution
       https://www.mitsuba-renderer.org/~wenzel/files/vmf.pdf */

    assert(kappa >= 0.f);
    if (kappa == 0.f)
        return math::InvFourPi;
    else
        return std::exp(kappa * (v.z() - 1)) * kappa /
               (2 * math::Pi * (1 - std::exp(-2 * kappa)));
}

Vector3f square_to_rough_fiber(const Point3f &sample, const Vector3f &wi_,
                               const Vector3f &tangent, Float kappa) {
    Frame3f tframe(tangent);

    /* Convert to local coordinate frame with Z = fiber tangent */
    Vector3f wi = tframe.to_local(wi_);

    /* Sample a point on the reflection cone */
    auto sc = sincos(2.f * math::Pi * sample.x());

    Float cos_theta = wi.z();
    Float sin_theta = safe_sqrt(1 - cos_theta * cos_theta);

    Vector3f wo(sc.second * sin_theta, sc.first * sin_theta, -cos_theta);

    /* Sample a roughness perturbation from a vMF distribution */
    Vector3f perturbation =
        square_to_von_mises_fisher(Point2f(sample.y(), sample.z()), kappa);

    /* Express perturbation relative to 'wo' */
    wo = Frame3f(wo).to_world(perturbation);

    /* Back to global coordinate frame */
    return tframe.to_world(wo);
}

/* Numerical approximations for the modified Bessel function
   of the first kind (I0) and its logarithm */
namespace {
    Float i0(Float x) {
        Float result = 1.f, x2 = x * x, xi = x2;
        Float denom = 4.f;
        for (int i = 1; i <= 10; ++i) {
            Float factor = i + 1;
            result += xi / denom;
            xi *= x2;
            denom *= 4.f * factor * factor;
        }
        return result;
    }

    Float logI0(Float x) {
        if (x > 12.f)
            return x + 0.5f * (std::log(1.f / (2 * math::Pi * x)) + 1.f / (8.f * x));
        else
            return std::log(i0(x));
    }
}

Float square_to_rough_fiber_pdf(const Vector3f &v, const Vector3f &wi,
                                const Vector3f &tangent, Float kappa) {
    /**
     * Analytic density function described in "An Energy-Conserving Hair Reflectance Model"
     * by Eugene d’Eon, Guillaume Francois, Martin Hill, Joe Letteri, and Jean-Marie Aubry
     *
     * Includes modifications for numerical robustness described here:
     * https://publons.com/publon/2803
     */

    Float cos_theta_i = dot(wi, tangent);
    Float cos_theta_o = dot(v, tangent);
    Float sin_theta_i = safe_sqrt(1 - cos_theta_i * cos_theta_i);
    Float sin_theta_o = safe_sqrt(1 - cos_theta_o * cos_theta_o);

    Float c = cos_theta_i * cos_theta_o * kappa;
    Float s = sin_theta_i * sin_theta_o * kappa;

    Float result;
    if (kappa > 10.f)
        result = std::exp(-c + logI0(s) - kappa + 0.6931f + std::log(kappa / 2.f));
    else
        result = std::exp(-c) * i0(s) * kappa / (2.f * std::sinh(kappa));

    return result * 1.f / (2.f * math::Pi);
}
#endif

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
