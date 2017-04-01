#include <mitsuba/core/ray.h>
#include <mitsuba/python/python.h>

template <typename Type, typename... Args> auto bind_ray(py::module &m, const char *name) {
    return py::class_<Type, Args...>(m, name, D(Ray))
        .def_readwrite("o", &Type::o, D(Ray, o))
        .def_readwrite("d", &Type::d, D(Ray, d))
        .def_readwrite("d_rcp", &Type::d_rcp, D(Ray, d_rcp))
        .def_readwrite("mint", &Type::mint, D(Ray, mint))
        .def_readwrite("maxt", &Type::maxt, D(Ray, maxt))
        .def("__repr__", [](const Type &f) {
            std::ostringstream oss;
            oss << f;
            return oss.str();
        });
}

template <typename Type, typename... Args> auto bind_ray_differential(py::module &m, const char *name) {
    auto r = bind_ray<Type, Args...>(m, name)
        .def_readwrite("o_x", &Type::o_x, D(RayDifferential, o_x))
        .def_readwrite("o_y", &Type::o_y, D(RayDifferential, o_y))
        .def_readwrite("d_x", &Type::d_x, D(RayDifferential, d_x))
        .def_readwrite("d_y", &Type::d_y, D(RayDifferential, d_y));
    r.attr("__doc__") = D(RayDifferential);
    return r;
}

MTS_PY_EXPORT(Ray) {
    bind_ray<Ray3f>(m, "Ray3f")
        .def(py::init<>())
        .def(py::init<const Ray3f &>())
        .def(py::init<Point3f, Vector3f>(), D(Ray, Ray, 5))
        .def(py::init<Point3f, Vector3f, Float, Float>(), D(Ray, Ray, 6))
        .def(py::init<const Ray3f &, Float, Float>(), D(Ray, Ray, 7))
        .def("update", &Ray3f::update, D(Ray, update))
        .def("__call__", &Ray3f::operator(), D(Ray, operator, call));

    bind_ray<RayDifferential3f, Ray3f>(m, "RayDifferential3f")
        .def(py::init<>(), D(RayDifferential, RayDifferential))
        .def(py::init<const RayDifferential3f &>())
        .def(py::init<Point3f, Vector3f>(), D(Ray, Ray, 5))
        .def(py::init<Point3f, Vector3f, Float, Float>(), D(Ray, Ray, 6))
        .def(py::init<const RayDifferential3f &, Float, Float>(), D(Ray, Ray, 7))
        .def("__call__", &RayDifferential3f::operator(), D(Ray, operator, call));

    bind_ray<Ray3fX>(m, "Ray3fX")
        .def(py::init<>(), D(Ray, Ray))
        .def("__init__", [](Ray3fX &r, size_t n) {
            new (&r) Ray3fX();
            set_slices(r, n);
        })
        .def("__getitem__", [](Ray3fX &r, size_t i) {
            if (i >= slices(r))
                throw py::index_error();
            return Ray3f(enoki::slice(r, i));
        })
        .def("__setitem__", [](Ray3fX &r, size_t i, const Ray3f &r2) {
            if (i >= slices(r))
                throw py::index_error();
            enoki::slice(r, i) = r2;
        });

    bind_ray_differential<RayDifferential3fX>(m, "RayDifferential3fX")
        .def(py::init<>(), D(RayDifferential, RayDifferential))
        .def("__init__", [](RayDifferential3fX &r, size_t n) {
            new (&r) RayDifferential3fX();
            set_slices(r, n);
        })
        .def("__getitem__", [](RayDifferential3fX &r, size_t i) {
            if (i >= slices(r))
                throw py::index_error();
            return RayDifferential3f(enoki::slice(r, i));
        })
        .def("__setitem__", [](RayDifferential3fX &r, size_t i, const RayDifferential3f &r2) {
            if (i >= slices(r))
                throw py::index_error();
            enoki::slice(r, i) = r2;
        });
}

