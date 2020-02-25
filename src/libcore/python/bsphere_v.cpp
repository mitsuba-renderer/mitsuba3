#include <mitsuba/core/bsphere.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(BoundingSphere) {
    MTS_PY_IMPORT_TYPES_DYNAMIC()

    MTS_PY_CHECK_ALIAS(BoundingSphere3f, "BoundingSphere3f") {
        py::class_<BoundingSphere3f>(m, "BoundingSphere3f", D(BoundingSphere))
            .def(py::init<>(), D(BoundingSphere, BoundingSphere))
            .def(py::init<Point3f, Float>(), D(BoundingSphere, BoundingSphere, 2))
            .def(py::init<const BoundingSphere3f &>())
            .def("empty", &BoundingSphere3f::empty, D(BoundingSphere, empty))
            .def("contains",
                [](const BoundingSphere3f &self, const Point3f &p, bool strict) {
                    return strict ? self.template contains<true>(p)
                                  : self.template contains<false>(p);
                }, D(BoundingSphere, contains), "p"_a, "strict"_a = false)
            .def("expand", &BoundingSphere3f::expand, D(BoundingSphere, expand))
            .def("ray_intersect",
                [](const BoundingSphere3f &self, const Ray3f &ray) {
                    return self.ray_intersect(ray);
                }, D(BoundingSphere, ray_intersect), "ray"_a)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def_readwrite("center", &BoundingSphere3f::center)
            .def_readwrite("radius", &BoundingSphere3f::radius)
            .def_repr(BoundingSphere3f);
    }
}
