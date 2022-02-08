
#include <mitsuba/core/properties.h>
#include <mitsuba/render/phase.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT
PhaseFunction<Float, Spectrum>::PhaseFunction(const Properties &props)
    : m_flags(+PhaseFunctionFlags::None), m_id(props.id()) {}

MTS_VARIANT PhaseFunction<Float, Spectrum>::~PhaseFunction() {}

MTS_IMPLEMENT_CLASS_VARIANT(PhaseFunction, Object, "phase")
MTS_INSTANTIATE_CLASS(PhaseFunction)
NAMESPACE_END(mitsuba)
