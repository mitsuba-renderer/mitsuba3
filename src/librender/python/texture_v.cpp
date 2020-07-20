#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Texture) {
    MTS_PY_IMPORT_TYPES(Texture)
    MTS_PY_CLASS(Texture, Object)
        .def_static("D65", &Texture::D65, "scale"_a = 1.f)
        .def("mean", &Texture::mean, D(Texture, mean))
        .def("is_spatially_varying", &Texture::is_spatially_varying,
             D(Texture, is_spatially_varying))
        .def("eval",
            vectorize(&Texture::eval),
            "si"_a, "active"_a = true, D(Texture, eval))
        .def("eval_1",
            vectorize(&Texture::eval_1),
            "si"_a, "active"_a = true, D(Texture, eval_1))
        .def("eval_1_grad",
            vectorize(&Texture::eval_1_grad),
            "si"_a, "active"_a = true, D(Texture, eval_1_grad))
        .def("eval_3",
            vectorize(&Texture::eval_3),
            "si"_a, "active"_a = true, D(Texture, eval_3))
        .def("sample_spectrum",
            vectorize(&Texture::sample_spectrum),
            "si"_a, "sample"_a, "active"_a = true, D(Texture, sample_spectrum))
        .def("pdf_spectrum", &Texture::pdf_spectrum,
            "si"_a, "active"_a = true, D(Texture, pdf_spectrum))
        .def("sample_position",
            vectorize(&Texture::sample_position),
            "sample"_a, "active"_a = true, D(Texture, sample_position))
        .def("pdf_position",
            vectorize(&Texture::pdf_position),
            "p"_a, "active"_a = true, D(Texture, pdf_position));
}
