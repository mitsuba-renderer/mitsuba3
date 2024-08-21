#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/tuple.h>
#include <drjit/python.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyMedium : public Medium<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Medium, Sampler, Scene)
    NB_TRAMPOLINE(Medium, 6);

    PyMedium(const Properties &props) : Medium(props) {}

    std::tuple<Mask, Float, Float> intersect_aabb(const Ray3f &ray) const override {
        NB_OVERRIDE_PURE(intersect_aabb, ray);
    }

    UnpolarizedSpectrum get_majorant(const MediumInteraction3f &mi, Mask active = true) const override {
        NB_OVERRIDE_PURE(get_majorant, mi, active);
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi, Mask active = true) const override {
        NB_OVERRIDE_PURE(get_scattering_coefficients, mi, active);
    }

    std::string to_string() const override {
        NB_OVERRIDE_PURE(to_string);
    }

    void traverse(TraversalCallback *cb) override {
        NB_OVERRIDE(traverse, cb);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        NB_OVERRIDE(parameters_changed, keys);
    }

    using Medium::m_sample_emitters;
    using Medium::m_is_homogeneous;
    using Medium::m_has_spectral_extinction;
};

template <typename Ptr, typename Cls> void bind_medium_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES(PhaseFunctionContext)

    using RetPhaseFunction = std::conditional_t<drjit::is_array_v<Ptr>, PhaseFunctionPtr, drjit::scalar_t<PhaseFunctionPtr>>;

    cls.def("phase_function",
            [](Ptr ptr) -> RetPhaseFunction { return ptr->phase_function(); },
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
       .def("transmittance_eval_pdf",
            [](Ptr ptr, const MediumInteraction3f &mi,
               const SurfaceInteraction3f &si, Mask active) {
                return ptr->transmittance_eval_pdf(mi, si, active); },
            "mi"_a, "si"_a, "active"_a,
            D(Medium, transmittance_eval_pdf))
       .def("get_scattering_coefficients",
            [](Ptr ptr, const MediumInteraction3f &mi, Mask active = true) {
                return ptr->get_scattering_coefficients(mi, active); },
            "mi"_a, "active"_a=true,
            D(Medium, get_scattering_coefficients));
}

MI_PY_EXPORT(Medium) {
    MI_PY_IMPORT_TYPES(Medium, MediumPtr, Scene, Sampler)
    using PyMedium = PyMedium<Float, Spectrum>;
    using Properties = PropertiesV<Float>;

    auto medium = MI_PY_TRAMPOLINE_CLASS(PyMedium, Medium, Object)
        .def(nb::init<const Properties &>(), "props"_a)
        .def_method(Medium, id)
        .def_method(Medium, set_id)
        .def_field(PyMedium, m_sample_emitters, D(Medium, m_sample_emitters))
        .def_field(PyMedium, m_is_homogeneous, D(Medium, m_is_homogeneous))
        .def_field(PyMedium, m_has_spectral_extinction, D(Medium, m_has_spectral_extinction))
        .def("__repr__", &Medium::to_string, D(Medium, to_string));

    bind_medium_generic<Medium *>(medium);

    if constexpr (dr::is_array_v<MediumPtr>) {
        dr::ArrayBinding b;
        auto medium_ptr = dr::bind_array_t<MediumPtr>(b, m, "MediumPtr");
        bind_medium_generic<MediumPtr>(medium_ptr);
    }

    MI_PY_REGISTER_OBJECT("register_medium", Medium)
}
