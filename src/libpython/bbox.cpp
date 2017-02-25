#include <mitsuba/core/bbox.h>
#include "python.h"

template <typename BBox> void bind_bbox(py::module &m, const char *name) {
    using Point = typename BBox::Point;

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
             }, D(BoundingBox, contains), "p"_a, "strict"_a = false)
        .def("contains",
             [](const BBox &self, const BBox &bbox, bool strict) {
                 return strict ? self.template contains<true>(bbox)
                               : self.template contains<false>(bbox);
             }, D(BoundingBox, contains, 2), "bbox"_a, "strict"_a = false)
        .def("overlaps",
             [](const BBox &self, const BBox &bbox, bool strict) {
                 return strict ? self.template overlaps<true>(bbox)
                               : self.template overlaps<false>(bbox);
             },
             D(BoundingBox, overlaps), "bbox"_a, "strict"_a = false)
        .def("squared_distance",
             py::overload_cast<const Point &>(&BBox::squared_distance, py::const_),
             D(BoundingBox, squared_distance))
        .def("squared_distance",
             py::overload_cast<const BBox &>(&BBox::squared_distance, py::const_),
             D(BoundingBox, squared_distance, 2))
        .def("distance",
             py::overload_cast<const Point &>(&BBox::distance, py::const_),
             D(BoundingBox, distance))
        .def("distance",
             py::overload_cast<const BBox &>(&BBox::distance, py::const_),
             D(BoundingBox, distance, 2))
        .def("reset", &BBox::reset, D(BoundingBox, reset))
        .def("clip", &BBox::clip, D(BoundingBox, clip))
        .def("expand", py::overload_cast<const Point &>(&BBox::expand),
             D(BoundingBox, expand))
        .def("expand", py::overload_cast<const BBox &>(&BBox::expand),
             D(BoundingBox, expand, 2))
        .def("ray_intersect", &BBox::template ray_intersect<Ray3f>,
             D(BoundingBox, ray_intersect))
#if 0
        .def("ray_intersect", [](const BBox &bbox, const Ray3fX &r) {
                typename Ray3fX::Value mint, maxt;
                typename Ray3fX::Value::Mask status;

                size_t size = dynamic_size(r);
                dynamic_resize(mint, size);
                dynamic_resize(maxt, size);
                dynamic_resize(status, size);

                vectorize(
                    [&bbox](auto &&r, auto &&status, auto &&mint, auto &&maxt) {
                        std::tie(status, mint, maxt) = bbox.template ray_intersect<Ray3fP>(r);
                    }, r, status, mint, maxt);

                return std::make_tuple(std::move(status), std::move(mint), std::move(maxt));
             },
             D(BoundingBox, ray_intersect))
#endif
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

MTS_PY_EXPORT(BoundingBox) {
    bind_bbox<BoundingBox3f>(m, "BoundingBox3f");
}

