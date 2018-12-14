#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Vector3, typename Value>
MTS_INLINE Value srgb_model_eval(const Vector3 &coeff, const Value &wavelengths) {
    Value v = fmadd(fmadd(coeff.x(), wavelengths, coeff.y()), wavelengths, coeff.z());

    return select(
        enoki::isinf(v), fmadd(sign(v), .5f, .5f),
        max(0.f, fmadd(.5f * v, rsqrt(fmadd(v, v, 1.f)), .5f))
    );
}

template <typename Vector3, typename Value = value_t<Vector3>>
MTS_INLINE Value srgb_model_mean(const Vector3 &coeff) {
    using Vec = Array<Value, 16>;
    Vec lambda = linspace<Vec>(MTS_WAVELENGTH_MIN, MTS_WAVELENGTH_MAX);
    Vec v = fmadd(fmadd(coeff.x(), lambda, coeff.y()), lambda, coeff.z());
    Vec result = select(enoki::isinf(v), fmadd(sign(v), .5f, .5f),
                        max(0.f, fmadd(.5f * v, rsqrt(fmadd(v, v, 1.f)), .5f)));
    return mean(result);
}

/**
 * Look up the model coefficients for a sRGB color value
 * @param  c An sRGB color value where all components are in [0, 1].
 * @return   Coefficients for use with \ref srgb_model_eval
 */
extern MTS_EXPORT_RENDER Vector3f srgb_model_fetch(const Color3f &c);

/// Sanity check: convert the coefficients back to sRGB
extern MTS_EXPORT_RENDER Color3f srgb_model_eval_rgb(const Vector3f &lambda);

NAMESPACE_END(mitsuba)
