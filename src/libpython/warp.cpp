#include <mitsuba/core/warp.h>
#include "python.h"

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
