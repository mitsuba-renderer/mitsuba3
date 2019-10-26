#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/endpoint.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
Emitter<Float, Spectrum>::~Emitter() { }

MTS_IMPLEMENT_CLASS_TEMPLATE(Emitter, Endpoint)
NAMESPACE_END(mitsuba)
