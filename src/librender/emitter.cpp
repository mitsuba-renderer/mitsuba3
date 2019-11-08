#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/endpoint.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT Emitter<Float, Spectrum>::~Emitter() { }

MTS_INSTANTIATE_OBJECT(Emitter)
NAMESPACE_END(mitsuba)
