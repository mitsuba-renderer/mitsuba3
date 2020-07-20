#include <mitsuba/render/shape.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/python/python.h>

template<typename Class, typename PyClass>
void bind_set_object(PyClass &cl) {
    using Float = typename Class::Float;
    using UInt64  =  uint64_array_t<Float>;
    using ObjectPtr = replace_scalar_t<Float, const Object *>;

    if constexpr (is_array_v<Float>)
        cl.def("set_object", [](Class& ps, UInt64 ptr) { ps.object = static_cast<ObjectPtr>(ptr); });
}

MTS_PY_EXPORT(PositionSample) {
    MTS_PY_IMPORT_TYPES_DYNAMIC(ObjectPtr)
    auto pos = py::class_<PositionSample3f>(m, "PositionSample3f", D(PositionSample))
        .def(py::init<>(), "Construct an unitialized position sample")
        .def(py::init<const PositionSample3f &>(), "Copy constructor", "other"_a)
        .def(py::init<const SurfaceInteraction3f &>(),
            "si"_a, D(PositionSample, PositionSample))
        .def_readwrite("p",      &PositionSample3f::p,      D(PositionSample, p))
        .def_readwrite("n",      &PositionSample3f::n,      D(PositionSample, n))
        .def_readwrite("uv",     &PositionSample3f::uv,     D(PositionSample, uv))
        .def_readwrite("time",   &PositionSample3f::time,   D(PositionSample, time))
        .def_readwrite("pdf",    &PositionSample3f::pdf,    D(PositionSample, pdf))
        .def_readwrite("delta",  &PositionSample3f::delta,  D(PositionSample, delta))
        .def_readwrite("object", &PositionSample3f::object, D(PositionSample, object))
        .def_repr(PositionSample3f);

    bind_set_object<PositionSample3f>(pos);

    bind_slicing_operators<PositionSample3f, PositionSample<ScalarFloat, scalar_spectrum_t<Spectrum>>>(pos);
}

MTS_PY_EXPORT(DirectionSample) {
    MTS_PY_IMPORT_TYPES_DYNAMIC(ObjectPtr)
    auto pos = py::class_<DirectionSample3f, PositionSample3f>(m, "DirectionSample3f", D(DirectionSample))
        .def(py::init<>(), "Construct an unitialized direct sample")
        .def(py::init<const PositionSample3f &>(), "Construct from a position sample", "other"_a)
        .def(py::init<const DirectionSample3f &>(), "Copy constructor", "other"_a)
        .def(py::init<const Point3f &, const Normal3f &, const Point2f &,
                        const Float &, const Float &, const Mask &,
                        const ObjectPtr &, const Vector3f &, const Float &>(),
            "p"_a, "n"_a, "uv"_a, "time"_a, "pdf"_a, "delta"_a, "object"_a, "d"_a, "dist"_a,
            "Element-by-element constructor")
        .def(py::init<const SurfaceInteraction3f &, const Interaction3f &>(),
            "si"_a, "ref"_a, D(PositionSample, PositionSample))
        .def("set_query", &DirectionSample3f::set_query, "ray"_a, "si"_a, D(DirectionSample, set_query))
        .def_readwrite("d",     &DirectionSample3f::d,     D(DirectionSample, d))
        .def_readwrite("dist",  &DirectionSample3f::dist,  D(DirectionSample, dist))
        .def_repr(DirectionSample3f);
    bind_slicing_operators<DirectionSample3f, DirectionSample<ScalarFloat, scalar_spectrum_t<Spectrum>>>(pos);
}
