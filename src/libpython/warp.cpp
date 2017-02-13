#include <mitsuba/core/warp.h>
#include <mitsuba/core/transform.h>
#include <tbb/tbb.h>
#include "python.h"

MTS_PY_EXPORT(warp) {
    MTS_PY_IMPORT_MODULE(warp, "mitsuba.core.warp");

    warp.def("square_to_uniform_sphere",
          warp::square_to_uniform_sphere<Vector3f, Point2f>, "sample"_a,
          D(warp, square_to_uniform_sphere));

    warp.def(
        "square_to_uniform_sphere",
        vectorize_wrapper(warp::square_to_uniform_sphere<Vector3fP, Point2fP>),
        "sample"_a);

    warp.def("square_to_uniform_sphere_pdf",
          warp::square_to_uniform_sphere_pdf<true, Vector3f>, "v"_a,
          D(warp, square_to_uniform_sphere_pdf));

    warp.def(
        "square_to_uniform_sphere_pdf",
        vectorize_wrapper(warp::square_to_uniform_sphere_pdf<true, Vector3fP>),
        "v"_a);

#if 0
    Old code -- outdated vectorization approach, naming conventions, etc.
    warp
      .def("square_to_uniform_sphere", vectorize(warp::square_to_uniform_sphere),
          D(warp, square_to_uniform_sphere), "sample"_a)
      .def("square_to_uniform_spherePdf", vectorize(warp::square_to_uniform_spherePdf<true>),
          D(warp, square_to_uniform_spherePdf), "v"_a)
      .def("squareToUniformHemisphere", vectorize(warp::squareToUniformHemisphere),
          D(warp, squareToUniformHemisphere), "sample"_a)
      .def("squareToUniformHemispherePdf", vectorize(warp::squareToUniformHemispherePdf<true>),
          D(warp, squareToUniformHemispherePdf), "v"_a)
      .def("squareToCosineHemisphere", vectorize(warp::squareToCosineHemisphere),
          D(warp, squareToCosineHemisphere), "sample"_a)
      .def("squareToCosineHemispherePdf", vectorize(warp::squareToCosineHemispherePdf<true>),
          D(warp, squareToCosineHemispherePdf), "v"_a)
      .def("squareToUniformDisk", vectorize(warp::squareToUniformDisk),
          D(warp, squareToUniformDisk), "sample"_a)
      .def("squareToUniformDiskPdf", vectorize(warp::squareToUniformDiskPdf<true>),
          D(warp, squareToUniformDiskPdf), "v"_a)
      .def("squareToUniformDiskConcentric", vectorize(warp::squareToUniformDiskConcentric),
          D(warp, squareToUniformDiskConcentric), "sample"_a)
      .def("squareToUniformDiskConcentricPdf", vectorize(warp::squareToUniformDiskConcentricPdf<true>),
          D(warp, squareToUniformDiskConcentricPdf), "v"_a)
      .def("diskToUniformSquareConcentric", vectorize(warp::diskToUniformSquareConcentric),
          D(warp, diskToUniformSquareConcentric), "sample"_a)
      .def("squareToUniformTriangle", vectorize(warp::squareToUniformTriangle),
          D(warp, squareToUniformTriangle), "sample"_a)
      .def("squareToUniformTrianglePdf", vectorize(warp::squareToUniformTrianglePdf<true>),
          D(warp, squareToUniformTrianglePdf), "v"_a)
      .def("squareToStdNormal", vectorize(warp::squareToStdNormal),
          D(warp, squareToStdNormal), "sample"_a)
      .def("squareToStdNormalPdf", vectorize(warp::squareToStdNormalPdf),
          D(warp, squareToStdNormalPdf), "p"_a)
      .def("squareToTent", vectorize(warp::squareToTent),
          D(warp, squareToTent), "sample"_a)
      .def("squareToTentPdf", vectorize(warp::squareToTentPdf),
          D(warp, squareToTentPdf), "p"_a)
      .def("squareToUniformCone", [](const py::array_t<Float, py::array::c_style> &sample, Float cosCutoff) {
              auto closure = [cosCutoff](const Point2f &sample) { return warp::squareToUniformCone(sample, cosCutoff); };
              return vectorize(closure)(sample);
           }, "sample"_a, "cosCutoff"_a, D(warp, squareToUniformCone))
      .def("squareToUniformConePdf", [](const py::array_t<Float, py::array::c_style> &v, Float cosCutoff) {
              auto closure = [cosCutoff](const Vector3f &v) { return warp::squareToUniformConePdf<true>(v, cosCutoff); };
              return vectorize(closure)(v);
           }, "v"_a, "cosCutoff"_a, D(warp, squareToUniformConePdf))
      .def("intervalToNonuniformTent", &warp::intervalToNonuniformTent,
           "sample"_a, "a"_a, "b"_a, "c"_a,
           D(warp, intervalToNonuniformTent))
      .def("squareToBeckmann", [](const py::array_t<Float, py::array::c_style> &sample, Float alpha) {
              auto closure = [alpha](const Point2f &sample) { return warp::squareToBeckmann(sample, alpha); };
              return vectorize(closure)(sample);
           }, "sample"_a, "alpha"_a, D(warp, squareToBeckmann))
      .def("squareToBeckmannPdf", [](const py::array_t<Float, py::array::c_style> &v, Float alpha) {
              auto closure = [alpha](const Vector3f &v) { return warp::squareToBeckmannPdf(v, alpha); };
              return vectorize(closure)(v);
           }, "v"_a, "alpha"_a, D(warp, squareToBeckmannPdf))
      .def("squareToVonMisesFisher", [](const py::array_t<Float, py::array::c_style> &sample, Float kappa) {
              auto closure = [kappa](const Point2f &sample) { return warp::squareToVonMisesFisher(sample, kappa); };
              return vectorize(closure)(sample);
           }, "sample"_a, "kappa"_a, D(warp, squareToVonMisesFisher))
      .def("squareToVonMisesFisherPdf", [](const py::array_t<Float, py::array::c_style> &v, Float kappa) {
              auto closure = [kappa](const Vector3f &v) { return warp::squareToVonMisesFisherPdf(v, kappa); };
              return vectorize(closure)(v);
           }, "v"_a, "kappa"_a, D(warp, squareToVonMisesFisherPdf))
      .def("squareToRoughFiber", [](const py::array_t<Float, py::array::c_style> &sample,
                                    const Vector3f &wi, const Vector3f &tangent, Float kappa) {
              auto closure = [wi, tangent, kappa](const Point3f &sample) {
                  return warp::squareToRoughFiber(sample, wi, tangent, kappa);
              };
              return vectorize(closure)(sample);
           }, "sample"_a, "wi"_a, "tangent"_a, "kappa"_a, D(warp, squareToRoughFiber))
      .def("squareToRoughFiberPdf", [](const py::array_t<Float, py::array::c_style> &v,
                                       const Vector3f &wi, const Vector3f &tangent, Float kappa) {
              auto closure = [wi, tangent, kappa](const Vector3f &v) {
                  return warp::squareToRoughFiberPdf(v, wi, tangent, kappa);
              };
              return vectorize(closure)(v);
           }, "v"_a, "wi"_a, "tangent"_a, "kappa"_a, D(warp, squareToRoughFiberPdf));
#endif
}
