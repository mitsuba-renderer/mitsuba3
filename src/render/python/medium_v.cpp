#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/python/python.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyMedium : public Medium<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Medium, Sampler, Scene, PhaseFunction)

    PyMedium(const Properties &props) : Medium(props) {}

    std::tuple<Mask, Float, Float> intersect_aabb(const Ray3f &ray) const override {
        using Return = std::tuple<Mask, Float, Float>;
        PYBIND11_OVERRIDE_PURE(Return, Medium, intersect_aabb, ray);
    }

    UnpolarizedSpectrum eval_tr_old(const MediumInteraction3f &mi,
                                    const SurfaceInteraction3f &si,
                                    Mask active) const override {
        PYBIND11_OVERRIDE_PURE(UnpolarizedSpectrum, Medium, eval_tr_old, mi, si, active);
    }

    UnpolarizedSpectrum eval_tr_new(const MediumInteraction3f &mi,
                                    const SurfaceInteraction3f &si,
                                    Mask active) const override {
        PYBIND11_OVERRIDE_PURE(UnpolarizedSpectrum, Medium, eval_tr_new, mi, si, active);
    }

    std::pair<UnpolarizedSpectrum, UnpolarizedSpectrum>
    eval_tr_and_pdf(const MediumInteraction3f &mi,
                    const SurfaceInteraction3f &si,
                    Mask active) const override {
        using Return = std::pair<UnpolarizedSpectrum, UnpolarizedSpectrum>;
        PYBIND11_OVERRIDE_PURE(Return, Medium, eval_tr_and_pdf, mi, si, active);
    }

    std::pair<MediumInteraction3f, MediumInteraction3f>
    sample_interaction_twostates(const Ray3f &ray, Float sample,
                       UInt32 channel, Mask active) const override {
        using Return = std::pair<MediumInteraction3f, MediumInteraction3f>;
        PYBIND11_OVERRIDE_PURE(Return, Medium, sample_interaction_twostates, ray, sample, channel, active);
    }

    MediumInteraction3f sample_interaction(const Ray3f &ray, Float sample,
                                           UInt32 channel, Mask active) const override {
        PYBIND11_OVERRIDE_PURE(MediumInteraction3f, Medium, sample_interaction, ray, sample, channel, active);
    }

    UnpolarizedSpectrum get_majorant(const MediumInteraction3f &mi, Mask active = true) const override {
        PYBIND11_OVERRIDE_PURE(UnpolarizedSpectrum, Medium, get_majorant, mi, active);
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi, Mask active = true) const override {
        using Return = std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>;
        PYBIND11_OVERRIDE_PURE(Return, Medium, get_scattering_coefficients, mi, active);
    }

    const PhaseFunction *old_phase_function() const override {
        PYBIND11_OVERRIDE_PURE(PhaseFunction*, Medium, old_phase_function);
    }

    const PhaseFunction *phase_function() const override {
        PYBIND11_OVERRIDE_PURE(PhaseFunction*, Medium, phase_function);
    }

    bool is_homogeneous() const override {
        PYBIND11_OVERRIDE_PURE(bool, Medium, is_homogeneous);
    }

    bool has_spectral_extinction() const override {
        PYBIND11_OVERRIDE_PURE(bool, Medium, has_spectral_extinction);
    }

    std::string to_string() const override {
        PYBIND11_OVERRIDE_PURE(std::string, Medium, to_string, );
    }

    bool use_emitter_sampling() const override {
        PYBIND11_OVERRIDE_PURE(bool, Medium, use_emitter_sampling, );
    }

};

template <typename Ptr, typename Cls> void bind_medium_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES(PhaseFunctionContext)

    cls.def("phase_function",
            [](Ptr ptr) { return ptr->phase_function(); },
            D(Medium, phase_function))
        .def("old_phase_function",
            [](Ptr ptr) { return ptr->old_phase_function(); },
            D(Medium, phase_function))
       .def("use_emitter_sampling",
            [](Ptr ptr) { return ptr->use_emitter_sampling(); },
            D(Medium, use_emitter_sampling))
       .def("is_homogeneous",
            [](Ptr ptr) { return ptr->is_homogeneous(); },
            D(Medium, is_homogeneous))
       .def("has_spectral_extinction",
            [](Ptr ptr) { return ptr->has_spectral_extinction(); },
            D(Medium, has_spectral_extinction))
       .def("get_majorant",
            [](Ptr ptr, const MediumInteraction3f &mi, Mask active) {
                return ptr->get_majorant(mi, active); },
            "mi"_a, "active"_a=true,
            D(Medium, get_majorant))
       .def("intersect_aabb",
            [](Ptr ptr, const Ray3f &ray) {
                return ptr->intersect_aabb(ray); },
            "ray"_a,
            D(Medium, intersect_aabb))
       .def("sample_interaction",
            [](Ptr ptr, const Ray3f &ray, Float sample, UInt32 channel, Mask active) {
                return ptr->sample_interaction(ray, sample, channel, active); },
            "ray"_a, "sample"_a, "channel"_a, "active"_a,
            D(Medium, sample_interaction))
       .def("sample_interaction_twostates",
            [](Ptr ptr, const Ray3f &ray, Float sample, UInt32 channel, Mask active) {
                return ptr->sample_interaction_twostates(ray, sample, channel, active); },
            "ray"_a, "sample"_a, "channel"_a, "active"_a,
            "Sample a medium interaction and evaluate it for 2 states")
       .def("eval_tr_old",
            [](Ptr ptr, const MediumInteraction3f &mi,
               const SurfaceInteraction3f &si, Mask active) {
                return ptr->eval_tr_old(mi, si, active); },
            "mi"_a, "si"_a, "active"_a,
            "Evaluate the old transmittance")
       .def("eval_tr_new",
            [](Ptr ptr, const MediumInteraction3f &mi,
               const SurfaceInteraction3f &si, Mask active) {
                return ptr->eval_tr_new(mi, si, active); },
            "mi"_a, "si"_a, "active"_a,
            "Evaluate the new transmittance")
       .def("eval_tr_and_pdf",
            [](Ptr ptr, const MediumInteraction3f &mi,
               const SurfaceInteraction3f &si, Mask active) {
                return ptr->eval_tr_and_pdf(mi, si, active); },
            "mi"_a, "si"_a, "active"_a,
            D(Medium, eval_tr_and_pdf))
       .def("get_scattering_coefficients",
            [](Ptr ptr, const MediumInteraction3f &mi, Mask active = true) {
                return ptr->get_scattering_coefficients(mi, active); },
            "mi"_a, "active"_a=true,
            D(Medium, get_scattering_coefficients));

    if constexpr (dr::is_array_v<Ptr>)
        bind_drjit_ptr_array(cls);
}

MI_PY_EXPORT(Medium) {
    MI_PY_IMPORT_TYPES(Medium, MediumPtr, Scene, Sampler)
    using PyMedium = PyMedium<Float, Spectrum>;

    auto medium = MI_PY_TRAMPOLINE_CLASS(PyMedium, Medium, Object)
            .def(py::init<const Properties &>())
            .def_method(Medium, id)
            .def("__repr__", &Medium::to_string);

    bind_medium_generic<Medium *>(medium);

    if constexpr (dr::is_array_v<MediumPtr>) {
        py::object dr       = py::module_::import("drjit"),
                   dr_array = dr.attr("ArrayBase");

        py::class_<MediumPtr> cls(m, "MediumPtr", dr_array);
        bind_medium_generic<MediumPtr>(cls);
    }


    MI_PY_REGISTER_OBJECT("register_medium", Medium)
}
