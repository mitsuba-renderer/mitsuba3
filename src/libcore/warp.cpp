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

Vector3f squareToUniformCone(Float cosCutoff, const Point2f &sample) {
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

bool isTwoDimensionalWarp(WarpType warpType) {
    switch (warpType) {
        case NoWarp:
        case UniformDisk:
        case UniformDiskConcentric:
        case UniformTriangle:
        case StandardNormal:
        case UniformTent:
            return true;
        default:
            return false;
    }
}

std::pair<Vector3f, Float> warpPoint(WarpType warpType, Point2f sample,
                                     Float parameterValue) {
    Vector3f result;

    auto fromPoint = [](const Point2f& p) {
        Vector3f v;
        v[0] = p[0]; v[1] = p[1]; v[2] = 0.0;
        return v;
    };

    switch (warpType) {
        case NoWarp:
            result = fromPoint(sample);
            break;
        case UniformSphere:
            result = warp::squareToUniformSphere(sample);
            break;
        case UniformHemisphere:
            result = warp::squareToUniformHemisphere(sample);
            break;
        case CosineHemisphere:
            result = warp::squareToCosineHemisphere(sample);
            break;
        case UniformCone:
            result = warp::squareToUniformCone(parameterValue, sample);
            break;

        case UniformDisk:
            result = fromPoint(warp::squareToUniformDisk(sample));
            break;
        case UniformDiskConcentric:
            result = fromPoint(warp::squareToUniformDiskConcentric(sample));
            break;
        case UniformTriangle:
            result = fromPoint(warp::squareToUniformTriangle(sample));
            break;
        case StandardNormal:
            result = fromPoint(warp::squareToStdNormal(sample));
            break;
        case UniformTent:
            result = fromPoint(warp::squareToTent(sample));
            break;
        // TODO: need to pass 3 parameters instead of one...
        // case NonUniformTent:
        //     result << warp::squareToUniformSphere(sample);
        //     break;
        default:
            throw new std::runtime_error("Unsupported warping function");
    }
    return std::make_pair(result, 1.f);
}

Point2f domainToPoint(const Eigen::Vector3f &v, WarpType warpType) {
    Point2f p;
    if (warpType == NoWarp) {
        p[0] = v(0); p[1] = v(1);
    } else if (warpType == UniformTriangle) {
        p[0] = v(0); p[1] = v(1);
    } else if (isTwoDimensionalWarp(warpType)) {
        p[0] = 0.5f * v(0) + 0.5f;
        p[1] = 0.5f * v(1) + 0.5f;
    } else if (warpType == StandardNormal) {
        // TODO: inverse gaussian mapping
        p = 5 * domainToPoint(v, UniformDisk);
    } else {
        p[0] = std::atan2(v(1), v(0)) * math::InvTwoPi;
        if (p[0] < 0) p[0] += 1;
        p[1] = 0.5f * v(2) + 0.5f;
    }

    return p;
}

double getPdfScalingFactor(WarpType warpType) {
    if (warpType == NoWarp)
        return 1.0;

    if (isTwoDimensionalWarp(warpType))
        return 4.0;

    if (warpType == StandardNormal)
        return 100.0;

    // Other warps are 3D
    return 4.0 * math::Pi_d;
}

Float pdfValueForSample(WarpType warpType, Float parameterValue,
                        double x, double y) {
    if (warpType == NoWarp) {
        return 1.0;
    } else if (isTwoDimensionalWarp(warpType)) {
        Point2f p;
        if (warpType == UniformTriangle) {
            p[0] = x; p[1] = y;
        } else {
            p[0] = 2 * x - 1; p[1] = 2 * y - 1;
        }

        switch (warpType) {
            case UniformDisk:
                return warp::squareToUniformDiskPdf(p);
            case UniformDiskConcentric:
                return warp::squareToUniformDiskConcentricPdf(p);
            case StandardNormal:
                // TODO: use inverse gaussian mapping instead
                return warp::squareToStdNormalPdf((1 / 5.0) * p);
            case UniformTriangle:
                return warp::squareToUniformTrianglePdf(p);
            case UniformTent:
                return warp::squareToTentPdf(p);

            default:
                Log(EError, "Unsupported 2D warp type");
        }
    } else {
        // Map 2D sample to 3D domain
        x = 2 * math::Pi_d * x;
        y = 2 * y - 1;

        double sinTheta = std::sqrt(1 - y * y);
        double sinPhi, cosPhi;
        math::sincos(x, &sinPhi, &cosPhi);

        Vector3f v((Float) (sinTheta * cosPhi),
                   (Float) (sinTheta * sinPhi),
                   (Float) y);

        switch (warpType) {
            case UniformSphere:
                return warp::squareToUniformSpherePdf(v);
            case UniformHemisphere:
                return warp::squareToUniformHemispherePdf(v);
            case CosineHemisphere:
                return warp::squareToCosineHemispherePdf(v);
            case UniformCone:
                return warp::squareToUniformConePdf(v, parameterValue);
            default:
                Log(EError, "Unsupported 3D warp type");
        }
    }

    return 0.0;
}

void generatePoints(size_t &pointCount, SamplingType pointType, WarpType warpType,
                    Float parameterValue,
                    Eigen::MatrixXf &positions, std::vector<Float> &weights) {
    /* Determine the number of points that should be sampled */
    size_t sqrtVal = static_cast<size_t>(std::sqrt((float) pointCount) + 0.5f);
    float invSqrtVal = 1.f / sqrtVal;
    if (pointType == Grid || pointType == Stratified)
        pointCount = sqrtVal*sqrtVal;

    pcg32 rng;

    positions.resize(3, pointCount);
    for (size_t i = 0; i < pointCount; ++i) {
        size_t y = i / sqrtVal, x = i % sqrtVal;
        Point2f sample;

        switch (pointType) {
            case Independent:
                sample = Point2f(rng.nextFloat(), rng.nextFloat());
                break;

            case Grid:
                sample = Point2f((x + 0.5f) * invSqrtVal, (y + 0.5f) * invSqrtVal);
                break;

            case Stratified:
                sample = Point2f((x + rng.nextFloat()) * invSqrtVal,
                                 (y + rng.nextFloat()) * invSqrtVal);
                break;
        }

        auto result = warpPoint(warpType, sample, parameterValue);
        positions.col(i) = static_cast<Eigen::Matrix<float, 3, 1>>(result.first);
        weights.push_back(result.second);
    }
}

std::vector<double> computeHistogram(WarpType warpType,
                                    const Eigen::MatrixXf &positions,
                                    const std::vector<Float> &weights,
                                    size_t gridWidth, size_t gridHeight) {
    std::vector<double> hist(gridWidth * gridHeight, 0.0);

    for (size_t i = 0; i < static_cast<size_t>(positions.cols()); ++i) {
        if (weights[i] == 0) {
            continue;
        }

        Point2f sample = domainToPoint(positions.col(i), warpType);
        float x = sample[0],
              y = sample[1];

        size_t xbin = std::min(gridWidth - 1,
            std::max(0lu, static_cast<size_t>(std::floor(x * gridWidth))));
        size_t ybin = std::min(gridHeight - 1,
            std::max(0lu, static_cast<size_t>(std::floor(y * gridHeight))));

        hist[ybin * gridWidth + xbin] += 1;
    }

    return hist;
}

std::vector<double> generateExpectedHistogram(size_t pointCount,
                                              WarpType warpType, Float parameterValue,
                                              size_t gridWidth, size_t gridHeight) {
    std::vector<double> hist(gridWidth * gridHeight, 0.0);
    double scale = pointCount * getPdfScalingFactor(warpType);

    auto integrand = [&warpType, &parameterValue](double y, double x) {
        return pdfValueForSample(warpType, parameterValue, x, y);
    };

    for (size_t y = 0; y < gridHeight; ++y) {
        double yStart = y       / static_cast<double>(gridHeight);
        double yEnd   = (y + 1) / static_cast<double>(gridHeight);
        for (size_t x = 0; x < gridWidth; ++x) {
            double xStart =  x      / static_cast<double>(gridWidth);
            double xEnd   = (x + 1) / static_cast<double>(gridWidth);

            hist[y * gridWidth + x] = scale *
                hypothesis::adaptiveSimpson2D(integrand,
                                              yStart, xStart, yEnd, xEnd);
            if (hist[y * gridWidth + x] < 0)
                Log(EError, "The Pdf() function returned negative values!");
        }
    }

    return hist;
}

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
