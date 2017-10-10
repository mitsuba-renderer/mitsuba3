#include <mitsuba/core/ray.h>
#include <mitsuba/python/python.h>

template <typename Type, typename... Args, typename... Args2>
auto bind_ray(Args2&&... args2) {
    return py::class_<Type, Args...>(args2...)
        .def(py::init<>(), "Create an unitialized ray")
        .def(py::init<const Type &>(), "Copy constructor")
        .def(py::init<Point3f, Vector3f>(), D(Ray, Ray, 5))
        .def(py::init<Point3f, Vector3f, Float, Float>(), D(Ray, Ray, 6))
        .def(py::init<const Type &, Float, Float>(), D(Ray, Ray, 7))
        .def("update", &Type::update, D(Ray, update))
        .def("__call__", &Type::operator(), D(Ray, operator, call))
        .def("__repr__", [](const Type &f) {
            std::ostringstream oss;
            oss << f;
            return oss.str();
        })
        .def_readwrite("o", &Type::o, D(Ray, o))
        .def_readwrite("d", &Type::d, D(Ray, d))
        .def_readwrite("d_rcp", &Type::d_rcp, D(Ray, d_rcp))
        .def_readwrite("mint", &Type::mint, D(Ray, mint))
        .def_readwrite("maxt", &Type::maxt, D(Ray, maxt))
        .def_readwrite("time", &Type::time, D(Ray, time));
}

template <typename Type, typename... Args, typename... Args2>
auto bind_ray_differential(Args2&&... args2) {
    return bind_ray<Type, Args...>(args2...)
        .def_readwrite("o_x", &Type::o_x, D(RayDifferential, o_x))
        .def_readwrite("o_y", &Type::o_y, D(RayDifferential, o_y))
        .def_readwrite("d_x", &Type::d_x, D(RayDifferential, d_x))
        .def_readwrite("d_y", &Type::d_y, D(RayDifferential, d_y))
        .def_readwrite("has_differentials", &Type::has_differentials,
                       D(RayDifferential, has_differentials));
}

MTS_PY_EXPORT(Ray) {
    bind_ray<Ray3f>(m, "Ray3f", D(Ray));
    auto r3fx = bind_ray<Ray3fX>(m, "Ray3fX", D(Ray));
    bind_slicing_operators<Ray3fX, Ray3f>(r3fx);

    bind_ray_differential<RayDifferential3f, Ray3f>(m, "RayDifferential3f", D(RayDifferential));
    auto rd3fx = bind_ray_differential<RayDifferential3fX>(m, "RayDifferential3fX", D(RayDifferential));
    bind_slicing_operators<RayDifferential3fX, RayDifferential3f>(rd3fx);
}
