#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/shape.h>

MTS_PY_EXPORT(Endpoint) {
    using enoki::vectorize_wrapper;

    auto endpoint = MTS_PY_CLASS(Endpoint, Object)
        .def("sample_ray",
             py::overload_cast<Float, Float, const Point2f &, const Point2f &, bool>(
                &Endpoint::sample_ray, py::const_),
             D(Endpoint, sample_ray),
             "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "unused"_a = true)
        .def("sample_ray",
             enoki::vectorize_wrapper(
                py::overload_cast<FloatP, FloatP, const Point2fP &, const Point2fP &, MaskP>(
                    &Endpoint::sample_ray, py::const_)),
             "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = MaskP(true))
        .def("sample_direction",
             py::overload_cast<const Interaction3f &, const Point2f &, bool>(
                &Endpoint::sample_direction, py::const_),
             D(Endpoint, sample_direction), "it"_a, "sample"_a, "unused"_a = true)
        .def("sample_direction",
             enoki::vectorize_wrapper(
                py::overload_cast<const Interaction3fP &, const Point2fP &, MaskP>(
                    &Endpoint::sample_direction, py::const_)),
             "it"_a, "sample"_a, "active"_a = true)
        .def("pdf_direction",
             py::overload_cast<const Interaction3f &, const DirectionSample3f &, bool>(
                &Endpoint::pdf_direction, py::const_),
             D(Endpoint, pdf_direction), "it"_a, "ds"_a, "unused"_a = true)
        .def("pdf_direction",
             enoki::vectorize_wrapper(
                py::overload_cast<const Interaction3fP &, const DirectionSample3fP &, MaskP>(
                    &Endpoint::pdf_direction, py::const_)),
             "it"_a, "ds"_a, "active"_a = true)
        .def("eval",
             py::overload_cast<const SurfaceInteraction3f &, bool>(
                &Endpoint::eval, py::const_),
             D(Endpoint, eval), "si"_a, "unused"_a = true)
        .def("eval",
             enoki::vectorize_wrapper(
                py::overload_cast<const SurfaceInteraction3fP &, MaskP>(
                    &Endpoint::eval, py::const_)),
             "si"_a, "active"_a = true)
        .mdef(Endpoint, world_transform)
        .mdef(Endpoint, needs_sample_2)
        .mdef(Endpoint, needs_sample_3)
        .def("shape", py::overload_cast<>(&Endpoint::shape, py::const_),
             D(Endpoint, shape))
        .def("medium", py::overload_cast<>(&Endpoint::medium, py::const_),
             D(Endpoint, medium))
        .mdef(Endpoint, set_shape, "shape"_a)
        .mdef(Endpoint, set_medium, "medium"_a)
        .mdef(Endpoint, bbox);
}
