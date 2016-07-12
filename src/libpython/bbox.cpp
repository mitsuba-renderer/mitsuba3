#include <mitsuba/core/bbox.h>
#include "python.h"

template <typename Type> void bind_bbox(py::module &m, const char *name) {
    typedef typename Type::Point Point;
    typedef typename Type::Scalar Scalar;

    py::class_<Type>(m, name, DM(TBoundingBox))
        .def(py::init<>(), DM(TBoundingBox, TBoundingBox))
        .def(py::init<Point>(), DM(TBoundingBox, TBoundingBox, 2))
        .def(py::init<Point, Point>(), DM(TBoundingBox, TBoundingBox, 3))
        .def(py::init<const Type &>(), "Copy constructor")
        .def("valid", &Type::valid, DM(TBoundingBox, valid))
        .def("collapsed", &Type::collapsed, DM(TBoundingBox, collapsed))
        .def("majorAxis", &Type::majorAxis, DM(TBoundingBox, majorAxis))
        .def("minorAxis", &Type::minorAxis, DM(TBoundingBox, minorAxis))
        .def("center", &Type::center, DM(TBoundingBox, center))
        .def("extents", &Type::extents, DM(TBoundingBox, extents))
        .def("corner", &Type::corner, DM(TBoundingBox, corner))
        .def("volume", &Type::volume, DM(TBoundingBox, volume))
        .def("surfaceArea", &Type::template surfaceArea<>, DM(TBoundingBox, surfaceArea))
        .def("contains", [](const Type &self, const Point &p, bool strict) {
                return strict ? self.template contains<true>(p) : self.template contains<false>(p);
            }, DM(TBoundingBox, contains), py::arg("p"), py::arg("strict") = false)
        .def("contains", [](const Type &self, const Type &bbox, bool strict) {
                return strict ? self.template contains<true>(bbox) : self.template contains<false>(bbox);
            }, DM(TBoundingBox, contains, 2), py::arg("bbox"), py::arg("strict") = false)
        .def("overlaps", [](const Type &self, const Type &bbox, bool strict) {
                return strict ? self.template overlaps<true>(bbox) : self.template overlaps<false>(bbox);
            }, DM(TBoundingBox, overlaps), py::arg("bbox"), py::arg("strict") = false)
        .def("squaredDistance", (Scalar (Type::*)(const Point &) const) &Type::squaredDistance,
             DM(TBoundingBox, squaredDistance))
        .def("squaredDistance", (Scalar (Type::*)(const Type &bbox) const) &Type::squaredDistance,
             DM(TBoundingBox, squaredDistance, 2))
        .def("distance", (Scalar (Type::*)(const Point &) const) &Type::distance,
             DM(TBoundingBox, distance))
        .def("distance", (Scalar (Type::*)(const Type &bbox) const) &Type::distance,
             DM(TBoundingBox, distance, 2))
        .def("reset", &Type::reset, DM(TBoundingBox, reset))
        .def("clip", &Type::clip, DM(TBoundingBox, clip))
        .def("expand", (void (Type::*)(const Point &)) &Type::expand, DM(TBoundingBox, expand))
        .def("expand", (void (Type::*)(const Type &)) &Type::expand, DM(TBoundingBox, expand, 2))
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__repr__", [](const Type &bbox) { std::ostringstream oss; oss << bbox; return oss.str(); })
        .def_static("merge", &Type::merge, DM(TBoundingBox, merge))
        .def_readwrite("min", &Type::min)
        .def_readwrite("max", &Type::max);
}

MTS_PY_EXPORT(BoundingBox) {
    bind_bbox<BoundingBox3f>(m, "BoundingBox3f");
}

