#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Emitter) {
    auto emitter = MTS_PY_CLASS(Emitter, Endpoint)
        .def("eval",
             py::overload_cast<const SurfaceInteraction3f &, const Vector3f &>(
                &Emitter::eval, py::const_),
             D(Emitter, eval), "its"_a, "d"_a)
        .def("eval", enoki::vectorize_wrapper(
             py::overload_cast<const SurfaceInteraction3fP &, const Vector3fP &,
                               const mask_t<FloatP> &>(
                &Emitter::eval, py::const_)),
             D(Emitter, eval), "its"_a, "d"_a, "active"_a = true)

        .def("sample_ray",
             py::overload_cast<const Point2f &, const Point2f &, Float>(
                &Emitter::sample_ray, py::const_),
             D(Emitter, sample_ray),
             "position_sample"_a, "direction_sample"_a, "time_sample"_a)
        .def("sample_ray", enoki::vectorize_wrapper(
             py::overload_cast<const Point2fP &, const Point2fP &, FloatP,
                               const mask_t<FloatP> &>(
                &Emitter::sample_ray, py::const_)),
             D(Emitter, sample_ray),
             "position_sample"_a, "direction_sample"_a,
             "time_sample"_a, "active"_a = true)

        .mdef(Emitter, bitmap, "size_hint"_a = Vector2i(-1, -1))
        .mdef(Emitter, is_environment_emitter)

        .def("eval_environment",
             py::overload_cast<const RayDifferential3f &>(
                &Emitter::eval_environment, py::const_),
             D(Emitter, eval_environment), "ray"_a)
        .def("eval_environment", enoki::vectorize_wrapper(
             py::overload_cast<const RayDifferential3fP &,
                               const mask_t<FloatP> &>(
                &Emitter::eval_environment, py::const_)),
             D(Emitter, eval_environment), "ray"_a, "active"_a = true)

        .def("fill_direct_sample",
             py::overload_cast<DirectSample3f &, const Ray3f &>(
                &Emitter::fill_direct_sample, py::const_),
             D(Emitter, fill_direct_sample), "d_rec"_a, "ray"_a)
        // .def("fill_direct_sample", enoki::vectorize_wrapper(
        //      py::overload_cast<DirectSample3fP &, const Ray3fP &,
        //                        const mask_t<FloatP> &>(
        //         &Emitter::fill_direct_sample, py::const_)),
        //      D(Emitter, fill_direct_sample), "d_rec"_a, "ray"_a, "active"_a = true)
        ;

    // TODO: import EFlags from endpoint?
    // emitter.attr("EFlags") = emitter_type;
    // TODO: proper docs
    py::enum_<Emitter::EEmitterFlags>(emitter, "EEmitterFlags",
        "This list of flags is used to additionally characterize and"
        " classify the response functions of different types of sensors",
        py::arithmetic())
        .value("EEnvironmentEmitter", Emitter::EEnvironmentEmitter)
               // D(Emitter, EEmitterFlags, EEnvironmentEmitter))
        .export_values();
}
