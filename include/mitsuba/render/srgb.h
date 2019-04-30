#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)


template <typename Vector3, typename Value>
MTS_INLINE Value srgb_model_eval(const Vector3 &coeff_, const Value &wavelengths) {
#if RGB2SPEC_MAPPING == 2
    /// See rgb2spec.h for details on this mapping variant
    const Matrix3f M(1.00298e-4, -1.68405e-4,  6.81071e-5,
                    -9.87989e-2,  1.76582e-1, -7.77831e-2,
                     2.41114e+1, -4.52511e+1,  2.21398e+1);

    Vector3 coeff = M * coeff_;
#else
    Vector3 coeff = coeff_;
#endif

    Value v = fmadd(fmadd(coeff.x(), wavelengths, coeff.y()), wavelengths, coeff.z());

    return select(
        enoki::isinf(coeff_.z()), fmadd(sign(coeff_.z()), .5f, .5f),
        max(0.f, fmadd(.5f * v, rsqrt(fmadd(v, v, 1.f)), .5f))
    );
}

template <typename Vector3, typename Value = value_t<Vector3>>
MTS_INLINE Value srgb_model_mean(const Vector3 &coeff_) {
    using Vec = Array<Value, 16>;

#if RGB2SPEC_MAPPING == 2
    /// See rgb2spec.h for details on this mapping variant
    const Matrix3f M(1.00298e-4, -1.68405e-4,  6.81071e-5,
                    -9.87989e-2,  1.76582e-1, -7.77831e-2,
                     2.41114e+1, -4.52511e+1,  2.21398e+1);

    Vector3 coeff = M * coeff_;
#else
    Vector3 coeff = coeff_;
#endif

    Vec lambda = linspace<Vec>(MTS_WAVELENGTH_MIN, MTS_WAVELENGTH_MAX);
    Vec v = fmadd(fmadd(coeff.x(), lambda, coeff.y()), lambda, coeff.z());
    Vec result = select(enoki::isinf(coeff_.z()), fmadd(sign(coeff_.z()), .5f, .5f),
                        max(0.f, fmadd(.5f * v, rsqrt(fmadd(v, v, 1.f)), .5f)));
    return mean(result);
}

#if defined(MTS_ENABLE_AUTODIFF)
MTS_INLINE FloatD srgb_model_mean_d(const Vector3fD &coeff_) {
    Throw("Not implemented: srgb_model_mean_d");
    return zero<FloatD>(coeff_.size());
}
#endif

/**
 * Look up the model coefficients for a sRGB color value
 * @param  c An sRGB color value where all components are in [0, 1].
 * @return   Coefficients for use with \ref srgb_model_eval
 */
extern MTS_EXPORT_RENDER Vector3f srgb_model_fetch(const Color3f &c);

#if defined(MTS_ENABLE_AUTODIFF)
/// Differentiable variant of \ref srgb_model_fetch.
extern MTS_EXPORT_RENDER Vector3fD srgb_model_fetch_d(const Color3fD &c);
#endif

/// Sanity check: convert the coefficients back to sRGB
template <typename Value>
extern MTS_EXPORT_RENDER Color<Value, 3> srgb_model_eval_rgb(const Vector<Value, 3> &coeff);

NAMESPACE_END(mitsuba)
