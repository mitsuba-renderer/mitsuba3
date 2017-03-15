#include <mitsuba/core/spline.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(spline)

template MTS_EXPORT_CORE Float  eval_spline(Float,  Float,  Float,  Float,  Float);
template MTS_EXPORT_CORE FloatP eval_spline(FloatP, FloatP, FloatP, FloatP, FloatP);

template MTS_EXPORT_CORE std::pair<Float, Float>   eval_spline_d(Float,  Float, Float,  Float,  Float);
template MTS_EXPORT_CORE std::pair<FloatP, FloatP> eval_spline_d(FloatP, FloatP, FloatP, FloatP, FloatP);

template MTS_EXPORT_CORE std::pair<Float, Float>   eval_spline_i(Float,  Float,  Float,  Float,  Float);
template MTS_EXPORT_CORE std::pair<FloatP, FloatP> eval_spline_i(FloatP, FloatP, FloatP, FloatP, FloatP);

template MTS_EXPORT_CORE Float  eval_1d<true> (Float, Float, const Float*, uint32_t, Float);
template MTS_EXPORT_CORE Float  eval_1d<false>(Float, Float, const Float*, uint32_t, Float);
template MTS_EXPORT_CORE FloatP eval_1d<true> (Float, Float, const Float*, uint32_t, FloatP);
template MTS_EXPORT_CORE FloatP eval_1d<false>(Float, Float, const Float*, uint32_t, FloatP);

template MTS_EXPORT_CORE Float  eval_1d<true> (const Float*, const Float*, uint32_t, Float);
template MTS_EXPORT_CORE Float  eval_1d<false>(const Float*, const Float*, uint32_t, Float);
template MTS_EXPORT_CORE FloatP eval_1d<true> (const Float*, const Float*, uint32_t, FloatP);
template MTS_EXPORT_CORE FloatP eval_1d<false>(const Float*, const Float*, uint32_t, FloatP);

template MTS_EXPORT_CORE void integrate_1d(Float, Float, const Float*, uint32_t, Float*);
template MTS_EXPORT_CORE void integrate_1d(const Float*, const Float*, uint32_t, Float*);

template MTS_EXPORT_CORE Float  invert_1d(Float, Float, const Float*, uint32_t, Float);
template MTS_EXPORT_CORE FloatP invert_1d(Float, Float, const Float*, uint32_t, FloatP);

template MTS_EXPORT_CORE Float  invert_1d(const Float*, const Float*, uint32_t, Float);
template MTS_EXPORT_CORE FloatP invert_1d(const Float*, const Float*, uint32_t, FloatP);

template MTS_EXPORT_CORE Float  sample_1d(Float, Float, const Float*, const Float*, uint32_t, Float,  Float*,  Float*);
template MTS_EXPORT_CORE FloatP sample_1d(Float, Float, const Float*, const Float*, uint32_t, FloatP, FloatP*, FloatP*);

template MTS_EXPORT_CORE Float  sample_1d(const Float*, const Float*, const Float*, uint32_t, Float,  Float*,  Float*);
template MTS_EXPORT_CORE FloatP sample_1d(const Float*, const Float*, const Float*, uint32_t, FloatP, FloatP*, FloatP*);

template MTS_EXPORT_CORE std::pair<bool, int32_t> eval_spline_weights(Float, Float, uint32_t, Float, Float*);
template MTS_EXPORT_CORE std::pair<mask_t<FloatP>, Int32P> eval_spline_weights(Float, Float, uint32_t, FloatP, FloatP*);

template MTS_EXPORT_CORE std::pair<bool, int32_t> eval_spline_weights(const Float*, uint32_t, Float, Float*);
template MTS_EXPORT_CORE std::pair<mask_t<FloatP>, Int32P> eval_spline_weights(const Float*, uint32_t, FloatP, FloatP*);

template MTS_EXPORT_CORE Float  eval_2d<true>(const  Float*, uint32_t, const Float*,  uint32_t, const Float*, Float,  Float);
template MTS_EXPORT_CORE Float  eval_2d<false>(const Float*, uint32_t, const Float*,  uint32_t, const Float*, Float,  Float);
template MTS_EXPORT_CORE FloatP eval_2d<true>(const  Float*, uint32_t, const Float*,  uint32_t, const Float*, FloatP, FloatP);
template MTS_EXPORT_CORE FloatP eval_2d<false>(const Float*, uint32_t, const Float*,  uint32_t, const Float*, FloatP, FloatP);

NAMESPACE_END(spline)
NAMESPACE_END(mitsuba)
