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
        .def("__repr__", [](const Type &f) {
                std::ostringstream oss;
                oss << f;
                return oss.str();
        })
        .def_readwrite("s", &Type::s)
        .def_readwrite("t", &Type::t)
        .def_readwrite("n", &Type::n)
        ;
}

MTS_PY_EXPORT(Frame) {
    bind_frame<Frame3f>(m, "Frame3f")
        .def("to_local", &Frame3f::to_local, "v"_a, D(Frame, to_local))
        .def("to_world", &Frame3f::to_world, "v"_a, D(Frame, to_world))
        .def_static("cos_theta", &Frame3f::cos_theta, "v"_a, D(Frame, cos_theta))
        .def_static("cos_theta_2", &Frame3f::cos_theta_2, "v"_a, D(Frame, cos_theta_2))
        .def_static("sin_theta", &Frame3f::sin_theta, "v"_a, D(Frame, sin_theta))
        .def_static("sin_theta_2", &Frame3f::sin_theta_2, "v"_a, D(Frame, sin_theta_2))
        .def_static("tan_theta", &Frame3f::tan_theta, "v"_a, D(Frame, tan_theta))
        .def_static("tan_theta_2", &Frame3f::tan_theta_2, "v"_a, D(Frame, tan_theta_2))
        .def_static("sin_phi", &Frame3f::sin_phi, "v"_a, D(Frame, sin_phi))
        .def_static("sin_phi_2", &Frame3f::sin_phi_2, "v"_a, D(Frame, sin_phi_2))
        .def_static("cos_phi", &Frame3f::cos_phi, "v"_a, D(Frame, cos_phi))
        .def_static("cos_phi_2", &Frame3f::cos_phi_2, "v"_a, D(Frame, cos_phi_2))
        .def_static("sincos_phi", &Frame3f::sincos_phi, "v"_a, D(Frame, sincos_phi))
        .def_static("sincos_phi_2", &Frame3f::sincos_phi_2, "v"_a, D(Frame, sincos_phi_2))
        ;


    auto f3fx = bind_frame<Frame3fX>(m, "Frame3fX")
        .def("to_local", enoki::vectorize_wrapper(&Frame3fP::to_local),
             "v"_a, D(Frame, to_local))
        .def("to_world", enoki::vectorize_wrapper(&Frame3fP::to_world),
             "v"_a, D(Frame, to_world))
        ;

    bind_slicing_operators<Frame3fX, Frame3f>(f3fx);
}
