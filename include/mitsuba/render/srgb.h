#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Spectrum, typename Array3f>
MTS_INLINE depolarized_t<Spectrum> srgb_model_eval(const Array3f &coeff_,
                                                   const wavelength_t<Spectrum> &wavelengths) {
    using SpectrumU = depolarized_t<Spectrum>;

#if RGB2SPEC_MAPPING == 2
    /// See rgb2spec.h for details on this mapping variant
    using Matrix3f = enoki::Matrix<float, 3>;
    const Matrix3f M(1.00298e-4, -1.68405e-4,  6.81071e-5,
                    -9.87989e-2,  1.76582e-1, -7.77831e-2,
                     2.41114e+1, -4.52511e+1,  2.21398e+1);

    Array3f coeff = M * coeff_;
#else
    Array3f coeff = coeff_;
#endif

    SpectrumU v = fmadd(fmadd(coeff.x(), wavelengths, coeff.y()), wavelengths, coeff.z());

    return select(
        enoki::isinf(coeff_.z()), fmadd(sign(coeff_.z()), .5f, .5f),
        max(0.f, fmadd(.5f * v, rsqrt(fmadd(v, v, 1.f)), .5f))
    );
}

template <typename Array3f>
MTS_INLINE value_t<Array3f> srgb_model_mean(const Array3f &coeff_) {
    using Float = value_t<Array3f>;
    using Vec = Array<Float, 16>;

#if RGB2SPEC_MAPPING == 2
    /// See rgb2spec.h for details on this mapping variant
    using Matrix3f = enoki::Matrix<float, 3>;
    const Matrix3f M(1.00298e-4, -1.68405e-4,  6.81071e-5,
                    -9.87989e-2,  1.76582e-1, -7.77831e-2,
                     2.41114e+1, -4.52511e+1,  2.21398e+1);

    Array3f coeff = M * coeff_;
#else
    Array3f coeff = coeff_;
#endif

    Vec lambda = linspace<Vec>(MTS_WAVELENGTH_MIN, MTS_WAVELENGTH_MAX);
    Vec v = fmadd(fmadd(coeff.x(), lambda, coeff.y()), lambda, coeff.z());
    Vec result = select(enoki::isinf(coeff_.z()), fmadd(sign(coeff_.z()), .5f, .5f),
                        max(0.f, fmadd(.5f * v, rsqrt(fmadd(v, v, 1.f)), .5f)));
    return mean(result);
}

/**
 * Look up the model coefficients for a sRGB color value
 * @param  c An sRGB color value where all components are in [0, 1].
 * @return   Coefficients for use with \ref srgb_model_eval
 */
Array<float, 3> srgb_model_fetch(const Color<float, 3> &);

/// Sanity check: convert the coefficients back to sRGB
Color<float, 3> srgb_model_eval_rgb(const Array<float, 3> &);

NAMESPACE_END(mitsuba)
