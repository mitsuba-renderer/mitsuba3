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
        .def("sample_ray", vectorize(&Endpoint::sample_ray),
            "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true,
            D(Endpoint, sample_ray))
        .def("sample_direction", vectorize(&Endpoint::sample_direction),
            "it"_a, "sample"_a, "active"_a = true, D(Endpoint, sample_direction))
        .def("pdf_direction", vectorize(&Endpoint::pdf_direction),
            "it"_a, "ds"_a, "active"_a = true, D(Endpoint, pdf_direction))
        .def("eval", vectorize(&Endpoint::eval),
            "si"_a, "active"_a = true, D(Endpoint, eval))
        .def_method(Endpoint, world_transform)
        .def_method(Endpoint, needs_sample_2)
        .def_method(Endpoint, needs_sample_3)
        .def("shape",  py::overload_cast<>(&Endpoint::shape, py::const_),  D(Endpoint, shape))
        .def("medium", py::overload_cast<>(&Endpoint::medium, py::const_), D(Endpoint, medium))
        .def_method(Endpoint, set_shape, "shape"_a)
        .def_method(Endpoint, set_medium, "medium"_a)
        .def_method(Endpoint, bbox);
}
