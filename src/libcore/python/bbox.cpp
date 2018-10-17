#include <mitsuba/core/bbox.h>
#include <mitsuba/python/python.h>

template <typename BBox> void bind_bbox(py::module &m, const char *name) {
    using Point = typename BBox::Point;
    using Value = value_t<Point>;

    py::class_<BBox>(m, name, D(BoundingBox))
        .def(py::init<>(), D(BoundingBox, BoundingBox))
        .def(py::init<Point>(), D(BoundingBox, BoundingBox, 2))
        .def(py::init<Point, Point>(), D(BoundingBox, BoundingBox, 3))
        .def(py::init<const BBox &>(), "Copy constructor")
        .def("valid", &BBox::valid, D(BoundingBox, valid))
        .def("collapsed", &BBox::collapsed, D(BoundingBox, collapsed))
        .def("major_axis", &BBox::major_axis, D(BoundingBox, major_axis))
        .def("minor_axis", &BBox::minor_axis, D(BoundingBox, minor_axis))
        .def("center", &BBox::center, D(BoundingBox, center))
        .def("extents", &BBox::extents, D(BoundingBox, extents))
        .def("corner", &BBox::corner, D(BoundingBox, corner))
        .def("volume", &BBox::volume, D(BoundingBox, volume))
        .def("surface_area", &BBox::template surface_area<>,
             D(BoundingBox, surface_area))
        .def("contains",
             [](const BBox &self, const Point &p, bool strict) {
                 return strict ? self.template contains<true>(p)
                               : self.template contains<false>(p);
             },
             D(BoundingBox, contains), "p"_a, "strict"_a = false)
        .def("contains",
             [](const BBox &self, const BBox &bbox, bool strict) {
                 return strict ? self.template contains<true>(bbox)
                               : self.template contains<false>(bbox);
             },
             D(BoundingBox, contains, 2), "bbox"_a, "strict"_a = false)
        .def("overlaps",
             [](const BBox &self, const BBox &bbox, bool strict) {
                 return strict ? self.template overlaps<true>(bbox)
                               : self.template overlaps<false>(bbox);
             },
             D(BoundingBox, overlaps), "bbox"_a, "strict"_a = false)
        .def("squared_distance",
             py::overload_cast<const mitsuba::Point<Value, Point::Size> &>(
                 &BBox::template squared_distance<Value>, py::const_),
             D(BoundingBox, squared_distance))
        .def("squared_distance",
             (Value (BBox::*)(const BBox &) const)(&BBox::squared_distance),
             D(BoundingBox, squared_distance, 2))
        .def("distance",
             py::overload_cast<const mitsuba::Point<Value, Point::Size> &>(
                 &BBox::template distance<Value>, py::const_),
             D(BoundingBox, distance))
        .def("distance",
             (Value (BBox::*)(const BBox &) const)(&BBox::distance),
             D(BoundingBox, distance, 2))
        .def("reset", &BBox::reset, D(BoundingBox, reset))
        .def("clip", &BBox::clip, D(BoundingBox, clip))
        .def("expand", py::overload_cast<const Point &>(&BBox::expand),
             D(BoundingBox, expand))
        .def("expand", py::overload_cast<const BBox &>(&BBox::expand),
             D(BoundingBox, expand, 2))
        .def("ray_intersect", &BBox::template ray_intersect<Ray3f>,
             D(BoundingBox, ray_intersect))
        .def("ray_intersect",
             vectorize_wrapper(&BBox::template ray_intersect<Ray3fP>))
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__repr__",
             [](const BBox &bbox) {
                 std::ostringstream oss;
                 oss << bbox;
                 return oss.str();
             })
        .def_static("merge", &BBox::merge, D(BoundingBox, merge))
        .def_readwrite("min", &BBox::min)
        .def_readwrite("max", &BBox::max);
}

MTS_PY_EXPORT(BoundingBox) { bind_bbox<BoundingBox3f>(m, "BoundingBox3f"); }
