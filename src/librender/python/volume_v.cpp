#include <mitsuba/render/texture.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Volume) {
    MTS_PY_IMPORT_TYPES(Volume)
    MTS_PY_CLASS(Volume, Object)
        .def_method(Volume, resolution)
        .def_method(Volume, bbox)
        .def_method(Volume, max)
        .def("eval", &Volume::eval, "it"_a, "active"_a = true, D(Volume, eval))
        .def("eval_1", &Volume::eval_1, "it"_a, "active"_a = true,
             D(Volume, eval_1))
        .def("eval_3", &Volume::eval_3, "it"_a, "active"_a = true,
             D(Volume, eval_3))
        .def("eval_6", &Volume::eval_6, "it"_a, "active"_a = true,
             D(Volume, eval_6))
        .def("eval_gradient", &Volume::eval_gradient, "it"_a, "active"_a = true,
             D(Volume, eval_gradient));
}
