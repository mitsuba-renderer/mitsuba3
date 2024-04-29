
#include <mitsuba/core/properties.h>
#include <mitsuba/render/phase.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT
PhaseFunction<Float, Spectrum>::PhaseFunction(const Properties &props)
    : m_flags(+PhaseFunctionFlags::Empty), m_id(props.id()) {
    if constexpr (dr::is_jit_v<Float>)
        jit_registry_put(dr::backend_v<Float>, "mitsuba::PhaseFunction", this);
}

MI_VARIANT PhaseFunction<Float, Spectrum>::~PhaseFunction() {
    if constexpr (dr::is_jit_v<Float>)
        jit_registry_remove(this);
}

MI_IMPLEMENT_CLASS_VARIANT(PhaseFunction, Object, "phase")
MI_INSTANTIATE_CLASS(PhaseFunction)
NAMESPACE_END(mitsuba)
