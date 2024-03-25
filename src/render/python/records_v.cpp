#include <mitsuba/render/shape.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/string.h>
#include <nanobind/ndarray.h>

MI_PY_EXPORT(PositionSample) {
    MI_PY_IMPORT_TYPES()
    auto pos = nb::class_<PositionSample3f>(m, "PositionSample3f", D(PositionSample))
        .def(nb::init<>(), "Construct an uninitialized position sample")
        .def(nb::init<const PositionSample3f &>(), "Copy constructor", "other"_a)
        .def(nb::init<const SurfaceInteraction3f &>(),
            "si"_a, D(PositionSample, PositionSample))
        .def_rw("p",      &PositionSample3f::p,      D(PositionSample, p))
        .def_rw("n",      &PositionSample3f::n,      D(PositionSample, n))
        .def_rw("uv",     &PositionSample3f::uv,     D(PositionSample, uv))
        .def_rw("time",   &PositionSample3f::time,   D(PositionSample, time))
        .def_rw("pdf",    &PositionSample3f::pdf,    D(PositionSample, pdf))
        .def_rw("delta",  &PositionSample3f::delta,  D(PositionSample, delta))
        .def_repr(PositionSample3f);

    MI_PY_DRJIT_STRUCT(pos, PositionSample3f, p, n, uv, time, pdf, delta)
}

MI_PY_EXPORT(DirectionSample) {
    MI_PY_IMPORT_TYPES()
    auto pos = nb::class_<DirectionSample3f, PositionSample3f>(m, "DirectionSample3f", D(DirectionSample))
        .def(nb::init<>(), "Construct an uninitialized direct sample")
        .def(nb::init<const PositionSample3f &>(), "Construct from a position sample", "other"_a)
        .def(nb::init<const DirectionSample3f &>(), "Copy constructor", "other"_a)
        .def(nb::init<const Point3f &, const Normal3f &, const Point2f &,
                        const Float &, const Float &, const Mask &,
                        const Vector3f &, const Float &, const EmitterPtr &>(),
            "p"_a, "n"_a, "uv"_a, "time"_a, "pdf"_a, "delta"_a, "d"_a, "dist"_a,
            "emitter"_a, "Element-by-element constructor")
        .def(nb::init<const Scene *, const SurfaceInteraction3f &, const Interaction3f &>(),
            "scene"_a.none(), "si"_a, "ref"_a, D(PositionSample, PositionSample))
        .def_rw("d",     &DirectionSample3f::d,     D(DirectionSample, d))
        .def_rw("dist",  &DirectionSample3f::dist,  D(DirectionSample, dist))
        .def_rw("emitter", &DirectionSample3f::emitter, D(DirectionSample, emitter))
        .def_repr(DirectionSample3f);

    MI_PY_DRJIT_STRUCT(pos, DirectionSample3f, p, n, uv, time, pdf, delta, emitter, d, dist)
}
