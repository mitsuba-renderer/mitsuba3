#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

FloatPacket ContinuousSpectrum::pdf(FloatPacket) const {
    NotImplementedError("pdf");
}

FloatPacket ContinuousSpectrum::sample(FloatPacket, FloatPacket &,
                                       FloatPacket &) const {
    NotImplementedError("sample");
}

MTS_IMPLEMENT_CLASS(ContinuousSpectrum, Object)
NAMESPACE_END(mitsuba)
