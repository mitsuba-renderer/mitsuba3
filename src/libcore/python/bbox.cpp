#include <mitsuba/core/bbox.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT_VARIANTS(BoundingBox) {
    MTS_IMPORT_CORE_TYPES()
    using Ray3f = Ray<Point3f, Spectrum>;
    MTS_PY_CHECK_ALIAS(BoundingBox3f)

    py::class_<BoundingBox3f>(m, "BoundingBox3f", D(BoundingBox3f))
        .def(py::init<>(), D(BoundingBox3f, BoundingBox3f))
        .def(py::init<Point3f>(), D(BoundingBox3f, BoundingBox3f, 2))
        .def(py::init<Point3f, Point3f>(), D(BoundingBox3f, BoundingBox3f, 3))
        .def(py::init<const BoundingBox3f &>(), "Copy constructor")
        .def_method(BoundingBox3f, valid)
        .def_method(BoundingBox3f, collapsed)
        .def_method(BoundingBox3f, major_axis)
        .def_method(BoundingBox3f, minor_axis)
        .def_method(BoundingBox3f, center)
        .def_method(BoundingBox3f, extents)
        .def_method(BoundingBox3f, corner)
        .def_method(BoundingBox3f, volume)
        .def("surface_area", &BoundingBox3f::template surface_area<>,
            D(BoundingBox3f, surface_area))
        .def("contains",
            [](const BoundingBox3f &self, const Point3f &p, bool strict) {
                return strict ? self.template contains<true>(p)
                                : self.template contains<false>(p);
            },
            D(BoundingBox3f, contains), "p"_a, "strict"_a = false)
        .def("contains",
            [](const BoundingBox3f &self, const BoundingBox3f &bbox, bool strict) {
                return strict ? self.template contains<true>(bbox)
                                : self.template contains<false>(bbox);
            },
            D(BoundingBox3f, contains, 2), "bbox"_a, "strict"_a = false)
        .def("overlaps",
            [](const BoundingBox3f &self, const BoundingBox3f &bbox, bool strict) {
                return strict ? self.template overlaps<true>(bbox)
                                : self.template overlaps<false>(bbox);
            },
            D(BoundingBox3f, overlaps), "bbox"_a, "strict"_a = false)
        .def("squared_distance",
            py::overload_cast<const Point3f &>(
                &BoundingBox3f::template squared_distance<Float>, py::const_),
            D(BoundingBox3f, squared_distance))
        .def("squared_distance",
            (Float (BoundingBox3f::*)(const BoundingBox3f &) const)(&BoundingBox3f::squared_distance),
            D(BoundingBox3f, squared_distance, 2))
        .def("distance",
            py::overload_cast<const Point3f &>(
                &BoundingBox3f::template distance<Float>, py::const_),
            D(BoundingBox3f, distance))
        .def("distance",
            (Float (BoundingBox3f::*)(const BoundingBox3f &) const)(&BoundingBox3f::distance),
            D(BoundingBox3f, distance, 2))
        .def("reset", &BoundingBox3f::reset, D(BoundingBox3f, reset))
        .def("clip", (void (BoundingBox3f::*)(const BoundingBox3f &)) &BoundingBox3f::clip,
            D(BoundingBox3f, clip))
        .def("expand", (void (BoundingBox3f::*)(const Point3f &)) &BoundingBox3f::expand,
            D(BoundingBox3f, expand))
        .def("expand", (void (BoundingBox3f::*)(const BoundingBox3f &)) &BoundingBox3f::expand,
            D(BoundingBox3f, expand, 2))
        // TODO
        // .def("ray_intersect", vectorize<Float>(&BoundingBox3f::template ray_intersect<Ray3f>),
            // D(BoundingBox3f, ray_intersect))
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_static("merge", &BoundingBox3f::merge, D(BoundingBox3f, merge))
        .def_readwrite("min", &BoundingBox3f::min)
        .def_readwrite("max", &BoundingBox3f::max)
        .def_repr(BoundingBox3f)
        ;
}
