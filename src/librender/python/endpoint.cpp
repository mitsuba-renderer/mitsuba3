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
        .def("sample_position",
             py::overload_cast<PositionSample3f &, const Point2f &,
                               const Point2f *>(
                &Endpoint::sample_position, py::const_),
             D(Endpoint, sample_position),
             "p_rec"_a, "sample"_a, "extra"_a = py::none())

        // TODO: the vectorized bindings are currently disabled due to a
        // limitation in `vectorize_wrapper` (it's not possible to
        // support non-const reference arguments).

        // .def("sample_position", vectorize_wrapper(
        //      py::overload_cast<PositionSample3fP &, const Point2fP &,
        //                        const Point2fP *, const mask_t<FloatP> &>(
        //         &Endpoint::sample_position, py::const_)),
        //      D(Endpoint, sample_position),
        //      "p_rec"_a, "sample"_a, "extra"_a = py::none())

        .def("sample_direction",
             py::overload_cast<DirectionSample3f &, PositionSample3f &,
                               const Point2f &, const Point2f *>(
                &Endpoint::sample_direction, py::const_),
             D(Endpoint, sample_direction),
             "d_rec"_a, "p_rec"_a, "sample"_a, "extra"_a = py::none())
        // .def("sample_direction", vectorize_wrapper(
        //      py::overload_cast<DirectionSample3fP &, PositionSample3fP &,
        //                        const Point2fP &, const Point2fP *,
        //                        const mask_t<FloatP> &>(
        //         &Endpoint::sample_direction, py::const_)),
        //      D(Endpoint, sample_direction),
        //      "d_rec"_a, "p_rec"_a, "sample"_a, "extra"_a = py::none())

        .def("sample_direct",
             py::overload_cast<DirectSample3f &, const Point2f &>(
                &Endpoint::sample_direct, py::const_),
             D(Endpoint, sample_direct),
             "d_rec"_a, "sample"_a)
        // .def("sample_direct", vectorize_wrapper(
        //      py::overload_cast<DirectSample3fP &, const Point2fP &,
        //                        const mask_t<FloatP> &>(
        //         &Endpoint::sample_direct, py::const_)),
        //      D(Endpoint, sample_direct),
        //      "d_rec"_a, "sample"_a)

        .def("eval_position",
             py::overload_cast<const PositionSample3f &>(
                &Endpoint::eval_position, py::const_),
             D(Endpoint, eval_position), "p_rec"_a)
        .def("eval_position", vectorize_wrapper(
             py::overload_cast<const PositionSample3fP &, const mask_t<FloatP> &>(
                &Endpoint::eval_position, py::const_)),
             D(Endpoint, eval_position), "p_rec"_a, "active"_a = true)

        .def("eval_direction",
             py::overload_cast<const DirectionSample3f &, const PositionSample3f &>(
                &Endpoint::eval_direction, py::const_),
             D(Endpoint, eval_direction), "d_rec"_a, "p_rec"_a)
        .def("eval_direction", vectorize_wrapper(
             py::overload_cast<const DirectionSample3fP &,
                               const PositionSample3fP &, const mask_t<FloatP> &>(
                &Endpoint::eval_direction, py::const_)),
             D(Endpoint, eval_direction), "d_rec"_a, "p_rec"_a, "active"_a = true)

        .def("pdf_position",
             py::overload_cast<const PositionSample3f &>(
                &Endpoint::pdf_position, py::const_),
             D(Endpoint, pdf_position), "p_rec"_a)
        .def("pdf_position", vectorize_wrapper(
             py::overload_cast<const PositionSample3fP &, const mask_t<FloatP> &>(
                &Endpoint::pdf_position, py::const_)),
             D(Endpoint, pdf_position), "p_rec"_a, "active"_a = true)

        .def("pdf_direction",
             py::overload_cast<const DirectionSample3f &, const PositionSample3f &>(
                &Endpoint::pdf_direction, py::const_),
             D(Endpoint, pdf_direction), "d_rect"_a, "p_rec"_a)
        .def("pdf_direction", vectorize_wrapper(
             py::overload_cast<const DirectionSample3fP &,
                               const PositionSample3fP &, const mask_t<FloatP> &>(
                &Endpoint::pdf_direction, py::const_)),
             D(Endpoint, pdf_direction), "d_rect"_a, "p_rec"_a, "active"_a = true)

        .def("pdf_direct",
             py::overload_cast<const DirectSample3f &>(
                &Endpoint::pdf_direct, py::const_),
             D(Endpoint, pdf_direct), "d_rec"_a)
        .def("pdf_direct", vectorize_wrapper(
             py::overload_cast<const DirectSample3fP &, const mask_t<FloatP> &>(
                &Endpoint::pdf_direct, py::const_)),
             D(Endpoint, pdf_direct), "d_rec"_a, "active"_a = true)

        .mdef(Endpoint, type, "unused"_a = true)
        .mdef(Endpoint, world_transform, "unused"_a = true)
        .mdef(Endpoint, set_world_transform, "trafo"_a, "unused"_a = true)
        .mdef(Endpoint, needs_position_sample, "unused"_a = true)
        .mdef(Endpoint, needs_direction_sample, "unused"_a = true)
        .mdef(Endpoint, is_on_surface, "unused"_a = true)
        .mdef(Endpoint, is_degenerate, "unused"_a = true)
        .mdef(Endpoint, needs_direct_sample, "unused"_a = true)
        .mdef(Endpoint, direct_measure, "unused"_a = true)

        .def("medium", py::overload_cast<bool>(&Endpoint::medium),
             "unused"_a = true, D(Endpoint, medium))
        .def("medium", py::overload_cast<bool>(&Endpoint::medium, py::const_),
             "unused"_a = true, D(Endpoint, medium))
        .def("shape", py::overload_cast<bool>(&Endpoint::shape),
             "unused"_a = true, D(Endpoint, shape))
        .def("shape", py::overload_cast<bool>(&Endpoint::shape, py::const_),
             "unused"_a = true, D(Endpoint, shape))

        .mdef(Endpoint, create_shape, "scene"_a, "unused"_a = true)
        .mdef(Endpoint, bbox, "unused"_a = true)
        .mdef(Endpoint, set_shape, "shape"_a, "unused"_a = true)
        .mdef(Endpoint, set_medium, "medium"_a, "unused"_a = true)
        ;

    // TODO: use docstrings from `mkdoc.py` (currently, there's no pybind11 support).
    py::enum_<Endpoint::EFlags>(endpoint,
        "EFlags",
        "Flags used to classify the emission profile of different types of"
        " emitters. \n"
        "\n    EDeltaDirection: Emission profile contains a Dirac delta term with respect to direction."
        "\n    EDeltaPosition: Emission profile contains a Dirac delta term with respect to position."
        "\n    EOnSurface: Is the emitter associated with a surface in the scene?",
        py::arithmetic())
        .value("EDeltaDirection", Endpoint::EDeltaDirection)
        .value("EDeltaPosition", Endpoint::EDeltaPosition)
        .value("EOnSurface", Endpoint::EOnSurface)
        .export_values();
}
