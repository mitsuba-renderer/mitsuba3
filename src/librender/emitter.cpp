#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/endpoint.h>

NAMESPACE_BEGIN(mitsuba)

Emitter::Emitter(const Properties &props)
    : Endpoint(props) { }

Emitter::~Emitter() { }

bool Emitter::is_environment() const {
    return false;
}

MTS_IMPLEMENT_CLASS(Emitter, Endpoint)
NAMESPACE_END(mitsuba)
