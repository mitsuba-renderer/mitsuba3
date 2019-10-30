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
        .mdef(BoundingBox, valid)
        .mdef(BoundingBox, collapsed)
        .mdef(BoundingBox, major_axis)
        .mdef(BoundingBox, minor_axis)
        .mdef(BoundingBox, center)
        .mdef(BoundingBox, extents)
        .mdef(BoundingBox, corner)
        .mdef(BoundingBox, volume)
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
        .def("clip", (void (BBox::*)(const BBox &)) &BBox::clip,
             D(BoundingBox, clip))
        .def("expand", (void (BBox::*)(const Point &)) &BBox::expand,
             D(BoundingBox, expand))
        .def("expand", (void (BBox::*)(const BBox &)) &BBox::expand,
             D(BoundingBox, expand, 2))
        .def("ray_intersect", &BBox::template ray_intersect<Ray3f>,
             D(BoundingBox, ray_intersect))
        .def("ray_intersect",
             vectorize_wrapper(&BBox::template ray_intersect<Ray3fP>))
        .def(py::self == py::self)
        .def(py::self != py::self)
        .repr_def(BBox)
        .def_static("merge", &BBox::merge, D(BoundingBox, merge))
        .def_readwrite("min", &BBox::min)
        .def_readwrite("max", &BBox::max);
}

MTS_PY_EXPORT(BoundingBox) { bind_bbox<BoundingBox3f>(m, "BoundingBox3f"); }
