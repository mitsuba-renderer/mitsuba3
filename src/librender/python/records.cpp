#include <mitsuba/render/records.h>
#include <mitsuba/python/python.h>

using namespace pybind11::literals;

template <typename Point3>
auto bind_position_sample(py::module &m, const char *name) {
    using Type = PositionSample<Point3>;

    return py::class_<Type>(m, name, D(PositionSample))
        .def_readwrite("p", &Type::p, D(PositionSample, p))
        .def_readwrite("time", &Type::time, D(PositionSample, time))
        .def_readwrite("n", &Type::n, D(PositionSample, n))
        .def_readwrite("pdf", &Type::pdf, D(PositionSample, pdf))
        .def_readwrite("measure", &Type::measure, D(PositionSample, measure))
        .def_readwrite("uv", &Type::uv, D(PositionSample, uv))
        .def_readwrite("object", &Type::object, D(PositionSample, object))
        .def("__repr__", [](const Type &ps) {
            std::ostringstream oss;
            oss << ps;
            return oss.str();
        });
}

MTS_PY_EXPORT(PositionSample) {
    bind_position_sample<Point3f>(m, "PositionSample3f")
        .def(py::init<>(), D(PositionSample, PositionSample))
        .def(py::init<Float>(), D(PositionSample, PositionSample, 2))
        .def(py::init<const Intersection &, EMeasure>(),
             D(PositionSample, PositionSample, 3),
             "intersection"_a, "measure"_a=ESolidAngle);

    bind_position_sample<Point3fX>(m, "PositionSample3fX")
        .def("__init__",
             [](PositionSample3fX &ps, size_t n) {
                 new (&ps) PositionSample3fX();
                 enoki::set_slices(ps, n);
                 ps.object = zero<PositionSample3fX::ObjectPointer>(n);
             })
        .def("__getitem__",
             [](PositionSample3fX &ps, size_t i) {
                 if (i >= slices(ps))
                     throw py::index_error();
                 return PositionSample3f(enoki::slice(ps, i));
             })
        .def("__setitem__", [](PositionSample3fX &ps, size_t i,
                               const PositionSample3f &ps2) {
            if (i >= slices(ps))
                throw py::index_error();
            enoki::slice(ps, i) = ps2;
        });
}

// -----------------------------------------------------------------------------

template <typename Vector3> auto bind_direction_sample(py::module &m, const char *name) {
    using Type = DirectionSample<Vector3>;

    return py::class_<Type>(m, name, D(DirectionSample))
        .def_readwrite("d", &Type::d, D(DirectionSample, d))
        .def_readwrite("pdf", &Type::pdf, D(DirectionSample, pdf))
        .def_readwrite("measure", &Type::measure, D(DirectionSample, measure))
        .def("__repr__", [](const Type &record) {
            std::ostringstream oss;
            oss << record;
            return oss.str();
        });
}

MTS_PY_EXPORT(DirectionSample) {
    bind_direction_sample<Vector3f>(m, "DirectionSample3f")
        .def(py::init<>(), D(DirectionSample, DirectionSample))
        .def(py::init<const Vector3f &, EMeasure>(),
             D(DirectionSample, DirectionSample, 2),
             "d"_a, "measure"_a=ESolidAngle)
        .def(py::init<const Intersection &, EMeasure>(),
             D(DirectionSample, DirectionSample, 3),
             "intersection"_a, "measure"_a=ESolidAngle);

    bind_direction_sample<Vector3fX>(m, "DirectionSample3fX")
        .def("__init__",
             [](DirectionSample3fX &ds, size_t n) {
                 new (&ds) DirectionSample3fX();
                 enoki::set_slices(ds, n);
             })
        .def("__getitem__",
             [](DirectionSample3fX &ds, size_t i) {
                 if (i >= slices(ds))
                     throw py::index_error();
                 return DirectionSample3f(enoki::slice(ds, i));
             })
        .def("__setitem__", [](DirectionSample3fX &ds, size_t i,
                               const DirectionSample3f &ds2) {
            if (i >= slices(ds))
                throw py::index_error();
            enoki::slice(ds, i) = ds2;
        });
}

// -----------------------------------------------------------------------------

template <typename Point3, typename Base>
auto bind_direct_sample(py::module &m, const char *name) {
    using Type = DirectSample<Point3>;

    return py::class_<Type, Base>(m, name, D(DirectSample))
        .def_readwrite("ref_p", &Type::ref_p, D(DirectSample, ref_p))
        .def_readwrite("ref_n", &Type::ref_n, D(DirectSample, ref_n))
        .def_readwrite("d", &Type::d, D(DirectSample, d))
        .def_readwrite("dist", &Type::dist, D(DirectSample, dist))
        .def("__repr__", [](const Type &record) {
            std::ostringstream oss;
            oss << record;
            return oss.str();
        });
}

MTS_PY_EXPORT(DirectSample) {
    bind_direct_sample<Point3f, PositionSample3f>(m, "DirectSample3f")
        .def(py::init<>(), D(DirectSample, DirectSample))
        .def(py::init<const Point3f &, Float>(),
             D(DirectSample, DirectSample, 2))
        .def(py::init<const Intersection &>(), D(DirectSample, DirectSample, 3))
        .def(py::init<const MediumSample &>(), D(DirectSample, DirectSample, 4))
        .def(py::init<const Ray3f &, const Intersection, EMeasure>(),
             D(DirectSample, DirectSample, 4),
             "ray"_a, "intersection"_a, "measure"_a=ESolidAngle);

    bind_direct_sample<Point3fX, PositionSample3fX>(m, "DirectSample3fX")
        .def("__init__",
             [](DirectSample3fX &ds, size_t n) {
                 new (&ds) DirectSample3fX();
                 enoki::set_slices(ds, n);
                 ds.object = zero<PositionSample3fX::ObjectPointer>(n);
             })
        .def("__getitem__",
             [](DirectSample3fX &ds, size_t i) {
                 if (i >= slices(ds))
                     throw py::index_error();
                 return DirectSample3f(enoki::slice(ds, i));
             })
        .def("__setitem__", [](DirectSample3fX &ds, size_t i,
                               const DirectSample3f &ds2) {
            if (i >= slices(ds))
                throw py::index_error();
            enoki::slice(ds, i) = ds2;
        });
}
