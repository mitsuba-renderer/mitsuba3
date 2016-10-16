#include <mitsuba/core/warp.h>
#include <mitsuba/core/transform.h>
#include <tbb/tbb.h>
#include "python.h"

MTS_PY_EXPORT(warp) {
    MTS_PY_IMPORT_MODULE(warp, "mitsuba.core.warp");
    warp
      .def("squareToUniformSphere", vectorize(warp::squareToUniformSphere),
          DM(warp, squareToUniformSphere), py::arg("sample"))
      .def("squareToUniformSpherePdf", vectorize(warp::squareToUniformSpherePdf<true>),
          DM(warp, squareToUniformSpherePdf), py::arg("v"))
      .def("squareToUniformHemisphere", vectorize(warp::squareToUniformHemisphere),
          DM(warp, squareToUniformHemisphere), py::arg("sample"))
      .def("squareToUniformHemispherePdf", vectorize(warp::squareToUniformHemispherePdf<true>),
          DM(warp, squareToUniformHemispherePdf), py::arg("v"))
      .def("squareToCosineHemisphere", vectorize(warp::squareToCosineHemisphere),
          DM(warp, squareToCosineHemisphere), py::arg("sample"))
      .def("squareToCosineHemispherePdf", vectorize(warp::squareToCosineHemispherePdf<true>),
          DM(warp, squareToCosineHemispherePdf), py::arg("v"))
      .def("squareToUniformDisk", vectorize(warp::squareToUniformDisk),
          DM(warp, squareToUniformDisk), py::arg("sample"))
      .def("squareToUniformDiskPdf", vectorize(warp::squareToUniformDiskPdf<true>),
          DM(warp, squareToUniformDiskPdf), py::arg("v"))
      .def("squareToUniformDiskConcentric", vectorize(warp::squareToUniformDiskConcentric),
          DM(warp, squareToUniformDiskConcentric), py::arg("sample"))
      .def("squareToUniformDiskConcentricPdf", vectorize(warp::squareToUniformDiskConcentricPdf<true>),
          DM(warp, squareToUniformDiskConcentricPdf), py::arg("v"))
      .def("diskToUniformSquareConcentric", vectorize(warp::diskToUniformSquareConcentric),
          DM(warp, diskToUniformSquareConcentric), py::arg("sample"))
      .def("squareToUniformTriangle", vectorize(warp::squareToUniformTriangle),
          DM(warp, squareToUniformTriangle), py::arg("sample"))
      .def("squareToUniformTrianglePdf", vectorize(warp::squareToUniformTrianglePdf<true>),
          DM(warp, squareToUniformTrianglePdf), py::arg("v"))
      .def("squareToStdNormal", vectorize(warp::squareToStdNormal),
          DM(warp, squareToStdNormal), py::arg("sample"))
      .def("squareToStdNormalPdf", vectorize(warp::squareToStdNormalPdf),
          DM(warp, squareToStdNormalPdf), py::arg("p"))
      .def("squareToTent", vectorize(warp::squareToTent),
          DM(warp, squareToTent), py::arg("sample"))
      .def("squareToTentPdf", vectorize(warp::squareToTentPdf),
          DM(warp, squareToTentPdf), py::arg("p"))
      .def("squareToUniformCone", [](const py::array_t<Float, py::array::f_style> &sample, Float cosCutoff) {
              auto closure = [cosCutoff](const Point2f &sample) { return warp::squareToUniformCone(sample, cosCutoff); };
              return vectorize(closure)(sample);
           }, py::arg("sample"), py::arg("cosCutoff"), DM(warp, squareToUniformCone))
      .def("squareToUniformConePdf", [](const py::array_t<Float, py::array::f_style> &v, Float cosCutoff) {
              auto closure = [cosCutoff](const Vector3f &v) { return warp::squareToUniformConePdf(v, cosCutoff); };
              return vectorize(closure)(v);
           }, py::arg("v"), py::arg("cosCutoff"), DM(warp, squareToUniformConePdf))
      .def("intervalToNonuniformTent", &warp::intervalToNonuniformTent,
           py::arg("sample"), py::arg("a"), py::arg("b"), py::arg("c"),
           DM(warp, intervalToNonuniformTent));
}
