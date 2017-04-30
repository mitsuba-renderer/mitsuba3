#include <mitsuba/core/ray.h>
#include <mitsuba/python/python.h>

template <typename Type> auto bind_ray(py::module &m, const char *name) {
    return py::class_<Type>(m, name, D(Ray))
        .def(py::init<>(), D(Ray, Ray))
        .def(py::init<const Type &>(), D(Ray, Ray, 2))
        .def_readwrite("o", &Type::o, D(Ray, o))
        .def_readwrite("d", &Type::d, D(Ray, d))
        .def_readwrite("d_rcp", &Type::d_rcp, D(Ray, d_rcp))
        .def_readwrite("mint", &Type::mint, D(Ray, mint))
        .def_readwrite("maxt", &Type::maxt, D(Ray, maxt));
}

MTS_PY_EXPORT(Ray) {
    bind_ray<Ray3f>(m, "Ray3f")
        .def(py::init<Point3f, Vector3f>(), D(Ray, Ray, 4))
        .def(py::init<Point3f, Vector3f, Float, Float>(), D(Ray, Ray, 5))
        .def(py::init<const Ray3f &, Float, Float>(), D(Ray, Ray, 7))
        .def("update", &Ray3f::update, D(Ray, update))
        .def("reverse", &Ray3f::reverse, D(Ray, reverse))
        .def("__call__", &Ray3f::operator(), D(Ray, operator, call));

    bind_ray<Ray3fX>(m, "Ray3fX");
}

