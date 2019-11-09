#include <mitsuba/core/bsphere.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT_VARIANTS(BoundingSphere) {
    MTS_IMPORT_CORE_TYPES()
    using Ray3f = Ray<Point3f, Spectrum>;
    MTS_PY_CHECK_ALIAS(BoundingSphere3f)

    py::class_<BoundingSphere3f>(m, "BoundingSphere3f", D(BoundingSphere3f))
        .def(py::init<>(), D(BoundingSphere3f, BoundingSphere3f))
        .def(py::init<Point3f, Float>(), D(BoundingSphere3f, BoundingSphere3f, 2))
        .def(py::init<const BoundingSphere3f &>())
        .def("empty", &BoundingSphere3f::empty, D(BoundingSphere3f, empty))
        .def("contains",
            [](const BoundingSphere3f &self, const Point3f &p, bool strict) {
                return strict ? self.template contains<true>(p)
                              : self.template contains<false>(p);
            }, D(BoundingSphere3f, contains), "p"_a, "strict"_a = false)
        .def("expand", &BoundingSphere3f::expand, D(BoundingSphere3f, expand))
        .def("ray_intersect", &BoundingSphere3f::template ray_intersect<Float, Spectrum>,
            D(BoundingSphere3f, ray_intersect))
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_readwrite("center", &BoundingSphere3f::center)
        .def_readwrite("radius", &BoundingSphere3f::radius)
        .def_repr(BoundingSphere3f)
        ;
}
