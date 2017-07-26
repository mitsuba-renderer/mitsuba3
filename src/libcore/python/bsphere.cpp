#include <mitsuba/core/bsphere.h>
#include <mitsuba/python/python.h>

template <typename BSphere> auto bind_bsphere(py::module &m, const char *name) {
    auto bsphere = py::class_<BSphere>(m, name, D(BoundingSphere))
        .def(py::init<>(), D(BoundingSphere, BoundingSphere))
        .def_readwrite("center", &BSphere::center)
        .def_readwrite("radius", &BSphere::radius)
        .def("__repr__", [](const BSphere &bbox) {
                std::ostringstream oss;
                oss << bbox;
                return oss.str();
            }
        );

    return bsphere;
}

MTS_PY_EXPORT(BoundingSphere) {
    bind_bsphere<BoundingSphere3f>(m, "BoundingSphere3f")
        .def(py::init<Point3f, Float>(), D(BoundingSphere, BoundingSphere, 2))
        .def(py::init<const BoundingSphere3f &>())
        .def("empty", &BoundingSphere3f::empty, D(BoundingSphere, empty))
        .def("contains",
            [](const BoundingSphere3f &self, const Point3f &p, bool strict) {
                return strict ? self.template contains<true>(p)
                              : self.template contains<false>(p);
            }, D(BoundingSphere, contains), "p"_a, "strict"_a = false)
        .def("expand", &BoundingSphere3f::expand, D(BoundingSphere, expand))
        .def("ray_intersect", &BoundingSphere3f::template ray_intersect<Point3f>,
            D(BoundingSphere, ray_intersect))
        .def("ray_intersect", vectorize_wrapper(
            &BoundingSphere3f::template ray_intersect<Point3fP>))
        .def(py::self == py::self)
        .def(py::self != py::self);
}
