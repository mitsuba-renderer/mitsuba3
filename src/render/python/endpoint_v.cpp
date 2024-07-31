#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/pair.h>

MI_PY_EXPORT(Endpoint) {
    MI_PY_IMPORT_TYPES()
    MI_PY_CLASS(Endpoint, Object)
        .def_method(Endpoint, sample_ray, "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true)
        .def_method(Endpoint, sample_direction, "it"_a, "sample"_a, "active"_a = true)
        .def_method(Endpoint, pdf_direction, "it"_a, "ds"_a, "active"_a = true)
        .def_method(Endpoint, eval_direction, "it"_a, "ds"_a, "active"_a = true)
        .def_method(Endpoint, sample_position, "ref"_a, "ds"_a, "active"_a = true)
        .def_method(Endpoint, pdf_position, "ps"_a, "active"_a = true)
        .def_method(Endpoint, eval, "si"_a, "active"_a = true)
        .def_method(Endpoint, sample_wavelengths, "si"_a, "sample"_a, "active"_a = true)
        .def_method(Endpoint, world_transform)
        .def_method(Endpoint, needs_sample_2)
        .def_method(Endpoint, needs_sample_3)
        .def("get_shape",  nb::overload_cast<>(&Endpoint::shape, nb::const_),  D(Endpoint, shape))
        .def("get_medium", nb::overload_cast<>(&Endpoint::medium, nb::const_), D(Endpoint, medium))
        .def_method(Endpoint, set_shape, "shape"_a)
        .def_method(Endpoint, set_medium, "medium"_a)
        .def_method(Endpoint, set_scene, "scene"_a)
        .def_method(Endpoint, bbox);
}
