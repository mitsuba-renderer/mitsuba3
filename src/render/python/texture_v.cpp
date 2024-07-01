#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/pair.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyTexture : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)
    NB_TRAMPOLINE(Texture, 14);

    PyTexture(const Properties &props) : Texture(props) {}

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask active = true) const override {
        NB_OVERRIDE_PURE(eval, si, active);
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &si, const Wavelength &sample,
                    Mask active = true) const override {
        using Return = std::pair<Wavelength, UnpolarizedSpectrum>;
        NB_OVERRIDE_PURE(sample_spectrum, si, sample, active);
    }

    Wavelength pdf_spectrum(const SurfaceInteraction3f &si,
                            Mask active = true) const override {
        NB_OVERRIDE_PURE(pdf_spectrum, si, active);
    }

    std::pair<Point2f, Float>
    sample_position(const Point2f &sample, Mask active = true) const override {
        using Return = std::pair<Point2f, Float>;
        NB_OVERRIDE(sample_position, sample, active);
    }

    Float pdf_position(const Point2f &p, Mask active = true) const override {
        NB_OVERRIDE(pdf_position, p, active);
    }

    Float eval_1(const SurfaceInteraction3f &si,
                 Mask active = true) const override {
        NB_OVERRIDE_PURE(eval_1, si, active);
    }

    Vector2f eval_1_grad(const SurfaceInteraction3f &si,
                         Mask active = true) const override {
        NB_OVERRIDE_PURE(eval_1_grad, si, active);
    }

    Color3f eval_3(const SurfaceInteraction3f &si,
                   Mask active = true) const override {
        NB_OVERRIDE_PURE(eval_3, si, active);
    }

    Float mean() const override {
        NB_OVERRIDE_PURE(mean);
    }

    ScalarVector2i resolution() const override {
        NB_OVERRIDE(resolution);
    }

    ScalarFloat spectral_resolution() const override {
        NB_OVERRIDE_PURE(spectral_resolution);
    }

    ScalarVector2f wavelength_range() const override {
        NB_OVERRIDE(wavelength_range);
    }

    bool is_spatially_varying() const override {
        NB_OVERRIDE(is_spatially_varying);
    }

    std::string to_string() const override {
        NB_OVERRIDE(to_string);
    }
};

MI_PY_EXPORT(Texture) {
    MI_PY_IMPORT_TYPES(Texture)
    using PyTexture = PyTexture<Float, Spectrum>;
    using Properties = PropertiesV<Float>;

    MI_PY_TRAMPOLINE_CLASS(PyTexture, Texture, Object)
        .def(nb::init<const Properties &>(), "props"_a)
        .def_static("D65", nb::overload_cast<ScalarFloat>(&Texture::D65), "scale"_a = 1.f)
        .def_method(Texture, mean, D(Texture, mean))
        .def_method(Texture, max, D(Texture, max))
        .def_method(Texture, is_spatially_varying)
        .def_method(Texture, eval, "si"_a, "active"_a = true)
        .def_method(Texture, eval_1, "si"_a, "active"_a = true)
        .def_method(Texture, eval_1_grad, "si"_a, "active"_a = true)
        .def_method(Texture, eval_3, "si"_a, "active"_a = true)
        .def_method(Texture, sample_spectrum, "si"_a, "sample"_a,
                    "active"_a = true)
        .def_method(Texture, resolution)
        .def_method(Texture, pdf_spectrum, "si"_a, "active"_a = true)
        .def_method(Texture, sample_position, "sample"_a, "active"_a = true)
        .def_method(Texture, pdf_position, "p"_a, "active"_a = true)
        .def_method(Texture, spectral_resolution)
        .def_method(Texture, wavelength_range);

    MI_PY_REGISTER_OBJECT("register_texture", Texture)
}
