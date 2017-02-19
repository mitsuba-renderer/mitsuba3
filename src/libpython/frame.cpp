#include <mitsuba/core/frame.h>
#include "python.h"

MTS_PY_EXPORT(Frame) {
    using Frame = Frame3f;
    py::class_<Frame>(m, "Frame3f", D(Frame))
        .def(py::init<>(), D(Frame, Frame))
        .def(py::init<const Frame &>(), "Copy constructor")
        // Omitted because the Vector/Normal distinction doesn't apply in Python
        // .def(py::init<Vector3f, Vector3f, Normal3f>(), D(Frame, Frame, _2))
        .def(py::init<Vector3f, Vector3f, Vector3f>(), D(Frame, Frame, 3))
        .def(py::init<Vector3f>(), D(Frame, Frame, 4))
        .def("to_local", &Frame::to_local, D(Frame, to_local))
        .def("to_world", &Frame::to_world, D(Frame, to_world))
        .def_static("cos_theta", &Frame::cos_theta, D(Frame, cos_theta))
        .def_static("cos_theta_2", &Frame::cos_theta_2, D(Frame, cos_theta_2))
        .def_static("sin_theta", &Frame::sin_theta, D(Frame, sin_theta))
        .def_static("sin_theta_2", &Frame::sin_theta_2, D(Frame, sin_theta_2))
        .def_static("tan_theta", &Frame::tan_theta, D(Frame, tan_theta))
        .def_static("tan_theta_2", &Frame::tan_theta_2, D(Frame, tan_theta_2))
        .def_static("sin_phi", &Frame::sin_phi, D(Frame, sin_phi))
        .def_static("sin_phi_2", &Frame::sin_phi_2, D(Frame, sin_phi_2))
        .def_static("cos_phi", &Frame::cos_phi, D(Frame, cos_phi))
        .def_static("cos_phi_2", &Frame::cos_phi_2, D(Frame, cos_phi_2))
        .def_static("uv", &Frame::uv, D(Frame, uv))
        .def(py::self == py::self, D(Frame, operator_eq))
        .def(py::self != py::self, D(Frame, operator_ne))
        .def("__repr__", [](const Frame &f) {
                std::ostringstream oss;
                oss << f;
                return oss.str();
        })
        .def_readwrite("s", &Frame::s)
        .def_readwrite("t", &Frame::t)
        .def_readwrite("n", &Frame::n);
}
