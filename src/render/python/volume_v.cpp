#include <mitsuba/render/volume.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(Volume) {
    MI_PY_IMPORT_TYPES(Volume)
    MI_PY_CLASS(Volume, Object)
        .def_method(Volume, resolution)
        .def_method(Volume, bbox)
        .def_method(Volume, max)
        .def("eval", &Volume::eval, "it"_a, "active"_a = true, D(Volume, eval))
        .def("eval_1", &Volume::eval_1, "it"_a, "active"_a = true,
             D(Volume, eval_1))
        .def("eval_3", &Volume::eval_3, "it"_a, "active"_a = true,
             D(Volume, eval_3))
        .def("eval_6",
                [](const Volume &volume, const Interaction3f &it, const Mask active) {
                    dr::Array<Float, 6> result = volume.eval_6(it, active);
                    std::array<Float, 6> output;
                    for (size_t i = 0; i < 6; ++i)
                        output[i] = std::move(result[i]);
                    return output;
                }, "it"_a, "active"_a = true, D(Volume, eval_6))
        .def("eval_gradient", &Volume::eval_gradient, "it"_a, "active"_a = true,
             D(Volume, eval_gradient));
}
