#include <mitsuba/core/properties.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Volume) {
    MTS_PY_CLASS(Volume, Object)
        .def(py::init<const Properties &>())
        .def("eval",
            vectorize(py::overload_cast<const Interaction3f&, Mask>(
                &Volume::eval, py::const_)),
            "it"_a, "active"_a = true, D(Volume, eval))
        .def("eval_1",
            vectorize(py::overload_cast<const Interaction3f&, Mask>(
                &Volume::eval_1, py::const_)),
            "it"_a, "active"_a = true, D(Volume, eval_1))
        .def("eval_3",
            vectorize(py::overload_cast<const Interaction3f&, Mask>(
                &Volume::eval_3, py::const_)),
            "it"_a, "active"_a = true, D(Volume, eval_3))
        .def("eval_gradient",
             vectorize(py::overload_cast<const Interaction3f&, Mask>(&Volume::eval_gradient,
                                                            py::const_),
             D(Volume, eval_gradient), "it"_a, "active"_a = true)
        .def("eval_gradient",
             vectorize_wrapper(py::overload_cast<const Interaction3fP &, MaskP>(
                 &Volume::eval_gradient, py::const_)),
             D(Volume, eval_gradient), "it"_a, "active"_a = true)
        .def_method(Volume, max)
        .def_method(Volume, bbox)
        .def_method(Volume, resolution)
        .def("__repr__", &Volume::to_string);
}
