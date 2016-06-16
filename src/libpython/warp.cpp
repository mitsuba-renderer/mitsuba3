#include <mitsuba/core/math.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/warp.h>
#include <hypothesis.h>
#include <pcg32.h>
#include "python.h"

using VectorList = std::vector<Vector3f>;

/// Enum of available warp types
enum WarpType {
    NoWarp = 0,
    UniformSphere,
    UniformHemisphere,
    CosineHemisphere,
    UniformCone,
    UniformDisk,
    UniformDiskConcentric,
    UniformTriangle,
    StandardNormal,
    UniformTent,
    NonUniformTent
};

/// Enum of available point sampling types
enum SamplingType {
    Independent = 0,
    Grid,
    Stratified
};

namespace {

std::pair<Vector3f, Float> warpPoint(WarpType warpType, Point2f sample, Float parameterValue) {
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

Point2f domainToPoint(const Vector3f &v, WarpType warpType) {
    Point2f p;
    switch (warpType) {
        case NoWarp:
            p[0] = v[0]; p[1] = v[1];
            break;
        case UniformDisk:
        case UniformDiskConcentric:
            p[0] = 0.5f * v[0] + 0.5f;
            p[1] = 0.5f * v[1] + 0.5f;
            break;

        case StandardNormal:
            p = 5 * domainToPoint(v, UniformDisk);
            break;

        default:
            p[0] = std::atan2(v[1], v[0]) * math::InvTwoPi;
            if (p[0] < 0) p[0] += 1;
            p[1] = 0.5f * v[2] + 0.5f;
            break;
    }

    return p;
}

double getPdfScalingFactor(WarpType warpType) {
    switch (warpType) {
        case NoWarp: return 1.0;

        case UniformDisk:
        case UniformDiskConcentric:
            return 4.0;

        case StandardNormal: return 100.0;  // TODO: why?

        case UniformSphere:
        case UniformHemisphere:
        case CosineHemisphere:
        default:
            return 4.0 * math::Pi_d;
    }
}

Float pdfValueForSample(WarpType warpType, double x, double y, Float parameterValue) {
    Point2f p;
    switch (warpType) {
        case NoWarp: return 1.0;

        // TODO: need to use domain indicator functions to zero-out where appropriate
        case UniformDisk:
            p[0] = 2 * x - 1; p[1] = 2 * y - 1;
            return warp::squareToUniformDiskPdf();
        case UniformDiskConcentric:
            p[0] = 2 * x - 1; p[1] = 2 * y - 1;
            return warp::squareToUniformDiskConcentricPdf();

        case StandardNormal:
            p[0] = 2 * x - 1; p[1] = 2 * y - 1;
            return warp::squareToStdNormalPdf((1 / 5.0) * p);

        // TODO
        // case UniformTriangle:
        // case UniformTent:
        // case NonUniformTent:

        default: {
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
                    return warp::squareToUniformSpherePdf();
                case UniformHemisphere:
                    return warp::squareToUniformHemispherePdf();
                case CosineHemisphere:
                    Log(EError, "TODO: squareToCosineHemispherePdf");
                    // return warp::squareToCosineHemispherePdf();
                case UniformCone:
                    return warp::squareToUniformConePdf(parameterValue);
                default:
                    Log(EError, "Unsupported warp type");
            }
        }
    }

    return 0.0;
}

}  // end anonymous namespace

/**
 * \warning The point count may change depending on the sample strategy,
 *          the effective point count is the length of \ref positions
 *          after the function has returned.
 */
void generatePoints(size_t &pointCount, SamplingType pointType, WarpType warpType,
                    Float parameterValue, VectorList &positions, VectorList &weights) {
    /* Determine the number of points that should be sampled */
    size_t sqrtVal = static_cast<size_t>(std::sqrt((float) pointCount) + 0.5f);
    float invSqrtVal = 1.f / sqrtVal;
    if (pointType == Grid || pointType == Stratified)
        pointCount = sqrtVal*sqrtVal;

    pcg32 rng;

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
        positions.push_back(result.first);
        weights.push_back(result.second);
    }
}

std::vector<Float> computeHistogram(WarpType warpType,
                                    const VectorList &positions, const VectorList &weights,
                                    size_t gridWidth, size_t gridHeight) {
    std::vector<Float> hist(gridWidth * gridHeight, 0.0);

    for (size_t i = 0; i < positions.size(); ++i) {
        if (weights[i] == 0) {
            continue;
        }

        Point2f sample = domainToPoint(positions[i], warpType);
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

std::vector<Float> generateExpectedHistogram(size_t pointCount,
                                             WarpType warpType, Float parameterValue,
                                             size_t gridWidth, size_t gridHeight) {
    std::vector<Float> hist(gridWidth * gridHeight, 0.0);
    double scale = pointCount * getPdfScalingFactor(warpType);

    auto integrand = [&warpType, &parameterValue](double x, double y) {
        return pdfValueForSample(warpType, x, y, parameterValue);
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

bool runTest(size_t pointCount, size_t gridWidth, size_t gridHeight,
             SamplingType pointType, WarpType warpType, Float parameterValue) {
    VectorList positions, weights;

    generatePoints(pointCount, pointType, warpType,
                   parameterValue, positions, weights);
    auto observedHistogram = computeHistogram(warpType, positions, weights,
                                              gridWidth, gridHeight);
    auto expectedHistogram = generateExpectedHistogram(pointCount,
                                                       warpType, parameterValue,
                                                       gridWidth, gridHeight);


    return false;
}

MTS_PY_EXPORT(warp) {
    auto m2 = m.def_submodule("warp", "Common warping techniques that map from the unit"
                                      "square to other domains, such as spheres,"
                                      " hemispheres, etc.");

    m2.mdef(warp, squareToUniformSphere)
      .mdef(warp, squareToUniformSpherePdf)
      .def("unitSphereIndicator", [](const Vector3f& v) {
        return (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) <= 1;
      }, "Indicator function of the 3D unit sphere's domain.")

      .mdef(warp, squareToUniformHemisphere)
      .mdef(warp, squareToUniformHemispherePdf)
      .def("unitHemisphereIndicator", [](const Vector3f& v) {
        return ((v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) <= 1)
            && (v[2] >= 0);
      }, "Indicator function of the 3D unit sphere's domain.")

      .mdef(warp, squareToCosineHemisphere)
      // TODO: cosine hemisphere's PDF

      .mdef(warp, squareToUniformCone)
      .mdef(warp, squareToUniformConePdf)
      // TODO
      // .def("unitConeIndicator", [](const Vector3f& v, float cosCutoff) {
      //   return false;
      // }, "Indicator function of the 3D cone's domain.")

      .mdef(warp, squareToUniformDisk)
      .mdef(warp, squareToUniformDiskPdf)
      .def("unitDiskIndicator", [](const Point2f& p) {
        return (p[0] * p[0] + p[1] * p[1]) <= 1;
      }, "Indicator function of the 2D unit disc's domain.")

      .mdef(warp, squareToUniformDiskConcentric)
      .mdef(warp, squareToUniformDiskConcentricPdf)
      .mdef(warp, uniformDiskToSquareConcentric)

      .mdef(warp, squareToUniformTriangle)

      .mdef(warp, squareToStdNormal)
      .mdef(warp, squareToStdNormalPdf)

      .mdef(warp, squareToTent)
      .mdef(warp, intervalToNonuniformTent);


    py::enum_<WarpType>(m2, "WarpType")
        .value("NoWarp", WarpType::NoWarp)
        .value("UniformSphere", WarpType::UniformSphere)
        .value("UniformHemisphere", WarpType::UniformHemisphere)
        .value("CosineHemisphere", WarpType::CosineHemisphere)
        .value("UniformCone", WarpType::UniformCone)
        .value("UniformDisk", WarpType::UniformDisk)
        .value("UniformDiskConcentric", WarpType::UniformDiskConcentric)
        .value("UniformTriangle", WarpType::UniformTriangle)
        .value("StandardNormal", WarpType::StandardNormal)
        .value("UniformTent", WarpType::UniformTent)
        .value("NonUniformTent", WarpType::NonUniformTent)
        .export_values();

    py::enum_<SamplingType>(m2, "SamplingType")
        .value("Independent", SamplingType::Independent)
        .value("Grid", SamplingType::Grid)
        .value("Stratified", SamplingType::Stratified)
        .export_values();

    m2.def("generatePoints", &generatePoints, "Generate and warp points.");
}
