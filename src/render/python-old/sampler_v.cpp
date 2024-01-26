#include <mitsuba/render/sampler.h>
#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PySampler : public Sampler<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Sampler)

    PySampler(const Properties &props) : Sampler(props) {}

    ref<Sampler> fork() override { PYBIND11_OVERRIDE_PURE(ref<Sampler>, Sampler, fork); }

    ref<Sampler> clone() override {
        PYBIND11_OVERRIDE_PURE(ref<Sampler>, Sampler, fork);
    }

    void seed(uint32_t seed, uint32_t wavefront_size = (uint32_t) -1) override {
        PYBIND11_OVERRIDE(void, Sampler, seed, seed, wavefront_size);
    }

    void advance() override { PYBIND11_OVERRIDE(void, Sampler, advance); }

    Float next_1d(Mask active = true) override {
        PYBIND11_OVERRIDE_PURE(Float, Sampler, next_1d, active);
    }

    Point2f next_2d(Mask active = true) override {
        PYBIND11_OVERRIDE_PURE(Point2f, Sampler, next_2d, active);
    }

    void set_sample_count(uint32_t spp) override {
        PYBIND11_OVERRIDE(void, Sampler, set_sample_count, spp);
    }

    void schedule_state() override { PYBIND11_OVERRIDE(void, Sampler, schedule_state); }

    void loop_put(dr::Loop<Mask> &loop) override {
        PYBIND11_OVERRIDE(void, Sampler, loop_put, loop);
    }

    std::string to_string() const override {
        PYBIND11_OVERRIDE(std::string, Sampler, to_string);
    }
};

MI_PY_EXPORT(Sampler) {
    MI_PY_IMPORT_TYPES(Sampler)
    using PySampler = PySampler<Float, Spectrum>;

    MI_PY_TRAMPOLINE_CLASS(PySampler, Sampler, Object)
        .def(py::init<const Properties&>(), "props"_a)
        .def_method(Sampler, fork)
        .def_method(Sampler, clone)
        .def_method(Sampler, sample_count)
        .def_method(Sampler, wavefront_size)
        .def_method(Sampler, set_samples_per_wavefront, "samples_per_wavefront"_a)
        .def_method(Sampler, set_sample_count, "spp"_a)
        .def_method(Sampler, advance)
        .def_method(Sampler, schedule_state)
        .def_method(Sampler, loop_put, "loop"_a)
        .def_method(Sampler, seed, "seed"_a, "wavefront_size"_a = (uint32_t) -1)
        .def_method(Sampler, next_1d, "active"_a = true)
        .def_method(Sampler, next_2d, "active"_a = true);

    MI_PY_REGISTER_OBJECT("register_sampler", Sampler)
}
