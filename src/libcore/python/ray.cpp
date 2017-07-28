#include <mitsuba/core/ray.h>
#include <mitsuba/python/python.h>

template <typename Type, typename... Args, typename... Args2> auto bind_ray(py::module &m, const char *name, Args2&&... args2) {
    return py::class_<Type, Args...>(m, name, D(Ray), args2...)
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
    return bind_ray<Type, Args...>(m, name, D(RayDifferential))
        .def_readwrite("o_x", &Type::o_x, D(RayDifferential, o_x))
        .def_readwrite("o_y", &Type::o_y, D(RayDifferential, o_y))
        .def_readwrite("d_x", &Type::d_x, D(RayDifferential, d_x))
        .def_readwrite("d_y", &Type::d_y, D(RayDifferential, d_y))
        .def_readwrite("has_differentials", &Type::has_differentials,
                       D(RayDifferential, has_differentials));
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

    auto r3fx = bind_ray<Ray3fX>(m, "Ray3fX");
    bind_slicing_operators<Ray3fX, Ray3f>(r3fx);

    auto rd3fx = bind_ray_differential<RayDifferential3fX>(m, "RayDifferential3fX");
    bind_slicing_operators<Ray3fX, Ray3f>(rd3fx);
}
