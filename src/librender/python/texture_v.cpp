#include <mitsuba/python/python.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>

MTS_PY_EXPORT(Texture) {
    MTS_PY_IMPORT_TYPES(Texture)
    MTS_PY_CLASS(Texture, Object)
        .def_static("D65", &Texture::D65, "scale"_a = 1.f)
        .def("mean", &Texture::mean, D(Texture, mean))
        .def("is_spatially_varying", &Texture::is_spatially_varying,
             D(Texture, is_spatially_varying))
        .def("eval",
            vectorize(py::overload_cast<const SurfaceInteraction3f&, Mask>(
                &Texture::eval, py::const_)),
            "si"_a, "active"_a = true, D(Texture, eval))
        .def("eval_1",
            vectorize(py::overload_cast<const SurfaceInteraction3f&, Mask>(
                &Texture::eval_1, py::const_)),
            "si"_a, "active"_a = true, D(Texture, eval_1))
        .def("eval_3",
            vectorize(py::overload_cast<const SurfaceInteraction3f&, Mask>(
                &Texture::eval_3, py::const_)),
            "si"_a, "active"_a = true, D(Texture, eval_3))
        .def("sample",
            vectorize(
                py::overload_cast<const SurfaceInteraction3f&, const Wavelength &, Mask>(
                    &Texture::sample, py::const_)),
            "si"_a, "sample"_a, "active"_a = true, D(Texture, sample))
        .def("pdf",
            vectorize(py::overload_cast<const SurfaceInteraction3f&, Mask>(
                &Texture::pdf, py::const_)),
            "si"_a, "active"_a = true, D(Texture, pdf));
}
