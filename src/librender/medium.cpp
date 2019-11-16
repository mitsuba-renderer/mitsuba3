#include <mitsuba/render/medium.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT Medium<Float, Spectrum>::~Medium() { }

MTS_INSTANTIATE_CLASS(Medium)
NAMESPACE_END(mitsuba)
