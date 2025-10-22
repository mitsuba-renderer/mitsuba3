#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/vector.h>
#include <drjit/python.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyTexture : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)
    NB_TRAMPOLINE(Texture, 17);

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

    ScalarFloat max() const override {
        NB_OVERRIDE_PURE(max);
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

    void traverse(TraversalCallback *cb) override {
        NB_OVERRIDE(traverse, cb);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        NB_OVERRIDE(parameters_changed, keys);
    }

    DR_TRAMPOLINE_TRAVERSE_CB(Texture)
};

template <typename Ptr, typename Cls> void bind_texture_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES()

    cls.def("eval",
            [](Ptr texture, const SurfaceInteraction3f &si,
               Mask active) { return texture->eval(si, active); },
            "si"_a, "active"_a = true, D(Texture, eval))
        .def("eval_1",
             [](Ptr texture, const SurfaceInteraction3f &si,
                Mask active) { return texture->eval_1(si, active); },
             "si"_a, "active"_a = true, D(Texture, eval_1))
        .def("eval_3",
             [](Ptr texture, const SurfaceInteraction3f &si,
                Mask active) { return texture->eval_3(si, active); },
             "si"_a, "active"_a = true, D(Texture, eval_3))
        .def("sample_spectrum",
             [](Ptr texture, const SurfaceInteraction3f &si,
                const Wavelength &sample,
                Mask active) {
                 return texture->sample_spectrum(si, sample, active);
             }, "si"_a, "sample"_a, "active"_a = true,
             D(Texture, sample_spectrum))
        .def("pdf_spectrum",
             [](Ptr texture, const SurfaceInteraction3f &si,
                Mask active) {
                 return texture->pdf_spectrum(si, active);
             }, "si"_a, "active"_a = true,
             D(Texture, pdf_spectrum))
        .def("sample_position",
             [](Ptr texture, const Point2f &sample,
                Mask active) {
                 return texture->sample_position(sample, active);
             }, "sample"_a, "active"_a = true,
             D(Texture, sample_position))
        .def("pdf_position",
             [](Ptr texture, const Point2f &p,
                Mask active) {
                 return texture->pdf_position(p, active);
             }, "p"_a, "active"_a = true,
             D(Texture, pdf_position))
        .def("eval_1_grad",
             [](Ptr texture, const SurfaceInteraction3f &si,
                Mask active) { return texture->eval_1_grad(si, active); },
             "si"_a, "active"_a = true, D(Texture, eval_1_grad))
        .def("mean",
             [](Ptr texture) { return texture->mean(); },
             D(Texture, mean))
        .def("max",
             [](Ptr texture) { return texture->max(); },
             D(Texture, max))
        .def("is_spatially_varying",
             [](Ptr texture) { return texture->is_spatially_varying(); },
             D(Texture, is_spatially_varying));
}

MI_PY_EXPORT(Texture) {
    MI_PY_IMPORT_TYPES(Texture, TexturePtr)
    using PyTexture = PyTexture<Float, Spectrum>;
    using Properties = mitsuba::Properties;

    auto texture = MI_PY_TRAMPOLINE_CLASS(PyTexture, Texture, Object)
        .def(nb::init<const Properties &>(), "props"_a)
        .def_static("D65", nb::overload_cast<ScalarFloat>(&Texture::D65), "scale"_a = 1.f)
        .def_method(Texture, resolution)
        .def_method(Texture, spectral_resolution)
        .def_method(Texture, wavelength_range)
        .def("__repr__", &Texture::to_string);

    bind_texture_generic<Texture *>(texture);

    if constexpr (dr::is_array_v<TexturePtr>) {
        dr::ArrayBinding b;
        auto texture_ptr = dr::bind_array_t<TexturePtr>(b, m, "TexturePtr");
        bind_texture_generic<TexturePtr>(texture_ptr);
    }

    drjit::bind_traverse(texture);
}
