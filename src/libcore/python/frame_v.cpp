#include <mitsuba/core/frame.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Frame) {
    MTS_PY_IMPORT_TYPES_DYNAMIC()
    MTS_PY_CHECK_ALIAS(Frame3f, "Frame3f") {
        auto f = py::class_<Frame3f>(m, "Frame3f", D(Frame))
            .def(py::init<>(), D(Frame, Frame))
            .def(py::init<const Frame3f &>(), "Copy constructor")
            .def(py::init<Vector3f, Vector3f, Vector3f>(), D(Frame, Frame, 3))
            .def(py::init<Vector3f>(), D(Frame, Frame, 4))
            .def(py::self == py::self, D(Frame, operator_eq))
            .def(py::self != py::self, D(Frame, operator_ne))
            .def("to_local", &Frame3f::to_local, "v"_a, D(Frame, to_local))
            .def("to_world", &Frame3f::to_world, "v"_a, D(Frame, to_world))
            .def_static("cos_theta",    &Frame3f::cos_theta,    "v"_a, D(Frame, cos_theta))
            .def_static("cos_theta_2",  &Frame3f::cos_theta_2,  "v"_a, D(Frame, cos_theta_2))
            .def_static("sin_theta",    &Frame3f::sin_theta,    "v"_a, D(Frame, sin_theta))
            .def_static("sin_theta_2",  &Frame3f::sin_theta_2,  "v"_a, D(Frame, sin_theta_2))
            .def_static("tan_theta",    &Frame3f::tan_theta,    "v"_a, D(Frame, tan_theta))
            .def_static("tan_theta_2",  &Frame3f::tan_theta_2,  "v"_a, D(Frame, tan_theta_2))
            .def_static("sin_phi",      &Frame3f::sin_phi,      "v"_a, D(Frame, sin_phi))
            .def_static("sin_phi_2",    &Frame3f::sin_phi_2,    "v"_a, D(Frame, sin_phi_2))
            .def_static("cos_phi",      &Frame3f::cos_phi,      "v"_a, D(Frame, cos_phi))
            .def_static("cos_phi_2",    &Frame3f::cos_phi_2,    "v"_a, D(Frame, cos_phi_2))
            .def_static("sincos_phi",   &Frame3f::sincos_phi,   "v"_a, D(Frame, sincos_phi))
            .def_static("sincos_phi_2", &Frame3f::sincos_phi_2, "v"_a, D(Frame, sincos_phi_2))

            .def_field(Frame3f, s)
            .def_field(Frame3f, t)
            .def_field(Frame3f, n)
            .def_repr(Frame3f);
        bind_slicing_operators<Frame3f, ScalarFrame3f>(f);
    }
}