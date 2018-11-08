#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Vector3, typename Value>
MTS_INLINE Value srgb_model_eval(const Vector3 &coeff, const Value &wavelengths) {
    Value v = fmadd(fmadd(coeff.x(), wavelengths, coeff.y()), wavelengths, coeff.z());
    return max(0.f, fmadd(.5f * v, rsqrt(fmadd(v, v, 1.f)), .5f));
}

template <typename Vector3, typename Value = value_t<Vector3>>
MTS_INLINE Value srgb_model_mean(const Vector3 &coeff) {
    using Vec = Array<Value, 16>;
    Vec lambda = linspace<Vec>(MTS_WAVELENGTH_MIN, MTS_WAVELENGTH_MAX);
    Vec v = fmadd(fmadd(coeff.x(), lambda, coeff.y()), lambda, coeff.z());
    return mean(fmadd(.5f * v, rsqrt(fmadd(v, v, 1.f)), .5f));
}

extern MTS_EXPORT_RENDER Vector3f srgb_model_fetch(const Color3f &c);

NAMESPACE_END(mitsuba)
