#include <mitsuba/core/vector.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/warp.h>
#include <pcg32.h>
#include "python.h"

using Eigen::MatrixXf;

/// Enum of available warp types
enum WarpType {
    NoWarp = 0,
    UniformSphere,
    UniformHemisphere,
    UniformHemisphereCos,
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

// TODO: should be able to use Mitsuba's vector type here
Eigen::Vector3f warpPoint(WarpType warpType, Point2f sample, Float parameterValue) {
    // TODO: route the warping call to the appropriate function
    Eigen::Vector3f v;
    v << sample[0], sample[1], 0;
    return v;
}

void generatePoints(int &pointCount, SamplingType pointType, WarpType warpType,
                    Float parameterValue, MatrixXf &positions) {
    /* Determine the number of points that should be sampled */
    int sqrtVal = (int) (std::sqrt((float) pointCount) + 0.5f);
    float invSqrtVal = 1.f / sqrtVal;
    if (pointType == Grid || pointType == Stratified)
        pointCount = sqrtVal*sqrtVal;

    pcg32 rng;
    positions.resize(3, pointCount);

    for (int i = 0; i < pointCount; ++i) {
        int y = i / sqrtVal, x = i % sqrtVal;
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

        // TODO: what's the cleanest way to do generical warping (which side should route?)
        positions.col(i) = warpPoint(warpType, sample, parameterValue);
    }
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
        .value("UniformHemisphereCos", WarpType::UniformHemisphereCos)
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
}
