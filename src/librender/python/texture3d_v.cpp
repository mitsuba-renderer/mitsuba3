#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/spectrum.h>

MTS_PY_EXPORT(Texture3D) {
    MTS_PY_CLASS(Texture3D, Object)
        .def(py::init<const Properties &>())
        .def("eval",
            vectorize(py::overload_cast<const Interaction3f&, Mask>(
                &Texture3D::eval, py::const_)),
            "it"_a, "active"_a = true, D(Texture3D, eval))
        .def("eval_1",
            vectorize(py::overload_cast<const Interaction3f&, Mask>(
                &Texture3D::eval_1, py::const_)),
            "it"_a, "active"_a = true, D(Texture3D, eval_1))
        .def("eval_3",
            vectorize(py::overload_cast<const Interaction3f&, Mask>(
                &Texture3D::eval_3, py::const_)),
            "it"_a, "active"_a = true, D(Texture3D, eval_3))
        .def("eval_gradient",
             vectorize(py::overload_cast<const Interaction3f&, Mask>(&Texture3D::eval_gradient,
                                                            py::const_),
             D(Texture3D, eval_gradient), "it"_a, "active"_a = true)
        .def("eval_gradient",
             vectorize_wrapper(py::overload_cast<const Interaction3fP &, MaskP>(
                 &Texture3D::eval_gradient, py::const_)),
             D(Texture3D, eval_gradient), "it"_a, "active"_a = true)
        .def_method(Texture3D, max)
        .def_method(Texture3D, bbox)
        .def_method(Texture3D, resolution)
        .def("__repr__", &Texture3D::to_string);
}
