#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/render/sampler.h>
#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <drjit/python.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PySampler : public Sampler<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Sampler)
    NB_TRAMPOLINE(Sampler, 10);

    PySampler(const Properties &props) : Sampler(props) {}

    ref<Sampler> fork() override { NB_OVERRIDE_PURE(fork); }

    ref<Sampler> clone() override {
        NB_OVERRIDE_PURE(fork);
    }

    void seed(UInt32 seed, uint32_t wavefront_size = (uint32_t) -1) override {
        NB_OVERRIDE(seed, seed, wavefront_size);
    }

    void advance() override { NB_OVERRIDE(advance); }

    Float next_1d(Mask active = true) override {
        NB_OVERRIDE_PURE(next_1d, active);
    }

    Point2f next_2d(Mask active = true) override {
        NB_OVERRIDE_PURE(next_2d, active);
    }

    void set_sample_count(uint32_t spp) override {
        NB_OVERRIDE(set_sample_count, spp);
    }

    void schedule_state() override {
        NB_OVERRIDE(schedule_state);
    }

    std::string to_string() const override {
        NB_OVERRIDE(to_string);
    }
};

MI_PY_EXPORT(Sampler) {
    MI_PY_IMPORT_TYPES(Sampler)
    using PySampler = PySampler<Float, Spectrum>;
    using Properties = PropertiesV<Float>;

    auto sampler = MI_PY_TRAMPOLINE_CLASS(PySampler, Sampler, Object)
        .def(nb::init<const Properties&>(), "props"_a)
        .def_method(Sampler, fork)
        .def_method(Sampler, clone)
        .def_method(Sampler, sample_count)
        .def_method(Sampler, wavefront_size)
        .def_method(Sampler, set_samples_per_wavefront, "samples_per_wavefront"_a)
        .def_method(Sampler, set_sample_count, "spp"_a)
        .def_method(Sampler, advance)
        .def_method(Sampler, schedule_state)
        .def_method(Sampler, seed, "seed"_a, "wavefront_size"_a = (uint32_t) -1)
        .def_method(Sampler, next_1d, "active"_a = true)
        .def_method(Sampler, next_2d, "active"_a = true);

    dr::bind_traverse(sampler);

    MI_PY_REGISTER_OBJECT("register_sampler", Sampler)
}
