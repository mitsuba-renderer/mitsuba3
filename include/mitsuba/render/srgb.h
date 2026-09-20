#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Spectrum, typename Array3f>
MI_INLINE Spectrum srgb_model_eval(const Array3f &coeff,
                                    const wavelength_t<Spectrum> &wavelengths) {
    static_assert(!is_polarized_v<Spectrum>, "srgb_model_eval(): requires unpolarized spectrum type!");

    if constexpr (is_spectral_v<Spectrum>) {
        Spectrum v = dr::fmadd(dr::fmadd(coeff.x(), wavelengths, coeff.y()), wavelengths, coeff.z());

        return dr::select(
            dr::isinf(coeff.z()), dr::fmadd(dr::sign(coeff.z()), .5f, .5f),
            dr::maximum(0.f, dr::fmadd(.5f * v, dr::rsqrt(dr::fmadd(v, v, 1.f)), .5f))
        );
    } else {
        Throw("srgb_model_eval(): invoked for a non-spectral color type!");
    }
}

template <typename Array3f>
MI_INLINE dr::value_t<Array3f> srgb_model_mean(const Array3f &coeff) {
    using Float = dr::value_t<Array3f>;
    using Vec = dr::Array<Float, 16>;

    Vec lambda = dr::linspace<Vec>(MI_CIE_MIN, MI_CIE_MAX);
    Vec v = dr::fmadd(dr::fmadd(coeff.x(), lambda, coeff.y()), lambda, coeff.z());
    Vec result = dr::select(dr::isinf(coeff.z()), dr::fmadd(dr::sign(coeff.z()), .5f, .5f),
                        dr::maximum(0.f, dr::fmadd(.5f * v, dr::rsqrt(dr::fmadd(v, v, 1.f)), .5f)));
    return dr::mean(result);
}

/**
 * \brief Spectral upsampling model for sRGB colors
 *
 * Each variant loads its own copy of the model when it is first needed. JIT
 * variants additionally upload the coefficient table to the device so that
 * colors which are only known at render time (e.g. following a parameter
 * update) can be upsampled as well.
 */
template <typename Float, typename Spectrum> struct MI_EXPORT_LIB SRGBModel {
    /**
     * \brief Look up the model coefficients of an sRGB color value
     *
     * The color components must be in the range [0, 1]. A color of type
     * \c float is processed on the host, while a color of type \c Float
     * is processed on the device in JIT variants. In the latter case,
     * gradients propagate from the returned coefficients to \c color.
     */
    template <typename T> static dr::Array<T, 3> fetch(const Color<T, 3> &color) {
        if constexpr (dr::is_jit_v<T>)
            return fetch_jit(color);
        else
            return fetch_scalar(color);
    }

    /// Release the model and its device-resident copy
    static void static_shutdown();

private:
    static dr::Array<float, 3> fetch_scalar(const Color<float, 3> &color);
    static dr::Array<Float, 3> fetch_jit(const Color<Float, 3> &color);
};

MI_EXTERN_STRUCT(SRGBModel)

NAMESPACE_END(mitsuba)
