#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Spectrum, typename Array3f>
MTS_INLINE Spectrum srgb_model_eval(const Array3f &coeff,
                                    const wavelength_t<Spectrum> &wavelengths) {
    static_assert(!is_polarized_v<Spectrum>, "srgb_model_eval(): requires unpolarized spectrum type!");

    if constexpr (is_spectral_v<Spectrum>) {
        Spectrum v = dr::fmadd(dr::fmadd(coeff.x(), wavelengths, coeff.y()), wavelengths, coeff.z());

        return dr::select(
            dr::isinf(coeff.z()), dr::fmadd(dr::sign(coeff.z()), .5f, .5f),
            dr::max(0.f, dr::fmadd(.5f * v, dr::rsqrt(dr::fmadd(v, v, 1.f)), .5f))
        );
    } else {
        Throw("srgb_model_eval(): invoked for a non-spectral color type!");
    }
}

template <typename Array3f>
MTS_INLINE dr::value_t<Array3f> srgb_model_mean(const Array3f &coeff) {
    using Float = dr::value_t<Array3f>;
    using Vec = dr::Array<Float, 16>;

    Vec lambda = dr::linspace<Vec>(MTS_CIE_MIN, MTS_CIE_MAX);
    Vec v = dr::fmadd(dr::fmadd(coeff.x(), lambda, coeff.y()), lambda, coeff.z());
    Vec result = dr::select(dr::isinf(coeff.z()), dr::fmadd(dr::sign(coeff.z()), .5f, .5f),
                        dr::max(0.f, dr::fmadd(.5f * v, dr::rsqrt(dr::fmadd(v, v, 1.f)), .5f)));
    return dr::hmean(result);
}

/**
 * Look up the model coefficients for a sRGB color value
 * @param  c An sRGB color value where all components are in [0, 1].
 * @return   Coefficients for use with \ref srgb_model_eval
 */
MTS_EXPORT_LIB dr::Array<float, 3> srgb_model_fetch(const Color<float, 3> &);

/// Sanity check: convert the coefficients back to sRGB
// MTS_EXPORT_LIB Color<float, 3> srgb_model_eval_rgb(const dr::Array<float, 3> &);

NAMESPACE_END(mitsuba)
