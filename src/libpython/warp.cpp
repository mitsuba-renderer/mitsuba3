#include <mitsuba/core/math.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/gui/warp_visualizer.h>

#include <hypothesis.h>
#include <pcg32.h>
#include "python.h"

// TODO: proper support for:
// - uniformDiskToSquareConcentric
// - intervalToNonuniformTent

namespace {

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

Float pdfValueForSample(WarpType warpType, double x, double y, Float parameterValue) {
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
                return warp::unitDiskIndicator(p) * warp::squareToUniformDiskPdf();
            case UniformDiskConcentric:
                return warp::unitDiskIndicator(p) * warp::squareToUniformDiskConcentricPdf();
            case StandardNormal:
                // TODO: do not hardcode domain
                return warp::squareToStdNormalPdf((1 / 5.0) * p);
            case UniformTriangle:
                return warp::triangleIndicator(p) * warp::squareToUniformTrianglePdf(p);
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
                return warp::unitSphereIndicator(v) * warp::squareToUniformSpherePdf();
            case UniformHemisphere:
                return warp::unitHemisphereIndicator(v) * warp::squareToUniformHemispherePdf();
            case CosineHemisphere:
                return warp::unitHemisphereIndicator(v) * warp::squareToCosineHemispherePdf(v);
            case UniformCone:
                return warp::unitConeIndicator(v) * warp::squareToUniformConePdf(parameterValue);
            default:
                Log(EError, "Unsupported 3D warp type");
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

std::pair<bool, std::string> runStatisticalTestAndOutput(size_t pointCount,
    size_t gridWidth, size_t gridHeight, SamplingType samplingType, WarpType warpType,
    Float parameterValue, double minExpFrequency, double significanceLevel,
    std::vector<double> &observedHistogram, std::vector<double> &expectedHistogram) {

    const auto nBins = gridWidth * gridHeight;
    Eigen::MatrixXf positions;
    std::vector<Float> weights;

    generatePoints(pointCount, samplingType, warpType,
                   parameterValue, positions, weights);
    observedHistogram = computeHistogram(warpType, positions, weights,
                                         gridWidth, gridHeight);
    expectedHistogram = generateExpectedHistogram(pointCount,
                                                  warpType, parameterValue,
                                                  gridWidth, gridHeight);

    return hypothesis::chi2_test(nBins, observedHistogram.data(), expectedHistogram.data(),
                                 pointCount, minExpFrequency, significanceLevel, 1);
}

std::pair<bool, std::string>
runStatisticalTest(size_t pointCount, size_t gridWidth, size_t gridHeight,
                   SamplingType samplingType, WarpType warpType, Float parameterValue,
                   double minExpFrequency, double significanceLevel) {
    std::vector<double> observedHistogram, expectedHistogram;
    return runStatisticalTestAndOutput(pointCount,
        gridWidth, gridHeight, samplingType, warpType, parameterValue,
        minExpFrequency, significanceLevel, observedHistogram, expectedHistogram);
}

MTS_PY_EXPORT(warp) {
    auto m2 = m.def_submodule("warp", "Common warping techniques that map from the unit"
                                      "square to other domains, such as spheres,"
                                      " hemispheres, etc.");

    m2.mdef(warp, unitSphereIndicator)
      .mdef(warp, squareToUniformSphere)
      .mdef(warp, squareToUniformSpherePdf)

      .mdef(warp, unitHemisphereIndicator)
      .mdef(warp, squareToUniformHemisphere)
      .mdef(warp, squareToUniformHemispherePdf)

      .mdef(warp, squareToCosineHemisphere)
      .mdef(warp, squareToCosineHemispherePdf)

      .mdef(warp, unitConeIndicator)
      .mdef(warp, squareToUniformCone)
      .mdef(warp, squareToUniformConePdf)

      .mdef(warp, unitDiskIndicator)
      .mdef(warp, squareToUniformDisk)
      .mdef(warp, squareToUniformDiskPdf)

      .mdef(warp, squareToUniformDiskConcentric)
      .mdef(warp, squareToUniformDiskConcentricPdf)

      .mdef(warp, unitSquareIndicator)
      .mdef(warp, uniformDiskToSquareConcentric)

      .mdef(warp, triangleIndicator)
      .mdef(warp, squareToUniformTriangle)
      .mdef(warp, squareToUniformTrianglePdf)

      .mdef(warp, squareToStdNormal)
      .mdef(warp, squareToStdNormalPdf)

      .mdef(warp, squareToTent)
      .mdef(warp, squareToTentPdf)
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

    m2.def("runStatisticalTest", &runStatisticalTest,
           "Runs a Chi^2 statistical test verifying the given warping type"
           "against its PDF. Returns (passed, reason)");

    // Warp visualization widget, inherits from nanogui::Screen which
    // is already exposed to Python in another module.
    const auto PyScreen = static_cast<py::object>(py::module::import("nanogui").attr("Screen"));
    py::class_<WarpVisualizationWidget>(m2, "WarpVisualizationWidget", PyScreen)
        .def(py::init<int, int, std::string>(), "Default constructor.")
        .def("runTest", &WarpVisualizationWidget::runTest,
            "Run the Chi^2 test for the selected parameters and displays the histograms.")
        .def("refresh", &WarpVisualizationWidget::refresh, "Should be called upon UI interaction.")
        .def("setSamplingType", &WarpVisualizationWidget::setSamplingType, "")
        .def("setWarpType", &WarpVisualizationWidget::setWarpType, "")
        .def("setParameterValue", &WarpVisualizationWidget::setParameterValue, "")
        .def("setPointCount", &WarpVisualizationWidget::setPointCount, "")
        .def("isDrawingHistogram", &WarpVisualizationWidget::isDrawingHistogram, "")
        .def("setDrawHistogram", &WarpVisualizationWidget::setDrawHistogram, "")
        .def("isDrawingGrid", &WarpVisualizationWidget::isDrawingGrid, "")
        .def("setDrawGrid", &WarpVisualizationWidget::setDrawGrid, "")
        .def_readwrite("window", &WarpVisualizationWidget::window);
}
