#include <mitsuba/render/volume.h>
#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>

#include <array>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyVolume : public Volume<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Volume)
    NB_TRAMPOLINE(Volume, 8);

    PyVolume(const Properties &props) : Volume(props) { };

    UnpolarizedSpectrum eval(const Interaction3f &it,
                             Mask active = true) const override {
        NB_OVERRIDE_PURE(eval, it, active);
    }

    Float eval_1(const Interaction3f &it, Mask active = true) const override {
        NB_OVERRIDE_PURE(eval_1, it, active);
    }

    Vector3f eval_3(const Interaction3f &it,
                    Mask active = true) const override {
        NB_OVERRIDE_PURE(eval_3, it, active);
    }

    dr::Array<Float, 6> eval_6(const Interaction3f &it,
                               Mask active = true) const override {
        using Return = dr::Array<Float, 6>;
        NB_OVERRIDE_PURE(eval_6, it, active);
    }

    std::pair<UnpolarizedSpectrum, Vector3f>
    eval_gradient(const Interaction3f &it, Mask active = true) const override {
        using Return = std::pair<UnpolarizedSpectrum, Vector3f>;
        NB_OVERRIDE_PURE(eval_gradient, it, active);
    }

    ScalarFloat max() const override {
        NB_OVERRIDE_PURE(max);
    }

    ScalarVector3i resolution() const override {
        NB_OVERRIDE(resolution);
    }

    std::string to_string() const override {
        NB_OVERRIDE(to_string);
    }
};

MI_PY_EXPORT(Volume) {
    MI_PY_IMPORT_TYPES(Volume)
    using PyVolume = PyVolume<Float, Spectrum>;
    using Properties = PropertiesV<Float>;

    MI_PY_TRAMPOLINE_CLASS(PyVolume, Volume, Object)
        .def(nb::init<const Properties &>(), "props"_a)
        .def_method(Volume, resolution)
        .def_method(Volume, bbox)
        .def_method(Volume, channel_count)
        .def_method(Volume, max)
        .def("max_per_channel",
            [] (const Volume *volume) {
                std::vector<ScalarFloat> max_values(volume->channel_count());
                volume->max_per_channel(max_values.data());
                return max_values;
            },
            D(Volume, max_per_channel))
        .def_method(Volume, eval, "it"_a, "active"_a = true)
        .def_method(Volume, eval_1, "it"_a, "active"_a = true)
        .def_method(Volume, eval_3, "it"_a, "active"_a = true)
        .def("eval_6",
                [](const Volume &volume, const Interaction3f &it, const Mask active) {
                    dr::Array<Float, 6> result = volume.eval_6(it, active);
                    std::array<Float, 6> output;
                    for (size_t i = 0; i < 6; ++i)
                        output[i] = std::move(result.data()[i]);
                    return output;
                }, "it"_a, "active"_a = true, D(Volume, eval_6))
        .def("eval_gradient", &Volume::eval_gradient, "it"_a, "active"_a = true,
             D(Volume, eval_gradient))
        .def("eval_n",
            [] (const Volume *volume, const Interaction3f &it, Mask active = true) {
                std::vector<Float> evaluation(volume->channel_count());
                volume->eval_n(it, evaluation.data(), active);
                return evaluation;
            },
            "it"_a, "active"_a = true,
            D(Volume, eval_n));

    MI_PY_REGISTER_OBJECT("register_volume", Volume)
}
