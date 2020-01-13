#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>

/// Trampoline for derived types implemented in Python
MTS_VARIANT class PyPhaseFunction : public PhaseFunction<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(PhaseFunction)

    PyPhaseFunction(const Properties &) : PhaseFunction() {}

    Vector3f sample(const MediumInteraction3f &mi, const Point2f &sample1,
                    Mask active) const override {
        PYBIND11_OVERLOAD_PURE(Vector3f, PhaseFunction, sample, mi, sample1, active);
    }

    Float eval(const Vector3f &wo, Mask active) const override {
        PYBIND11_OVERLOAD_PURE(Float, PhaseFunction, eval, wo, active);
    }

    std::string to_string() const override {
        PYBIND11_OVERLOAD_PURE(std::string, PhaseFunction, to_string, );
    }
};

MTS_PY_EXPORT(PhaseFunction) {
    MTS_PY_IMPORT_TYPES(PhaseFunction, PhaseFunctionPtr)
    using PyPhaseFunction = PyPhaseFunction<Float, Spectrum>;
    auto phase = py::class_<PhaseFunction, PyPhaseFunction, Object, ref<PhaseFunction>>(
                        m, "PhaseFunction", D(PhaseFunction))
                        .def(py::init<const Properties &>())
                        .def("sample", vectorize(&PhaseFunction::sample), "mi"_a,
                            "sample1"_a, "active"_a = true, D(PhaseFunction, sample))
                        .def("eval", vectorize(&PhaseFunction::eval), "wo"_a,
                            "active"_a = true, D(PhaseFunction, eval))
                        .def_method(PhaseFunction, id)
                        .def("__repr__", &PhaseFunction::to_string);

    if constexpr (is_cuda_array_v<Float>) {
        pybind11_type_alias<UInt64, PhaseFunctionPtr>();
    }

    if constexpr (is_array_v<Float>) {
        phase.def_static(
            "sample_vec",
            vectorize([](const PhaseFunctionPtr &ptr, const MediumInteraction3f &mi,
                                const Point2f &s1,
                                Mask active) { return ptr->sample(mi, s1, active); }),
            "ptr"_a, "mi"_a, "sample1"_a, "active"_a = true, D(PhaseFunction, sample));
        phase.def_static("eval_vec",
                            vectorize([](const PhaseFunctionPtr &ptr, const Vector3f &wo,
                                                Mask active) { return ptr->eval(wo, active); }),
                            "ptr"_a, "wo"_a, "active"_a = true, D(PhaseFunction, eval));
    }
    MTS_PY_REGISTER_OBJECT("register_phasefunction", PhaseFunction)
}
