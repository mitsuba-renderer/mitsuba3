
#include <mitsuba/core/properties.h>
#include <mitsuba/render/phase.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT PhaseFunction<Float, Spectrum>::PhaseFunction(const Properties &props)
    : JitObject<PhaseFunction>(props.id()), m_flags(+PhaseFunctionFlags::Empty) {
}

MI_INSTANTIATE_CLASS(PhaseFunction)
NAMESPACE_END(mitsuba)
