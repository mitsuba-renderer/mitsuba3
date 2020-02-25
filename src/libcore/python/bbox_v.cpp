#include <mitsuba/core/bbox.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/python/python.h>

template <typename BBox, typename Ray> auto bind_bbox(py::module &m, const char *name) {
        using Point = typename BBox::Point;
        using Float = typename BBox::Value;

        MTS_PY_CHECK_ALIAS(BBox, name) {
            auto bbox = py::class_<BBox>(m, name, D(BoundingBox))
                .def(py::init<>(), D(BoundingBox, BoundingBox))
                .def(py::init<Point>(), D(BoundingBox, BoundingBox, 2), "p"_a)
                .def(py::init<Point, Point>(), D(BoundingBox, BoundingBox, 3), "min"_a, "max"_a)
                .def(py::init<const BBox &>(), "Copy constructor")
                .def("valid",        &BBox::valid,        D(BoundingBox, valid))
                .def("collapsed",    &BBox::collapsed,    D(BoundingBox, collapsed))
                .def("major_axis",   &BBox::major_axis,   D(BoundingBox, major_axis))
                .def("minor_axis",   &BBox::minor_axis,   D(BoundingBox, minor_axis))
                .def("center",       &BBox::center,       D(BoundingBox, center))
                .def("extents",      &BBox::extents,      D(BoundingBox, extents))
                .def("corner",       &BBox::corner,       D(BoundingBox, corner))
                .def("volume",       &BBox::volume,       D(BoundingBox, volume))
                .def("surface_area", &BBox::surface_area, D(BoundingBox, surface_area))
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
                    py::overload_cast<const Point &>(
                        &BBox::template squared_distance<Float>, py::const_),
                    D(BoundingBox, squared_distance))
                .def("squared_distance",
                    py::overload_cast<const BBox &>(
                        &BBox::template squared_distance<Float>, py::const_),
                    D(BoundingBox, squared_distance, 2))
                .def("distance",
                    py::overload_cast<const Point &>(
                        &BBox::template distance<Float>, py::const_),
                    D(BoundingBox, distance))
                .def("distance",
                    py::overload_cast<const BBox &>(
                        &BBox::template distance<Float>, py::const_),
                    D(BoundingBox, distance, 2))
                .def("reset", &BBox::reset, D(BoundingBox, reset))
                .def("clip", (void (BBox::*)(const BBox &)) &BBox::clip,
                    D(BoundingBox, clip))
                .def("expand", (void (BBox::*)(const Point &)) &BBox::expand,
                    D(BoundingBox, expand))
                .def("expand", (void (BBox::*)(const BBox &)) &BBox::expand,
                    D(BoundingBox, expand, 2))
                .def(py::self == py::self)
                .def(py::self != py::self)
                .def_static("merge", &BBox::merge, D(BoundingBox, merge))
                .def_readwrite("min", &BBox::min)
                .def_readwrite("max", &BBox::max)
                .def_repr(BBox);

            if constexpr (array_size_v<Point> == 3)
                bbox.def("ray_intersect",
                         [](const BBox &self, const Ray &ray) {
                             return self.ray_intersect(ray);
                         }, D(BoundingBox, ray_intersect), "ray"_a);
    }
}

MTS_PY_EXPORT(BoundingBox) {
    MTS_PY_IMPORT_TYPES_DYNAMIC()

    bind_bbox<BoundingBox2f, Ray3f>(m, "BoundingBox2f");
    bind_bbox<BoundingBox3f, Ray3f>(m, "BoundingBox3f");

    if constexpr (!std::is_same_v<Float, ScalarFloat>) {
        bind_bbox<ScalarBoundingBox2f, Ray3f>(m, "ScalarBoundingBox2f");
        bind_bbox<ScalarBoundingBox3f, Ray3f>(m, "ScalarBoundingBox3f");
    }
}
