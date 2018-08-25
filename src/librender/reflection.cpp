#include <mitsuba/render/reflection.h>

NAMESPACE_BEGIN(mitsuba)

template MTS_EXPORT_CORE std::tuple<Float, Float, Float, Float> fresnel(Float, Float);
template MTS_EXPORT_CORE std::tuple<FloatP, FloatP, FloatP, FloatP> fresnel(FloatP, FloatP);

template MTS_EXPORT_CORE Float fresnel_conductor(Float, Complex<Float>);
template MTS_EXPORT_CORE FloatP fresnel_conductor(FloatP, Complex<FloatP>);

template MTS_EXPORT_CORE std::tuple<Complex<Float>, Complex<Float>, Float, Float, Float> fresnel_polarized(Float, Float);
template MTS_EXPORT_CORE std::tuple<Complex<FloatP>, Complex<FloatP>, FloatP, FloatP, FloatP> fresnel_polarized(FloatP, FloatP);

template MTS_EXPORT_CORE std::tuple<Complex<Float>, Complex<Float>, Float, Complex<Float>, Complex<Float>> fresnel_polarized(Float, Complex<Float>);
template MTS_EXPORT_CORE std::tuple<Complex<FloatP>, Complex<FloatP>, FloatP, Complex<FloatP>, Complex<FloatP>> fresnel_polarized(FloatP, Complex<FloatP>);

NAMESPACE_END(mitsuba)
