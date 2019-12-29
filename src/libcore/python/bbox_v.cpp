#include <mitsuba/core/bbox.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(BoundingBox) {
    MTS_PY_IMPORT_TYPES_DYNAMIC()
    py::class_<BoundingBox3f>(m, "BoundingBox3f", D(BoundingBox), py::module_local())
        .def(py::init<>(), D(BoundingBox, BoundingBox))
        .def(py::init<Point3f>(), D(BoundingBox, BoundingBox, 2))
        .def(py::init<Point3f, Point3f>(), D(BoundingBox, BoundingBox, 3))
        .def(py::init<const BoundingBox3f &>(), "Copy constructor")
        .def("valid",        &BoundingBox3f::valid,      D(BoundingBox, valid))
        .def("collapsed",    &BoundingBox3f::collapsed,  D(BoundingBox, collapsed))
        .def("major_axis",   &BoundingBox3f::major_axis, D(BoundingBox, major_axis))
        .def("minor_axis",   &BoundingBox3f::minor_axis, D(BoundingBox, minor_axis))
        .def("center",       &BoundingBox3f::center,     D(BoundingBox, center))
        .def("extents",      &BoundingBox3f::extents,    D(BoundingBox, extents))
        .def("corner",       &BoundingBox3f::corner,     D(BoundingBox, corner))
        .def("volume",       &BoundingBox3f::volume,     D(BoundingBox, volume))
        .def("surface_area", &BoundingBox3f::template surface_area<>,
            D(BoundingBox, surface_area))
        .def("contains",
            [](const BoundingBox3f &self, const Point3f &p, bool strict) {
                return strict ? self.template contains<true>(p)
                                : self.template contains<false>(p);
            },
            D(BoundingBox, contains), "p"_a, "strict"_a = false)
        .def("contains",
            [](const BoundingBox3f &self, const BoundingBox3f &bbox, bool strict) {
                return strict ? self.template contains<true>(bbox)
                                : self.template contains<false>(bbox);
            },
            D(BoundingBox, contains, 2), "bbox"_a, "strict"_a = false)
        .def("overlaps",
            [](const BoundingBox3f &self, const BoundingBox3f &bbox, bool strict) {
                return strict ? self.template overlaps<true>(bbox)
                                : self.template overlaps<false>(bbox);
            },
            D(BoundingBox, overlaps), "bbox"_a, "strict"_a = false)
        .def("squared_distance",
            py::overload_cast<const Point3f &>(
                &BoundingBox3f::template squared_distance<Float>, py::const_),
            D(BoundingBox, squared_distance))
        .def("squared_distance",
            (Float (BoundingBox3f::*)(const BoundingBox3f &) const)(&BoundingBox3f::squared_distance),
            D(BoundingBox, squared_distance, 2))
        .def("distance",
            py::overload_cast<const Point3f &>(
                &BoundingBox3f::template distance<Float>, py::const_),
            D(BoundingBox, distance))
        .def("distance",
            (Float (BoundingBox3f::*)(const BoundingBox3f &) const)(&BoundingBox3f::distance),
            D(BoundingBox, distance, 2))
        .def("reset", &BoundingBox3f::reset, D(BoundingBox, reset))
        .def("clip", (void (BoundingBox3f::*)(const BoundingBox3f &)) &BoundingBox3f::clip,
            D(BoundingBox, clip))
        .def("expand", (void (BoundingBox3f::*)(const Point3f &)) &BoundingBox3f::expand,
            D(BoundingBox, expand))
        .def("expand", (void (BoundingBox3f::*)(const BoundingBox3f &)) &BoundingBox3f::expand,
            D(BoundingBox, expand, 2))
        .def("ray_intersect", &BoundingBox3f::template ray_intersect<Ray3f>,
            D(BoundingBox, ray_intersect))
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_static("merge", &BoundingBox3f::merge, D(BoundingBox, merge))
        .def_readwrite("min", &BoundingBox3f::min)
        .def_readwrite("max", &BoundingBox3f::max)
        .def_repr(BoundingBox3f);
}
