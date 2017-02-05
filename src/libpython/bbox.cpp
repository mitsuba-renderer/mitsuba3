#include <mitsuba/core/bbox.h>
#include "python.h"

template <typename Type> void bind_bbox(py::module &m, const char *name) {
    typedef typename Type::Point Point;
    typedef typename Type::Scalar Scalar;

    py::class_<Type>(m, name, D(TBoundingBox))
        .def(py::init<>(), D(TBoundingBox, TBoundingBox))
        .def(py::init<Point>(), D(TBoundingBox, TBoundingBox, 2))
        .def(py::init<Point, Point>(), D(TBoundingBox, TBoundingBox, 3))
        .def(py::init<const Type &>(), "Copy constructor")
        .def("valid", &Type::valid, D(TBoundingBox, valid))
        .def("collapsed", &Type::collapsed, D(TBoundingBox, collapsed))
        .def("major_axis", &Type::major_axis, D(TBoundingBox, major_axis))
        .def("minor_axis", &Type::minor_axis, D(TBoundingBox, minor_axis))
        .def("center", &Type::center, D(TBoundingBox, center))
        .def("extents", &Type::extents, D(TBoundingBox, extents))
        .def("corner", &Type::corner, D(TBoundingBox, corner))
        .def("volume", &Type::volume, D(TBoundingBox, volume))
        .def("surface_area", &Type::template surface_area<>, D(TBoundingBox, surface_area))
        .def("contains", [](const Type &self, const Point &p, bool strict) {
                return strict ? self.template contains<true>(p) : self.template contains<false>(p);
            }, D(TBoundingBox, contains), "p"_a, "strict"_a = false)
        .def("contains", [](const Type &self, const Type &bbox, bool strict) {
                return strict ? self.template contains<true>(bbox) : self.template contains<false>(bbox);
            }, D(TBoundingBox, contains, 2), "bbox"_a, "strict"_a = false)
        .def("overlaps", [](const Type &self, const Type &bbox, bool strict) {
                return strict ? self.template overlaps<true>(bbox) : self.template overlaps<false>(bbox);
            }, D(TBoundingBox, overlaps), "bbox"_a, "strict"_a = false)
        .def("squared_distance", (Scalar (Type::*)(const Point &) const) &Type::squared_distance,
             D(TBoundingBox, squared_distance))
        .def("squared_distance", (Scalar (Type::*)(const Type &bbox) const) &Type::squared_distance,
             D(TBoundingBox, squared_distance, 2))
        .def("distance", (Scalar (Type::*)(const Point &) const) &Type::distance,
             D(TBoundingBox, distance))
        .def("distance", (Scalar (Type::*)(const Type &bbox) const) &Type::distance,
             D(TBoundingBox, distance, 2))
        .def("reset", &Type::reset, D(TBoundingBox, reset))
        .def("clip", &Type::clip, D(TBoundingBox, clip))
        .def("expand", (void (Type::*)(const Point &)) &Type::expand, D(TBoundingBox, expand))
        .def("expand", (void (Type::*)(const Type &)) &Type::expand, D(TBoundingBox, expand, 2))
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__repr__", [](const Type &bbox) { std::ostringstream oss; oss << bbox; return oss.str(); })
        .def_static("merge", &Type::merge, D(TBoundingBox, merge))
        .def_readwrite("min", &Type::min)
        .def_readwrite("max", &Type::max);
}

MTS_PY_EXPORT(BoundingBox) {
    bind_bbox<BoundingBox3f>(m, "BoundingBox3f");
}

