#include <mitsuba/core/warp.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/transform.h>
#include <hypothesis.h>
#include <pcg32.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(warp)

Vector3f squareToUniformSphere(const Point2f &sample) {
    Float z = 1.f - 2.f * sample.y();
    Float r = math::safe_sqrt(1.f - z*z);
    Float sinPhi, cosPhi;
    math::sincos(2.f * math::Pi * sample.x(), &sinPhi, &cosPhi);
    return Vector3f(r * cosPhi, r * sinPhi, z);
}

Vector3f squareToUniformHemisphere(const Point2f &sample) {
    Float z = sample.x();
    Float tmp = math::safe_sqrt(1.f - z*z);

    Float sinPhi, cosPhi;
    math::sincos(2.f * math::Pi * sample.y(), &sinPhi, &cosPhi);

    return Vector3f(cosPhi * tmp, sinPhi * tmp, z);
}

Vector3f squareToCosineHemisphere(const Point2f &sample) {
    Point2f p = squareToUniformDiskConcentric(sample);
    /* Guard against numerical imprecisions */
    Float z = std::max(1e-10f, math::safe_sqrt(1.f - p.x() * p.x() - p.y() * p.y()));

    return Vector3f(p.x(), p.y(), z);
}

Vector3f squareToUniformCone(const Point2f &sample, Float cosCutoff) {
    Float cosTheta = (1 - sample.x()) + sample.x() * cosCutoff;
    Float sinTheta = math::safe_sqrt(1.f - cosTheta * cosTheta);

    Float sinPhi, cosPhi;
    math::sincos(2.f * math::Pi * sample.y(), &sinPhi, &cosPhi);

    return Vector3f(cosPhi * sinTheta, sinPhi * sinTheta, cosTheta);
}

Point2f squareToUniformDisk(const Point2f &sample) {
    Float r = std::sqrt(sample.x());
    Float sinPhi, cosPhi;
    math::sincos(2.f * math::Pi * sample.y(), &sinPhi, &cosPhi);

    return Point2f(cosPhi * r, sinPhi * r);
}

Point2f squareToUniformTriangle(const Point2f &sample) {
    Float a = math::safe_sqrt(1.f - sample.x());
    return Point2f(1 - a, a * sample.y());
}

Point2f squareToUniformDiskConcentric(const Point2f &sample) {
    Float r1 = 2.f * sample.x() - 1.f;
    Float r2 = 2.f * sample.y() - 1.f;

    /* Modified concencric map code with less branching (by Dave Cline), see
     * http://psgraphics.blogspot.ch/2011/01/improved-code-for-concentric-map.html */
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

    Float cosPhi, sinPhi;
    math::sincos(phi, &sinPhi, &cosPhi);

    return Point2f(r * cosPhi, r * sinPhi);
}

Point2f diskToUniformSquareConcentric(const Point2f &p) {
    Float r   = std::sqrt(p.x() * p.x() + p.y() * p.y()),
          phi = std::atan2(p.y(), p.x()),
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

Point2f squareToStdNormal(const Point2f &sample) {
    Float r   = std::sqrt(-2 * std::log(1-sample.x())),
          phi = 2 * math::Pi * sample.y();
    Point2f result;
    math::sincos(phi, &result.y(), &result.x());
    return result * r;
}

Float squareToStdNormalPdf(const Point2f &p) {
    return math::InvTwoPi * std::exp(-(p.x() * p.x() + p.y() * p.y()) / 2.f);
}

static Float intervalToTent(Float sample) {
    Float sign;

    if (sample < 0.5f) {
        sign = 1;
        sample *= 2;
    } else {
        sign = -1;
        sample = 2 * (sample - 0.5f);
    }

    return sign * (1 - std::sqrt(sample));
}

Point2f squareToTent(const Point2f &sample) {
    return Point2f(intervalToTent(sample.x()), intervalToTent(sample.y()));
}

Float squareToTentPdf(const Point2f &p) {
    if (p.x() >= -1 && p.x() <= 1 && p.y() >= -1 && p.y() <= 1)
        return (1 - std::abs(p.x())) * (1 - std::abs(p.y()));

    return 0.0;
}

Float intervalToNonuniformTent(Float a, Float b, Float c, Float sample) {
    Float factor;

    if (sample * (c - a) < b - a) {
        factor = a - b;
        sample *= (a - c) / (a - b);
    } else {
        factor = c - b;
        sample = (a - c) / (b - c) * (sample - (a - b) / (a - c));
    }

    return b + factor * (1 - math::safe_sqrt(sample));
}

Vector3f squareToBeckmann(const Point2f &sample, Float alpha) {
    Float sinPhi, cosPhi;
    math::sincos(2.f * math::Pi * sample.x(), &sinPhi, &cosPhi);

    Float tanThetaMSqr = -alpha * alpha * std::log(1.f - sample.y());
    Float cosThetaM = 1.f / std::sqrt(1 + tanThetaMSqr);
    Float sinThetaM = math::safe_sqrt(1.f - cosThetaM * cosThetaM);

    return Vector3f(sinThetaM * cosPhi, sinThetaM * sinPhi, cosThetaM);
}

Float squareToBeckmannPdf(const Vector3f &m, Float alpha) {
    if (m.z() < 1e-9f)
        return 0.f;

    Float temp = Frame::tanTheta(m) / alpha,
          ct = Frame::cosTheta(m), ct2 = ct*ct;

    return std::exp(-temp*temp)
        / (math::Pi * alpha * alpha * ct2 * ct);
}

Vector3f squareToVonMisesFisher(const Point2f &sample, Float kappa) {
    /* Stable algorithm for sampling the von Mises Fisher distribution
       https://www.mitsuba-renderer.org/~wenzel/files/vmf.pdf */
    assert(kappa >= 0.f);

    Float cosTheta;
    if (kappa == 0.f) {
        cosTheta = 1.f - 2.f * sample.y();
    } else if (sample.y() > 0) {
        cosTheta = 1 + (std::log(sample.y()) + std::log(
            1 - std::exp(-2*kappa) * ((sample.y()-1) / sample.y()))) / kappa;
    } else {
        cosTheta = 1 + std::exp(-2 * kappa) / kappa;
    }

    Float sinPhi, cosPhi;
    math::sincos(2.f * math::Pi * sample.x(), &sinPhi, &cosPhi);

    Float sinTheta = math::safe_sqrt(1.f - cosTheta * cosTheta);
    return Vector3f(cosPhi * sinTheta, sinPhi * sinTheta, cosTheta);
}

Float squareToVonMisesFisherPdf(const Vector3f &v, Float kappa) {
    /* Stable algorithm for evaluating the von Mises Fisher distribution
       https://www.mitsuba-renderer.org/~wenzel/files/vmf.pdf */

    assert(kappa >= 0.f);
	if (kappa == 0.f)
		return math::InvFourPi;
    else
        return std::exp(kappa * (v.z() - 1)) * kappa /
               (2 * math::Pi * (1 - std::exp(-2 * kappa)));
}

Vector3f squareToRoughFiber(const Point3f &sample, const Vector3f &wi_,
                                  const Vector3f &tangent, Float kappa) {
    Frame tframe(tangent);

    /* Convert to local coordinate frame with Z = fiber tangent */
    Vector3f wi = tframe.toLocal(wi_);

    /* Sample a point on the reflection cone */
    Float sinPhi, cosPhi;
    math::sincos(2.f * math::Pi * sample.x(), &sinPhi, &cosPhi);

    Float cosTheta = wi.z();
    Float sinTheta = math::safe_sqrt(1 - cosTheta * cosTheta);

    Vector3f wo(cosPhi * sinTheta, sinPhi * sinTheta, -cosTheta);

    /* Sample a roughness perturbation from a vMF distribution */
    Vector3f perturbation =
        squareToVonMisesFisher(Point2f(sample.y(), sample.z()), kappa);

    /* Express perturbation relative to 'wo' */
    wo = Frame(wo).toWorld(perturbation);

    /* Back to global coordinate frame */
    return tframe.toWorld(wo);
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

Float squareToRoughFiberPdf(const Vector3f &v, const Vector3f &wi,
                            const Vector3f &tangent, Float kappa) {
    /**
     * Analytic density function described in "An Energy-Conserving Hair Reflectance Model"
     * by Eugene d’Eon, Guillaume Francois, Martin Hill, Joe Letteri, and Jean-Marie Aubry
     *
     * Includes modifications for numerical robustness described here:
     * https://publons.com/publon/2803
     */

    Float cosThetaI = dot(wi, tangent);
    Float cosThetaO = dot(v, tangent);
    Float sinThetaI = math::safe_sqrt(1 - cosThetaI * cosThetaI);
    Float sinThetaO = math::safe_sqrt(1 - cosThetaO * cosThetaO);

    Float c = cosThetaI * cosThetaO * kappa;
    Float s = sinThetaI * sinThetaO * kappa;

    Float result;
    if (kappa > 10.f)
        result = std::exp(-c + logI0(s) - kappa + 0.6931f + std::log(kappa / 2.f));
    else
        result = std::exp(-c) * i0(s) * kappa / (2.f * std::sinh(kappa));

    return result * 1.f / (2.f * math::Pi);
}

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
