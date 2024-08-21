#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/list.h>
#include <drjit/python.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyPhaseFunction : public PhaseFunction<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(PhaseFunction, PhaseFunctionContext)
    NB_TRAMPOLINE(PhaseFunction, 7);

    PyPhaseFunction(const Properties &props) : PhaseFunction(props) {}

    std::tuple<Vector3f, Spectrum, Float> sample(const PhaseFunctionContext &ctx,
                                                 const MediumInteraction3f &mi,
                                                 Float sample1, const Point2f &sample2,
                                                 Mask active) const override {
        using Return = std::tuple<Vector3f, Spectrum, Float>;
        NB_OVERRIDE_PURE(sample, ctx, mi, sample1, sample2, active);
    }

    std::pair<Spectrum, Float> eval_pdf(const PhaseFunctionContext &ctx,
                                        const MediumInteraction3f &mi,
                                        const Vector3f &wo,
                                        Mask active) const override {
        using Return = std::pair<Spectrum, Float>;
        NB_OVERRIDE_PURE(eval_pdf, ctx, mi, wo, active);
    }

    Float projected_area(const MediumInteraction3f &mi, Mask active) const override {
        NB_OVERRIDE(projected_area, mi, active);
    }

    Float max_projected_area() const override {
        NB_OVERRIDE(max_projected_area);
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

    using PhaseFunction::m_flags;
    using PhaseFunction::m_components;
};

template <typename Ptr, typename Cls> void bind_phase_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES(PhaseFunctionContext)

    cls.def("sample",
            [](Ptr ptr, const PhaseFunctionContext &ctx,
               const MediumInteraction3f &mi, const Float &s1, const Point2f &s2,
               Mask active) { return ptr->sample(ctx, mi, s1, s2, active); },
            "ctx"_a, "mi"_a, "sample1"_a, "sample2"_a, "active"_a = true,
            D(PhaseFunction, sample))
       .def("eval_pdf",
            [](Ptr ptr, const PhaseFunctionContext &ctx,
               const MediumInteraction3f &mi, const Vector3f &wo,
               Mask active) { return ptr->eval_pdf(ctx, mi, wo, active); },
            "ctx"_a, "mi"_a, "wo"_a, "active"_a = true,
            D(PhaseFunction, eval_pdf))
       .def("projected_area",
            [](Ptr ptr, const MediumInteraction3f &mi,
               Mask active) { return ptr->projected_area(mi, active); },
            "mi"_a, "active"_a = true,
            D(PhaseFunction, projected_area))
       .def("max_projected_area",
            [](Ptr ptr) { return ptr->max_projected_area(); },
            D(PhaseFunction, max_projected_area))
       .def("flags", [](Ptr ptr, Mask active) { return ptr->flags(active); },
            "active"_a = true, D(PhaseFunction, flags))
       .def("component_count", [](Ptr ptr, Mask active) { return ptr->component_count(active); },
            "active"_a = true, D(PhaseFunction, component_count));
}

MI_PY_EXPORT(PhaseFunction) {
    MI_PY_IMPORT_TYPES(PhaseFunction, PhaseFunctionContext, PhaseFunctionPtr)
    using PyPhaseFunction = PyPhaseFunction<Float, Spectrum>;
    using Properties = PropertiesV<Float>;

    m.def("has_flag", [](uint32_t flags, PhaseFunctionFlags f) { return has_flag(flags, f); });
    m.def("has_flag", [](UInt32   flags, PhaseFunctionFlags f) { return has_flag(flags, f); });

    nb::class_<PhaseFunctionContext>(m, "PhaseFunctionContext", D(PhaseFunctionContext))
        .def(nb::init<Sampler*, TransportMode>(), "sampler"_a = nb::none(),
                "mode"_a = TransportMode::Radiance, D(PhaseFunctionContext, PhaseFunctionContext))
        .def_field(PhaseFunctionContext, mode,      D(PhaseFunctionContext, mode))
        .def_field(PhaseFunctionContext, sampler,   D(PhaseFunctionContext, sampler))
        .def_field(PhaseFunctionContext, type_mask, D(PhaseFunctionContext, type_mask))
        .def_field(PhaseFunctionContext, component, D(PhaseFunctionContext, component))
        .def_method(PhaseFunctionContext, reverse)
        .def_repr(PhaseFunctionContext);

    auto phase =
        MI_PY_TRAMPOLINE_CLASS(PyPhaseFunction, PhaseFunction, Object)
            .def(nb::init<const Properties &>())
            .def("flags", nb::overload_cast<size_t, Mask>(&PhaseFunction::flags, nb::const_),
                 "index"_a, "active"_a = true, D(PhaseFunction, flags, 2))
            .def_method(PhaseFunction, id)
            .def_field(PyPhaseFunction, m_flags, D(PhaseFunction, m_flags))
            .def("__repr__", &PhaseFunction::to_string);

    bind_phase_generic<PhaseFunction *>(phase);

    if constexpr (dr::is_array_v<PhaseFunctionPtr>) {
        dr::ArrayBinding b;
        auto phase_ptr = dr::bind_array_t<PhaseFunctionPtr>(b, m, "PhaseFunctionPtr");
        bind_phase_generic<PhaseFunctionPtr>(phase_ptr);
    }

    MI_PY_REGISTER_OBJECT("register_phasefunction", PhaseFunction)
}
