#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>

/// Trampoline for derived types implemented in Python
MTS_VARIANT class PyPhaseFunction : public PhaseFunction<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(PhaseFunction, PhaseFunctionContext)

    PyPhaseFunction(const Properties &) : PhaseFunction() {}

    std::pair<Vector3f, Float> sample(const PhaseFunctionContext &ctx,
                    const MediumInteraction3f &mi, const Point2f &sample,
                    Mask active) const override {
        using Return = std::pair<Vector3f, Float>;
        PYBIND11_OVERLOAD_PURE(Return, PhaseFunction, sample, ctx, mi, sample, active);
    }

    Float eval(const PhaseFunctionContext &ctx, const MediumInteraction3f &mi,
               const Vector3f &wo, Mask active) const override {
        PYBIND11_OVERLOAD_PURE(Float, PhaseFunction, eval, ctx, mi, wo, active);
    }

    std::string to_string() const override {
        PYBIND11_OVERLOAD_PURE(std::string, PhaseFunction, to_string, );
    }
};

MTS_PY_EXPORT(PhaseFunction) {
    MTS_PY_IMPORT_TYPES(PhaseFunction, PhaseFunctionContext, PhaseFunctionPtr)
    using PyPhaseFunction = PyPhaseFunction<Float, Spectrum>;
    auto phase = py::class_<PhaseFunction, PyPhaseFunction, Object, ref<PhaseFunction>>(
                        m, "PhaseFunction", D(PhaseFunction))
                        .def(py::init<const Properties &>())
                        .def("sample", vectorize(&PhaseFunction::sample), "ctx"_a, "mi"_a,
                            "sample1"_a, "active"_a = true, D(PhaseFunction, sample))
                        .def("eval", vectorize(&PhaseFunction::eval), "ctx"_a, "mi"_a, "wo"_a,
                            "active"_a = true, D(PhaseFunction, eval))
                        .def_method(PhaseFunction, id)
                        .def("__repr__", &PhaseFunction::to_string);

    if constexpr (is_cuda_array_v<Float>) {
        pybind11_type_alias<UInt64, PhaseFunctionPtr>();
    }

    if constexpr (is_array_v<Float>) {
        phase.def_static(
            "sample_vec",
            vectorize([](const PhaseFunctionPtr &ptr, const PhaseFunctionContext &ctx,
                                const MediumInteraction3f &mi, const Point2f &s,
                                Mask active) { return ptr->sample(ctx, mi, s, active); }),
            "ptr"_a, "ctx"_a, "mi"_a, "sample"_a, "active"_a = true, D(PhaseFunction, sample));
        phase.def_static(
            "eval_vec",
            vectorize([](const PhaseFunctionPtr &ptr, const PhaseFunctionContext &ctx,
                                const MediumInteraction3f &mi, const Vector3f &wo,
                                Mask active) { return ptr->eval(ctx, mi, wo, active); }),
            "ptr"_a, "ctx"_a, "mi"_a, "wo"_a, "active"_a = true, D(PhaseFunction, eval));
    }
    MTS_PY_REGISTER_OBJECT("register_phasefunction", PhaseFunction)
}
