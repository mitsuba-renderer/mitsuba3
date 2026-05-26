#include <nanobind/nanobind.h>
#include <mitsuba/render/volume.h>
#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>

#include <nanobind/trampoline.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <drjit/python.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyVolume : public VolumeField<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(VolumeField)
    using BaseField = mitsuba::VolumeField<Float, Spectrum>;

    NB_TRAMPOLINE(VolumeField, 21);

    PyVolume(const Properties &props) : BaseField(props) { }

    FieldValueType out_type() const override {
        NB_OVERRIDE(out_type);
    }

    FieldDomain domain() const override {
        NB_OVERRIDE(domain);
    }

    uint32_t out_dim() const override {
        NB_OVERRIDE(out_dim);
    }

    uint32_t args_dim() const override {
        NB_OVERRIDE(args_dim);
    }

    bool supports_scalar() const override {
        NB_OVERRIDE(supports_scalar);
    }

    bool supports_jit() const override {
        NB_OVERRIDE(supports_jit);
    }

    bool supports_surface_queries() const override {
        NB_OVERRIDE(supports_surface_queries);
    }

    bool supports_interaction_queries() const override {
        NB_OVERRIDE(supports_interaction_queries);
    }

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

    void eval_n(const Interaction3f &it, Float *out,
                Mask active = true) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_n", false);
        if (ticket.key.is_valid()) {
            std::vector<Float> result =
                nb::cast<std::vector<Float>>(
                    nb_trampoline.base().attr(ticket.key)(it, active));
            uint32_t count = this->out_dim();
            if (result.size() != count)
                Throw("Volume::eval_n(): Python override returned %zu "
                      "channel(s), expected %u.", result.size(), count);
            for (size_t i = 0; i < result.size(); ++i)
                out[i] = result[i];
            return;
        }
        BaseField::eval_n(it, out, active);
    }

    std::pair<UnpolarizedSpectrum, Vector3f>
    eval_gradient(const Interaction3f &it, Mask active = true) const override {
        using Return = std::pair<UnpolarizedSpectrum, Vector3f>;
        NB_OVERRIDE_PURE(eval_gradient, it, active);
    }

    ScalarFloat max() const override {
        NB_OVERRIDE_PURE(max);
    }

    void max_per_channel(ScalarFloat *out) const override {
        nb::detail::ticket ticket(nb_trampoline, "max_per_channel", false);
        if (ticket.key.is_valid()) {
            std::vector<ScalarFloat> result =
                nb::cast<std::vector<ScalarFloat>>(
                    nb_trampoline.base().attr(ticket.key)());
            uint32_t count = this->channel_count();
            if (result.size() != count)
                Throw("Volume::max_per_channel(): Python override returned %zu "
                      "value(s), expected %u.", result.size(), count);
            for (size_t i = 0; i < result.size(); ++i)
                out[i] = result[i];
            return;
        }
        BaseField::max_per_channel(out);
    }

    ScalarVector3i resolution() const override {
        NB_OVERRIDE(resolution);
    }

    uint32_t channel_count() const override {
        NB_OVERRIDE(channel_count);
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

    DR_TRAMPOLINE_TRAVERSE_CB(VolumeField)
};

MI_PY_EXPORT(Volume) {
    MI_PY_IMPORT_TYPES(Field, VolumeField)
    using PyVolume = PyVolume<Float, Spectrum>;
    using Properties = mitsuba::Properties;

    auto volume = nb::class_<VolumeField, Field, PyVolume>(
        m, "Volume", D(Volume))
        .def(nb::init<const Properties &>(), "props"_a)
        .def("resolution", &VolumeField::resolution, D(Volume, resolution))
        .def("bbox", &VolumeField::bbox, D(Volume, bbox))
        .def("channel_count", &VolumeField::channel_count,
             D(Volume, channel_count))
        .def("max", &VolumeField::max, D(Volume, max))
        .def("max_per_channel",
            [] (const VolumeField *volume) {
                std::vector<ScalarFloat> max_values(volume->channel_count());
                volume->max_per_channel(max_values.data());
                return max_values;
            },
            D(Volume, max_per_channel))
        .def("eval",
             [] (const VolumeField *volume, const Interaction3f &it,
                 Mask active) {
                 return volume->eval(it, active);
             },
             "it"_a, "active"_a = true, D(Volume, eval))
        .def("eval_1",
             [] (const VolumeField *volume, const Interaction3f &it,
                 Mask active) {
                 return volume->eval_1(it, active);
             },
             "it"_a, "active"_a = true, D(Volume, eval_1))
        .def("eval_3",
             [] (const VolumeField *volume, const Interaction3f &it,
                 Mask active) {
                 return volume->eval_3(it, active);
             },
             "it"_a, "active"_a = true, D(Volume, eval_3))
        .def("eval_6",
             [] (const VolumeField &volume, const Interaction3f &it,
                 const Mask active) {
                 dr::Array<Float, 6> result = volume.eval_6(it, active);
                 std::array<Float, 6> output;
                 for (size_t i = 0; i < 6; ++i)
                     output[i] = std::move(result.data()[i]);
                 return output;
             },
             "it"_a, "active"_a = true, D(Volume, eval_6))
        .def("eval_gradient", &VolumeField::eval_gradient,
             "it"_a, "active"_a = true, D(Volume, eval_gradient))
        .def("eval_n",
            [] (const VolumeField *volume, const Interaction3f &it,
                Mask active = true) {
                uint32_t count = volume->out_dim();
                std::vector<Float> evaluation(count);
                volume->eval_n(it, evaluation.data(), count,
                               FieldArgs<Float>{}, active);
                return evaluation;
            },
            "it"_a, "active"_a = true, D(Volume, eval_n));

    drjit::bind_traverse(volume);
}
