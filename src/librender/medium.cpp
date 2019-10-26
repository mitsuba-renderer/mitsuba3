#include <mitsuba/render/medium.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
Medium<Float, Spectrum>::~Medium() { }

MTS_IMPLEMENT_CLASS_TEMPLATE(Medium, Object)
NAMESPACE_END(mitsuba)
