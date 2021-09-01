#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Endpoint) {
    MTS_PY_IMPORT_TYPES()
    MTS_PY_CLASS(Endpoint, Object)
        .def("sample_ray", &Endpoint::sample_ray,
            "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true,
            D(Endpoint, sample_ray))
        .def("sample_direction", &Endpoint::sample_direction,
            "it"_a, "sample"_a, "active"_a = true, D(Endpoint, sample_direction))
        .def("pdf_direction", &Endpoint::pdf_direction,
            "it"_a, "ds"_a, "active"_a = true, D(Endpoint, pdf_direction))
        .def("eval", &Endpoint::eval,
            "si"_a, "active"_a = true, D(Endpoint, eval))
        .def_method(Endpoint, world_transform)
        .def_method(Endpoint, needs_sample_2)
        .def_method(Endpoint, needs_sample_3)
        .def("shape",  py::overload_cast<>(&Endpoint::shape, py::const_),  D(Endpoint, shape))
        .def("medium", py::overload_cast<>(&Endpoint::medium, py::const_), D(Endpoint, medium))
        .def_method(Endpoint, set_shape, "shape"_a)
        .def_method(Endpoint, set_medium, "medium"_a)
        .def_method(Endpoint, bbox);

    if constexpr (ek::is_array_v<EndpointPtr>) {
        py::object ek       = py::module_::import("enoki"),
                   ek_array = ek.attr("ArrayBase");

        py::class_<EndpointPtr> cls(m, "EndpointPtr", ek_array);

        cls.def("sample_ray",
                [](EndpointPtr ptr, Float time, Float sample1, const Point2f &sample2,
                const Point2f &sample3, Mask active) {
                    return ptr->sample_ray(time, sample1, sample2, sample3, active);
                },
                "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true,
                D(Endpoint, sample_ray))
        .def("sample_direction",
                [](EndpointPtr ptr, const Interaction3f &it, const Point2f &sample, Mask active) {
                    return ptr->sample_direction(it, sample, active);
                },
                "it"_a, "sample"_a, "active"_a = true,
                D(Endpoint, sample_direction))
        .def("pdf_direction",
                [](EndpointPtr ptr, const Interaction3f &it, const DirectionSample3f &ds, Mask active) {
                    return ptr->pdf_direction(it, ds, active);
                },
                "it"_a, "ds"_a, "active"_a = true,
                D(Endpoint, pdf_direction))
        .def("eval",
                [](EndpointPtr ptr, const SurfaceInteraction3f &si, Mask active) {
                    return ptr->eval(si, active);
                },
                "si"_a, "active"_a = true, D(Endpoint, eval));

        bind_enoki_ptr_array(cls);
    }
}
