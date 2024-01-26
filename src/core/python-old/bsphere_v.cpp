#include <mitsuba/core/bsphere.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/python/python.h>

template <typename BSphere, typename Ray> auto bind_bsphere(py::module &m, const char *name) {
        using Point = typename BSphere::Point;
        using Float = typename BSphere::Float;

        MI_PY_CHECK_ALIAS(BSphere, name) {
        py::class_<BSphere>(m, name, D(BoundingSphere))
            .def(py::init<>(), D(BoundingSphere, BoundingSphere))
            .def(py::init<Point, Float>(), D(BoundingSphere, BoundingSphere, 2))
            .def(py::init<const BSphere &>())
            .def("empty", &BSphere::empty, D(BoundingSphere, empty))
            .def("contains",
                [](const BSphere &self, const Point &p, bool strict) {
                    return strict ? self.template contains<true>(p)
                                  : self.template contains<false>(p);
                }, D(BoundingSphere, contains), "p"_a, "strict"_a = false)
            .def("expand", &BSphere::expand, D(BoundingSphere, expand))
            .def("ray_intersect",
                [](const BSphere &self, const Ray &ray) {
                    return self.ray_intersect(ray);
                }, D(BoundingSphere, ray_intersect), "ray"_a)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def_readwrite("center", &BSphere::center)
            .def_readwrite("radius", &BSphere::radius)
            .def_repr(BSphere);
        }
}

MI_PY_EXPORT(BoundingSphere) {
    MI_PY_IMPORT_TYPES()

    bind_bsphere<BoundingSphere3f, Ray3f>(m, "BoundingSphere3f");

    if constexpr (!std::is_same_v<Float, ScalarFloat>)
        bind_bsphere<ScalarBoundingSphere3f, Ray3f>(m, "ScalarBoundingSphere3f");
}
