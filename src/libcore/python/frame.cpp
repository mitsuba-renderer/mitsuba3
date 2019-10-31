#include <mitsuba/core/frame.h>
#include <mitsuba/python/python.h>

template <typename Type>
auto bind_frame(py::module &m, const char *name) {
    using Vector3 = typename Type::Vector3;

    return py::class_<Type>(m, name, D(Frame))
        .def(py::init<>(), D(Frame, Frame))
        .def(py::init<const Type &>(), "Copy constructor")
        .def(py::init<Vector3, Vector3, Vector3>(), D(Frame, Frame, 3))
        .def(py::init<Vector3>(), D(Frame, Frame, 4))
        .def(py::self == py::self, D(Frame, operator_eq))
        .def(py::self != py::self, D(Frame, operator_ne))
        .def_repr(Type)
        .def_readwrite("s", &Type::s)
        .def_readwrite("t", &Type::t)
        .def_readwrite("n", &Type::n);
}

MTS_PY_EXPORT(Frame) {
    bind_frame<Frame3f>(m, "Frame3f")
        .def_method(Frame, to_local, "v"_a)
        .def_method(Frame, to_world, "v"_a)
        .def_static_method(Frame, cos_theta, "v"_a)
        .def_static_method(Frame, cos_theta_2, "v"_a)
        .def_static_method(Frame, sin_theta, "v"_a)
        .def_static_method(Frame, sin_theta_2, "v"_a)
        .def_static_method(Frame, tan_theta, "v"_a)
        .def_static_method(Frame, tan_theta_2, "v"_a)
        .def_static_method(Frame, sin_phi, "v"_a)
        .def_static_method(Frame, sin_phi_2, "v"_a)
        .def_static_method(Frame, cos_phi, "v"_a)
        .def_static_method(Frame, cos_phi_2, "v"_a)
        .def_static_method(Frame, sincos_phi, "v"_a)
        .def_static_method(Frame, sincos_phi_2, "v"_a);


    auto f3fx = bind_frame<Frame3fX>(m, "Frame3fX")
        .def("to_local", enoki::vectorize_wrapper(&Frame3fP::to_local),
             "v"_a, D(Frame, to_local))
        .def("to_world", enoki::vectorize_wrapper(&Frame3fP::to_world),
             "v"_a, D(Frame, to_world));

    bind_slicing_operators<Frame3fX, Frame3f>(f3fx);

#if defined(MTS_ENABLE_AUTODIFF)
    auto f3fd = bind_frame<Frame3fD>(m, "Frame3fD")
        .def_method(Frame, to_local, "v"_a)
        .def_method(Frame, to_world, "v"_a)
        .def_static_method(Frame, cos_theta, "v"_a)
        .def_static_method(Frame, cos_theta_2, "v"_a)
        .def_static_method(Frame, sin_theta, "v"_a)
        .def_static_method(Frame, sin_theta_2, "v"_a)
        .def_static_method(Frame, tan_theta, "v"_a)
        .def_static_method(Frame, tan_theta_2, "v"_a)
        .def_static_method(Frame, sin_phi, "v"_a)
        .def_static_method(Frame, sin_phi_2, "v"_a)
        .def_static_method(Frame, cos_phi, "v"_a)
        .def_static_method(Frame, cos_phi_2, "v"_a)
        .def_static_method(Frame, sincos_phi, "v"_a)
        .def_static_method(Frame, sincos_phi_2, "v"_a);

    bind_slicing_operators<Frame3fD, Frame3f>(f3fd);
#endif
}
