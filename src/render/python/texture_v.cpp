#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(Texture) {
    MI_PY_IMPORT_TYPES(Texture)
    MI_PY_CLASS(Texture, Object)
        .def_static("D65", &Texture::D65, "scale"_a = 1.f)
        .def("mean", &Texture::mean, D(Texture, mean))
        .def("is_spatially_varying", &Texture::is_spatially_varying,
             D(Texture, is_spatially_varying))
        .def("eval",
            &Texture::eval,
            "si"_a, "active"_a = true, D(Texture, eval))
        .def("eval_1",
            &Texture::eval_1,
            "si"_a, "active"_a = true, D(Texture, eval_1))
        .def("eval_1_grad",
            &Texture::eval_1_grad,
            "si"_a, "active"_a = true, D(Texture, eval_1_grad))
        .def("eval_3",
            &Texture::eval_3,
            "si"_a, "active"_a = true, D(Texture, eval_3))
        .def("sample_spectrum",
            &Texture::sample_spectrum,
            "si"_a, "sample"_a, "active"_a = true, D(Texture, sample_spectrum))
        .def("pdf_spectrum", &Texture::pdf_spectrum,
            "si"_a, "active"_a = true, D(Texture, pdf_spectrum))
        .def("sample_position",
            &Texture::sample_position,
            "sample"_a, "active"_a = true, D(Texture, sample_position))
        .def("pdf_position",
            &Texture::pdf_position,
            "p"_a, "active"_a = true, D(Texture, pdf_position))
        .def("spectral_resolution",
            &Texture::spectral_resolution, D(Texture, spectral_resolution))
        .def("wavelength_range",
            &Texture::wavelength_range, D(Texture, wavelength_range));
}
