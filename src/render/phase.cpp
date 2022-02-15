
#include <mitsuba/core/properties.h>
#include <mitsuba/render/phase.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT
PhaseFunction<Float, Spectrum>::PhaseFunction(const Properties &props)
    : m_flags(+PhaseFunctionFlags::Empty), m_id(props.id()) {}

MI_VARIANT PhaseFunction<Float, Spectrum>::~PhaseFunction() {}

MI_IMPLEMENT_CLASS_VARIANT(PhaseFunction, Object, "phase")
MI_INSTANTIATE_CLASS(PhaseFunction)
NAMESPACE_END(mitsuba)
