#include <mitsuba/python/python.h>
#include <mitsuba/render/records.h>

MTS_PY_EXPORT_MODE_VARIANTS(PositionSample) {
    MTS_IMPORT_TYPES()

    py::class_<PositionSample3f>(m, "PositionSample3f", D(PositionSample3f))
        .def(py::init<>(), "Construct an unitialized position sample")
        .def(py::init<const PositionSample3f &>(), "Copy constructor", "other"_a)
        .def(py::init<const SurfaceInteraction3f &>(), "si"_a, D(PositionSample3f, PositionSample3f))
        .def_field(PositionSample3f, p)
        .def_field(PositionSample3f, n)
        .def_field(PositionSample3f, uv)
        .def_field(PositionSample3f, time)
        .def_field(PositionSample3f, pdf)
        .def_field(PositionSample3f, delta)
        .def_field(PositionSample3f, object)
        .def_repr(PositionSample3f)
        ;
}

MTS_PY_EXPORT_MODE_VARIANTS(DirectionSample) {
    MTS_IMPORT_TYPES()
    MTS_IMPORT_OBJECT_TYPES()
    using Base = typename DirectionSample3f::Base;

    py::class_<DirectionSample3f, Base>(m, "DirectionSample3f", D(DirectionSample3f))
        .def(py::init<>(), "Construct an unitialized direct sample")
        .def(py::init<const Base &>(), "Construct from a position sample", "other"_a)
        .def(py::init<const DirectionSample3f &>(), "Copy constructor", "other"_a)
        .def(py::init<const Point3f &, const Normal3f &, const Point2f &,
                      const Float &, const Float &, const Mask &,
                      const ObjectPtr &, const Vector3f &, const Float &>(),
             "p"_a, "n"_a, "uv"_a, "time"_a, "pdf"_a, "delta"_a, "object"_a, "d"_a, "dist"_a,
             "Element-by-element constructor")
        .def(py::init<const SurfaceInteraction3f &, const Interaction3f &>(),
             "si"_a, "ref"_a, D(PositionSample3f, PositionSample3f))
        .def_method(DirectionSample3f, set_query)
        .def_field(DirectionSample3f, d)
        .def_field(DirectionSample3f, dist)
        .def_repr(DirectionSample3f)
        ;
}

// TODO vectorize wrapper bindings
// MTS_PY_EXPORT_MODE_VARIANTS(RadianceSample) {
//     auto rs = py::class_<RadianceSample>(m, name, D(RadianceSample))
//         .def(py::init<>(), D(RadianceSample, RadianceSample))
//         .def(py::init<const Scene *, Sampler *>(),
//              D(RadianceSample, RadianceSample, 2), "scene"_a, "sampler"_a)
//         // .def(py::init<const RadianceSample &>(), D(RadianceSample, RadianceSample, 3),
//         //   "other"_a)
//         .def_method(RadianceSample, ray_intersect, "ray"_a, "active"_a)
//         .def_method(RadianceSample, next_1d)
//         .def_method(RadianceSample, next_2d)

//         .def_field(RadianceSample, scene)
//         .def_field(RadianceSample, sampler)
//         .def_field(RadianceSample, si)
//         .def_field(RadianceSample, alpha)
//         .def_repr(RadianceSample)
//         ;

//     if constexpr (is_array_v<Float> && !is_dynamic_v<Float>) {
//           rs.def("ray_intersect", enoki::vectorize_wrapper(&RadianceSample::ray_intersect),
//                  "ray"_a, "active"_a, D(RadianceSample, ray_intersect))
//             .def("next_1d", enoki::vectorize_wrapper(&RadianceSample::next_1d),
//                  D(RadianceSample, next_1d))
//             .def("next_2d", enoki::vectorize_wrapper(&RadianceSample::next_2d),
//                  D(RadianceSample, next_2d))
//     }
// }
