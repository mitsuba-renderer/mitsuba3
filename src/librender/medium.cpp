#include <mitsuba/render/medium.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
Medium<Float, Spectrum>::~Medium() { }

MTS_INSTANTIATE_OBJECT(Medium)
NAMESPACE_END(mitsuba)
