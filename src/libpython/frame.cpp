#include <mitsuba/core/frame.h>
#include <mitsuba/core/vector.h>
#include "python.h"

MTS_PY_EXPORT(Frame) {
    py::class_<Frame>(m, "Frame", DM(Frame))
        .def(py::init<>(), DM(Frame, Frame))
        // Omitted because the Vector/Normal distinction doesn't apply in Python
        // .def(py::init<Vector3f, Vector3f, Normal3f>(), DM(Frame, Frame, 2))
        .def(py::init<Vector3f, Vector3f, Vector3f>(), DM(Frame, Frame, 3))
        .def(py::init<Vector3f>(), DM(Frame, Frame, 4))
        .mdef(Frame, toLocal)
        .mdef(Frame, toWorld)
        .def_static("cosTheta", &Frame::cosTheta, DM(Frame, cosTheta))
        .def_static("cosTheta2", &Frame::cosTheta2, DM(Frame, cosTheta2))
        .def_static("sinTheta", &Frame::sinTheta, DM(Frame, sinTheta))
        .def_static("sinTheta2", &Frame::sinTheta2, DM(Frame, sinTheta2))
        .def_static("tanTheta", &Frame::tanTheta, DM(Frame, tanTheta))
        .def_static("tanTheta2", &Frame::tanTheta2, DM(Frame, tanTheta2))
        .def_static("sinPhi", &Frame::sinPhi, DM(Frame, sinPhi))
        .def_static("sinPhi2", &Frame::sinPhi2, DM(Frame, sinPhi2))
        .def_static("cosPhi", &Frame::cosPhi, DM(Frame, cosPhi))
        .def_static("cosPhi2", &Frame::cosPhi2, DM(Frame, cosPhi2))
        .def_static("uv", &Frame::uv, DM(Frame, uv))
        .def(py::self == py::self, DM(Frame, operator_eq))
        .def(py::self != py::self, DM(Frame, operator_ne))
        .def("__repr__", &Frame::toString)
        .def_readwrite("s", &Frame::s)
        .def_readwrite("t", &Frame::t)
        .def_readwrite("n", &Frame::n);
}
