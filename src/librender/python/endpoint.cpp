#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/shape.h>

MTS_PY_EXPORT_VARIANTS(Endpoint) {
    auto endpoint = MTS_PY_CLASS(Endpoint, Object)
        .mdef(Endpoint, sample_ray,
              "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true)
        .mdef(Endpoint, sample_direction, "it"_a, "sample"_a, "active"_a = true)
        .mdef(Endpoint, pdf_direction, "it"_a, "ds"_a, "active"_a = true)
        .mdef(Endpoint, eval, "si"_a, "active"_a = true)
        .mdef(Endpoint, world_transform)
        .mdef(Endpoint, needs_sample_2)
        .mdef(Endpoint, needs_sample_3)
        .def("shape", py::overload_cast<>(&Endpoint::shape, py::const_), D(Endpoint, shape))
        .def("medium", py::overload_cast<>(&Endpoint::medium, py::const_), D(Endpoint, medium))
        .mdef(Endpoint, set_shape, "shape"_a)
        .mdef(Endpoint, set_medium, "medium"_a)
        .mdef(Endpoint, bbox);

    if constexpr (is_array_v<Float> && !is_dynamic_v<Float>) {
        endpoint.def("sample_ray", enoki::vectorize_wrapper(&Endpoint::sample_ray),
                     "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true,
                     D(Endpoint, sample_ray))
                .def("sample_direction", enoki::vectorize_wrapper(&Endpoint::sample_direction),
                     "it"_a, "sample"_a, "active"_a = true, D(Endpoint, sample_direction))
                .def("pdf_direction", enoki::vectorize_wrapper(&Endpoint::pdf_direction),
                     "it"_a, "ds"_a, "active"_a = true, D(Endpoint, pdf_direction))
                .def("eval", enoki::vectorize_wrapper(&Endpoint::eval),
                     "si"_a, "active"_a = true, D(Endpoint, eval))
                ;
    }
}
