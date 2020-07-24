#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Spectrum, typename Array3f>
MTS_INLINE Spectrum srgb_model_eval(const Array3f &coeff,
                                    const wavelength_t<Spectrum> &wavelengths) {
    static_assert(!is_polarized_v<Spectrum>, "srgb_model_eval(): requires unpolarized spectrum type!");

    if constexpr (is_spectral_v<Spectrum>) {
        Spectrum v = ek::fmadd(ek::fmadd(coeff.x(), wavelengths, coeff.y()), wavelengths, coeff.z());

        return ek::select(
            ek::isinf(coeff.z()), ek::fmadd(ek::sign(coeff.z()), 0.5f, 0.5f),
            ek::max(0.f, ek::fmadd(.5f * v, ek::rsqrt(ek::fmadd(v, v, 1.f)), 0.5f))
        );
    } else {
        Throw("srgb_model_eval(): invoked for a non-spectral color type!");
    }
}

template <typename Array3f>
MTS_INLINE ek::value_t<Array3f> srgb_model_mean(const Array3f &coeff) {
    using Float = ek::value_t<Array3f>;
    using Vec = ek::Array<Float, 16>;

    Vec lambda = ek::linspace<Vec>(MTS_WAVELENGTH_MIN, MTS_WAVELENGTH_MAX);
    Vec v = ek::fmadd(ek::fmadd(coeff.x(), lambda, coeff.y()), lambda, coeff.z());
    Vec result = ek::select(ek::isinf(coeff.z()), ek::fmadd(ek::sign(coeff.z()), 0.5f, 0.5f),
                        ek::max(0.f, ek::fmadd(.5f * v, ek::rsqrt(ek::fmadd(v, v, 1.f)), 0.5f)));
    return ek::hmean(result);
}

/**
 * Look up the model coefficients for a sRGB color value
 * @param  c An sRGB color value where all components are in [0, 1].
 * @return   Coefficients for use with \ref srgb_model_eval
 */
MTS_EXPORT_RENDER ek::Array<float, 3> srgb_model_fetch(const Color<float, 3> &);

/// Sanity check: convert the coefficients back to sRGB
// MTS_EXPORT_RENDER Color<float, 3> srgb_model_eval_rgb(const ek::Array<float, 3> &);

NAMESPACE_END(mitsuba)
