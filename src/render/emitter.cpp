#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/endpoint.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Emitter<Float, Spectrum>::Emitter(const Properties &props)
    : Base(props) {}
MI_VARIANT Emitter<Float, Spectrum>::~Emitter() { }

MI_IMPLEMENT_CLASS_VARIANT(Emitter, Endpoint, "emitter")
MI_INSTANTIATE_CLASS(Emitter)
NAMESPACE_END(mitsuba)
