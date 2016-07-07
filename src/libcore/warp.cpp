#include <mitsuba/core/warp.h>
#include <mitsuba/core/warp_adapters.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/transform.h>
#include <hypothesis.h>
#include <pcg32.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(warp)

Vector3f squareToUniformSphere(const Point2f &sample) {
    Float z = 1.0f - 2.0f * sample[1];
    Float r = math::safe_sqrt(1.0f - z*z);
    Float sinPhi, cosPhi;
    math::sincos(2.0f * math::Pi * sample[0], &sinPhi, &cosPhi);
    return Vector3f(r * cosPhi, r * sinPhi, z);
}

Vector3f squareToUniformHemisphere(const Point2f &sample) {
    Float z = sample[0];
    Float tmp = math::safe_sqrt(1.0f - z*z);

    Float sinPhi, cosPhi;
    math::sincos(2.0f * math::Pi * sample[1], &sinPhi, &cosPhi);

    return Vector3f(cosPhi * tmp, sinPhi * tmp, z);
}

Vector3f squareToCosineHemisphere(const Point2f &sample) {
    Point2f p = squareToUniformDiskConcentric(sample);
    Float z = math::safe_sqrt(1.0f - p[0]*p[0] - p[1]*p[1]);

    /* Guard against numerical imprecisions */
    if (z == 0)
        z = 1e-10;

    return Vector3f(p[0], p[1], z);
}

Vector3f squareToUniformCone(const Point2f &sample, Float cosCutoff) {
    Float cosTheta = (1-sample[0]) + sample[0] * cosCutoff;
    Float sinTheta = math::safe_sqrt(1.0f - cosTheta * cosTheta);

    Float sinPhi, cosPhi;
    math::sincos(2.0f * math::Pi * sample[1], &sinPhi, &cosPhi);

    return Vector3f(cosPhi * sinTheta,
                    sinPhi * sinTheta, cosTheta);
}

Point2f squareToUniformDisk(const Point2f &sample) {
    Float r = std::sqrt(sample[0]);
    Float sinPhi, cosPhi;
    math::sincos(2.0f * math::Pi * sample[1], &sinPhi, &cosPhi);

    return Point2f(cosPhi * r,
                   sinPhi * r);
}

Point2f squareToUniformTriangle(const Point2f &sample) {
    Float a = math::safe_sqrt(1.0f - sample[0]);
    return Point2f(1 - a, a * sample[1]);
}

Point2f squareToUniformDiskConcentric(const Point2f &sample) {
    Float r1 = 2.0f*sample[0] - 1.0f;
    Float r2 = 2.0f*sample[1] - 1.0f;

    /* Modified concencric map code with less branching (by Dave Cline), see
     * http://psgraphics.blogspot.ch/2011/01/improved-code-for-concentric-map.html */
    Float phi, r;
    if (r1 == 0 && r2 == 0) {
        r = phi = 0;
    } else if (r1*r1 > r2*r2) {
        r = r1;
        phi = (math::Pi/4.0f) * (r2/r1);
    } else {
        r = r2;
        phi = (math::Pi/2.0f) - (r1/r2) * (math::Pi/4.0f);
    }

    Float cosPhi, sinPhi;
    math::sincos(phi, &sinPhi, &cosPhi);

    return Point2f(r * cosPhi, r * sinPhi);
}

Point2f uniformDiskToSquareConcentric(const Point2f &p) {
    Float r   = std::sqrt(p[0] * p[0] + p[1] * p[1]),
          phi = std::atan2(p[1], p[0]),
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
    Float r   = std::sqrt(-2 * std::log(1-sample[0])),
          phi = 2 * math::Pi * sample[1];
    Point2f result;
    math::sincos(phi, &result[1], &result[0]);
    return result * r;
}

Float squareToStdNormalPdf(const Point2f &pos) {
    return math::InvTwoPi * std::exp(-(pos[0]*pos[0] + pos[1]*pos[1])/2.0f);
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
    return Point2f(intervalToTent(sample[0]),
                   intervalToTent(sample[1]));
}

Float intervalToNonuniformTent(Float a, Float b, Float c, Float sample) {
    Float factor;

    if (sample * (c-a) < b-a) {
        factor = a-b;
        sample *= (a-c)/(a-b);
    } else {
        factor = c-b;
        sample = (a-c)/(b-c) * (sample - (a-b)/(a-c));
    }

    return b + factor * (1-math::safe_sqrt(sample));
}



NAMESPACE_BEGIN(detail)

std::pair<bool, std::string>
runStatisticalTestAndOutput(size_t pointCount, size_t gridWidth, size_t gridHeight,
    SamplingType samplingType, WarpAdapter *warpAdapter,
    double minExpFrequency, double significanceLevel,
    std::vector<double> &observedHistogram, std::vector<double> &expectedHistogram) {

    const auto nBins = gridWidth * gridHeight;
    Eigen::MatrixXf positions;
    std::vector<Float> weights;

    // TODO: should rather use the Sampler interface
    pcg32 sampler;
    observedHistogram = warpAdapter->generateObservedHistogram(
        &sampler, samplingType, pointCount, gridWidth, gridHeight);
    expectedHistogram = warpAdapter->generateExpectedHistogram(
        pointCount, gridWidth, gridHeight);

    return hypothesis::chi2_test(nBins,
        observedHistogram.data(), expectedHistogram.data(),
        pointCount, minExpFrequency, significanceLevel, 1);
}

NAMESPACE_END(detail)

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
