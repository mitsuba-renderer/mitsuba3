#include <mitsuba/python/python.h>
#include <mitsuba/render/records.h>

MTS_PY_EXPORT_VARIANTS(PositionSample) {
    using SurfaceInteraction3f = typename PositionSample::SurfaceInteraction3f;
    py::class_<PositionSample>(m, "PositionSample", D(PositionSample))
        .def(py::init<>(), "Construct an unitialized position sample")
        .def(py::init<const PositionSample &>(), "Copy constructor", "other"_a)
        .def(py::init<const SurfaceInteraction3f &>(), "si"_a, D(PositionSample, PositionSample))
        .rwdef(PositionSample, p)
        .rwdef(PositionSample, n)
        .rwdef(PositionSample, uv)
        .rwdef(PositionSample, time)
        .rwdef(PositionSample, pdf)
        .rwdef(PositionSample, delta)
        .rwdef(PositionSample, object)
        .repr_def(PositionSample)
        ;
}

MTS_PY_EXPORT_VARIANTS(DirectionSample) {
    using Base = typename DirectionSample::Base;
    using SurfaceInteraction3f = typename DirectionSample::SurfaceInteraction3f;
    using Interaction3f = typename DirectionSample::Interaction3f;
    using Point3f = typename DirectionSample::Point3f;
    using Normal3f = typename DirectionSample::Normal3f;
    using Vector3f = typename DirectionSample::Vector3f;
    using Point2f = typename DirectionSample::Point2f;
    using ObjectPtr = typename DirectionSample::ObjectPtr;
    using Mask = typename DirectionSample::Mask;

    py::class_<DirectionSample, Base>(m, "DirectionSample", D(DirectionSample))
        .def(py::init<>(), "Construct an unitialized direct sample")
        .def(py::init<const Base &>(), "Construct from a position sample", "other"_a)
        .def(py::init<const DirectionSample &>(), "Copy constructor", "other"_a)
        .def(py::init<const Point3f &, const Normal3f &, const Point2f &,
                      const Float &, const Float &, const Mask &,
                      const ObjectPtr &, const Vector3f &, const Float &>(),
             "p"_a, "n"_a, "uv"_a, "time"_a, "pdf"_a, "delta"_a, "object"_a, "d"_a, "dist"_a,
             "Element-by-element constructor")
        .def(py::init<const SurfaceInteraction3f &, const Interaction3f &>(),
             "si"_a, "ref"_a, D(PositionSample, PositionSample))
        .mdef(DirectionSample, set_query)
        .rwdef(DirectionSample, d)
        .rwdef(DirectionSample, dist)
        .repr_def(DirectionSample)
        ;
}

// TODO
// MTS_PY_EXPORT_VARIANTS(RadianceSample) {
//     auto rs = py::class_<RadianceSample>(m, name, D(RadianceSample))
//         .def(py::init<>(), D(RadianceSample, RadianceSample))
//         .def(py::init<const Scene *, Sampler *>(),
//              D(RadianceSample, RadianceSample, 2), "scene"_a, "sampler"_a)
//         // .def(py::init<const RadianceSample &>(), D(RadianceSample, RadianceSample, 3),
//         //   "other"_a)
//         .mdef(RadianceSample, ray_intersect, "ray"_a, "active"_a)
//         .mdef(RadianceSample, next_1d)
//         .mdef(RadianceSample, next_2d)

//         .rwdef(RadianceSample, scene)
//         .rwdef(RadianceSample, sampler)
//         .rwdef(RadianceSample, si)
//         .rwdef(RadianceSample, alpha)
//         .repr_def(RadianceSample)
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
