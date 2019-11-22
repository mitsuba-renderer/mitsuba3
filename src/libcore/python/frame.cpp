#include <mitsuba/core/frame.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT_STRUCT(Frame) {
    MTS_IMPORT_CORE_TYPES()
    MTS_PY_CHECK_ALIAS(Frame3f, m) {
        auto f = py::class_<Frame3f>(m, "Frame3f", D(Frame3f))
            .def(py::init<>(), D(Frame3f, Frame3f))
            .def(py::init<const Frame3f &>(), "Copy constructor")
            .def(py::init<Vector3f, Vector3f, Vector3f>(), D(Frame3f, Frame3f, 3))
            .def(py::init<Vector3f>(), D(Frame3f, Frame3f, 4))
            .def(py::self == py::self, D(Frame3f, operator_eq))
            .def(py::self != py::self, D(Frame3f, operator_ne))
            .def_method(Frame3f, to_local, "v"_a)
            .def_method(Frame3f, to_world, "v"_a)
            .def_static_method(Frame3f, cos_theta, "v"_a)
            .def_static_method(Frame3f, cos_theta_2, "v"_a)
            .def_static_method(Frame3f, sin_theta, "v"_a)
            .def_static_method(Frame3f, sin_theta_2, "v"_a)
            .def_static_method(Frame3f, tan_theta, "v"_a)
            .def_static_method(Frame3f, tan_theta_2, "v"_a)
            .def_static_method(Frame3f, sin_phi, "v"_a)
            .def_static_method(Frame3f, sin_phi_2, "v"_a)
            .def_static_method(Frame3f, cos_phi, "v"_a)
            .def_static_method(Frame3f, cos_phi_2, "v"_a)
            .def_static_method(Frame3f, sincos_phi, "v"_a)
            .def_static_method(Frame3f, sincos_phi_2, "v"_a)
            .def_field(Frame3f, s)
            .def_field(Frame3f, t)
            .def_field(Frame3f, n)
            .def_repr(Frame3f);
        bind_slicing_operators<Frame3f, ScalarFrame3f>(f);
    }
}
