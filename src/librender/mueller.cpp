#include <mitsuba/render/mueller.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(mueller)

template MTS_EXPORT_CORE MuellerMatrix<Float> depolarizer(Float);
template MTS_EXPORT_CORE MuellerMatrix<FloatP> depolarizer(FloatP);

template MTS_EXPORT_CORE MuellerMatrix<Float> rotator(Float);
template MTS_EXPORT_CORE MuellerMatrix<FloatP> rotator(FloatP);

template MTS_EXPORT_CORE MuellerMatrix<Float> rotated_element(Float, const MuellerMatrix<Float> &);
template MTS_EXPORT_CORE MuellerMatrix<FloatP> rotated_element(FloatP, const MuellerMatrix<FloatP> &);

template MTS_EXPORT_CORE MuellerMatrix<Float> specular_reflection(Float, Float);
template MTS_EXPORT_CORE MuellerMatrix<FloatP> specular_reflection(FloatP, FloatP);

template MTS_EXPORT_CORE MuellerMatrix<Float> specular_reflection(Float, Complex<Float>);
template MTS_EXPORT_CORE MuellerMatrix<FloatP> specular_reflection(FloatP, Complex<FloatP>);

template MTS_EXPORT_CORE MuellerMatrix<Float> specular_transmission(Float, Float);
template MTS_EXPORT_CORE MuellerMatrix<FloatP> specular_transmission(FloatP, FloatP);

NAMESPACE_END(mueller)
NAMESPACE_END(mitsuba)
