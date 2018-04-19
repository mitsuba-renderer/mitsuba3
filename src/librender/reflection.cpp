#include <mitsuba/render/reflection.h>

NAMESPACE_BEGIN(mitsuba)

template MTS_EXPORT_CORE std::tuple<float, float, float, float, float> fresnel_polarized(float, float);
template MTS_EXPORT_CORE std::tuple<FloatP, FloatP, FloatP, FloatP, FloatP> fresnel_polarized(FloatP, FloatP);
template MTS_EXPORT_CORE std::tuple<float, float, float, float> fresnel(float, float);
template MTS_EXPORT_CORE std::tuple<FloatP, FloatP, FloatP, FloatP> fresnel(FloatP, FloatP);
template MTS_EXPORT_CORE float fresnel_complex(float, Complex<float>);
template MTS_EXPORT_CORE FloatP fresnel_complex(FloatP, Complex<FloatP>);

NAMESPACE_END(mitsuba)
