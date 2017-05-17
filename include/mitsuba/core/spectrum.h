#pragma once

#include <mitsuba/core/simd.h>
#include <mitsuba/core/object.h>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

#if !defined(MTS_WAVELENGTH_SAMPLES)
#  define MTS_WAVELENGTH_SAMPLES 8
#endif

#if !defined(MTS_WAVELENGTH_MIN)
#  define MTS_WAVELENGTH_MIN Float(360)
#endif

#if !defined(MTS_WAVELENGTH_MAX)
#  define MTS_WAVELENGTH_MAX Float(830)
#endif

// =======================================================================
//! @{ \name Data types for RGB data
// =======================================================================

template <typename Type_, size_t Size_>
struct Color : enoki::StaticArrayImpl<Type_, Size_, enoki::detail::approx_default<Type_>::value,
                                       RoundingMode::Default,
                                       Color<Type_, Size_>> {

    using Base = enoki::StaticArrayImpl<Type_, Size_, enoki::detail::approx_default<Type_>::value,
                                        RoundingMode::Default,
                                        Color<Type_, Size_>>;

    ENOKI_DECLARE_CUSTOM_ARRAY(Base, Color)

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceType = Color<T, Size_>;

    using typename Base::Scalar;

    const Scalar &r() const { return Base::x(); }
    Scalar &r() { return Base::x(); }

    const Scalar &g() const { return Base::y(); }
    Scalar &g() { return Base::y(); }

    const Scalar &b() const { return Base::z(); }
    Scalar &b() { return Base::z(); }
};


using Color3f  = Color<Float,  3>;
using Color3fP = Color<FloatP, 3>;
using Color3fX = Color<FloatX, 3>;

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Data types for discretized spectral data
// =======================================================================

/**
 * The following type is used for computations involving data that
 * is sampled at a fixed number of points in the wavelength domain.
 *
 * Note that this is not restricted to radiance data -- probabilities or
 * sampling weights often need to be expressed in a spectrally varying manner,
 * and this type also applies to these situations.
 */
using DiscreteSpectrum  = enoki::Array<Float,  MTS_WAVELENGTH_SAMPLES>;
using DiscreteSpectrumP = enoki::Array<FloatP, MTS_WAVELENGTH_SAMPLES>;
using DiscreteSpectrumX = enoki::Array<FloatX, MTS_WAVELENGTH_SAMPLES>;

//! @}
// =======================================================================

/**
 * \brief Abstract continuous spectral power distribution data type,
 * which supports evaluation at arbitrary wavelengths.
 *
 * \remark The term 'continuous' doesn't necessarily mean that the underlying
 * spectrum is continuous, but rather emphasizes the fact that it is a function
 * over the reals (as opposed to the discretely sampled spectrum, which only
 * stores samples at a finite set of wavelengths).
 */
class MTS_EXPORT_CORE ContinuousSpectrum : public Object {
public:
    /**
     * Evaluate the value of the spectral power distribution
     * at a set of wavelengths
     *
     * \param lambda
     *     List of wavelengths specified in nanometers
     */
    virtual DiscreteSpectrum eval(DiscreteSpectrum lambda) const = 0;

    /// Vectorized version of \ref eval()
    virtual DiscreteSpectrumP eval(DiscreteSpectrumP lambda) const = 0;

    /**
     * \brief Importance sample the spectral power distribution
     *
     * Not every implementation may provide this function; the default
     * implementation throws an exception.
     *
     * \param sample
     *     A uniform variate
     *
     * \return
     *     1. Set of sampled wavelengths specified in nanometers
     *     2. The Monte Carlo sampling weight (SPD value divided
     *        by the sampling density)
     *     3. Sample probability per unit wavelength (in units of 1/nm)
     */
    virtual std::tuple<DiscreteSpectrum, DiscreteSpectrum,
                       DiscreteSpectrum> sample(Float sample) const;

    /// Vectorized version of \ref sample()
    virtual std::tuple<DiscreteSpectrumP, DiscreteSpectrumP,
                       DiscreteSpectrumP> sample(FloatP sample) const;

    /**
     * \brief Return the probability distribution of the \ref sample() method
     * as a probability per unit wavelength (in nm).
     *
     * Not every implementation may provide this function; the default
     * implementation throws an exception.
     */
    virtual DiscreteSpectrum pdf(DiscreteSpectrum lambda) const;

    /// Vectorized version of \ref pdf()
    virtual DiscreteSpectrumP pdf(DiscreteSpectrumP lambda) const;

    /**
     * Return the integral over the spectrum over its support
     *
     * Not every implementation may provide this function; the default
     * implementation throws an exception.
     *
     * Even if the operation is provided, it may only return an approximation.
     */
    virtual Float integral() const;

    MTS_DECLARE_CLASS()

protected:
    virtual ~ContinuousSpectrum() = default;
};

/**
 * \brief Linear interpolant of a regularly sampled spectrum
 */
class MTS_EXPORT_CORE InterpolatedSpectrum : public ContinuousSpectrum {
public:
    InterpolatedSpectrum(Float lambda_min, Float lambda_max,
                         size_t size, const Float *data);

    // =======================================================================
    //! @{ \name Implementation of the ContinuousSpectrum interface
    // =======================================================================

    virtual DiscreteSpectrum eval(DiscreteSpectrum lambda) const override;

    virtual DiscreteSpectrumP eval(DiscreteSpectrumP lambda) const override;

    virtual std::tuple<DiscreteSpectrum, DiscreteSpectrum,
                       DiscreteSpectrum> sample(Float sample) const override;

    virtual std::tuple<DiscreteSpectrumP, DiscreteSpectrumP,
                       DiscreteSpectrumP> sample(FloatP sample) const override;

    virtual DiscreteSpectrum pdf(DiscreteSpectrum lambda) const override;

    virtual DiscreteSpectrumP pdf(DiscreteSpectrumP lambda) const override;

    virtual Float integral() const override;

    //! @}
    // =======================================================================

    MTS_DECLARE_CLASS()

protected:
    virtual ~InterpolatedSpectrum() = default;

    template <typename Value> auto eval(Value lambda) const;
    template <typename Value, typename Float> auto sample(Float sample) const;

private:
    std::vector<Float> m_data, m_cdf;
    uint32_t m_size_minus_2;
    Float m_lambda_min;
    Float m_lambda_max;
    Float m_interval_size;
    Float m_inv_interval_size;
    Float m_integral;
    Float m_normalization;
};

/// Table with fits for \ref cie1931_xyz and \ref cie1931_y
extern MTS_EXPORT_CORE const Float cie1931_fits[7][4];
extern MTS_EXPORT_CORE const Float cie1931_x_data[95];
extern MTS_EXPORT_CORE const Float cie1931_y_data[95];
extern MTS_EXPORT_CORE const Float cie1931_z_data[95];

/**
 * \brief Compute the CIE 1931 XYZ color matching functions given a wavelength
 * in nanometers
 *
 * Based on "Simple Analytic Approximations to the CIE XYZ Color Matching
 * Functions" by Chris Wyman, Peter-Pike Sloan, and Peter Shirley
 * Journal of Computer Graphics Techniques Vol 2, No 2, 2013
 */
template <typename T, typename Expr = expr_t<T>>
std::tuple<Expr, Expr, Expr> cie1931_xyz(T lambda) {
#if 0
    Expr result[7];

    for (int i=0; i<7; ++i) {
        /* Coefficients of Gaussian fits */
        Float alpha(cie1931_fits[i][0]), beta (cie1931_fits[i][1]),
              gamma(cie1931_fits[i][2]), delta(cie1931_fits[i][3]);

        Expr tmp = select(lambda < beta, Expr(gamma),
                                         Expr(delta)) * (lambda - beta);

        result[i] = alpha * exp(-.5f * tmp * tmp);
    }

    return { result[0] + result[1] + result[2],
             result[3] + result[4],
             result[5] + result[6] };
#else
    using Mask = mask_t<Expr>;
    using Index = int_array_t<Expr>;

    Expr t = (lambda - 360.f) * 0.2f;
    Mask mask_valid = lambda >= 360.f & lambda <= 830.f;

    Index i0 = min(max(Index(t), zero<Index>()), Index(95-2));
    Index i1 = i0 + 1;

    Expr v0_x = gather<Expr>(cie1931_x_data, i0, mask_valid);
    Expr v1_x = gather<Expr>(cie1931_x_data, i1, mask_valid);
    Expr v0_y = gather<Expr>(cie1931_y_data, i0, mask_valid);
    Expr v1_y = gather<Expr>(cie1931_y_data, i1, mask_valid);
    Expr v0_z = gather<Expr>(cie1931_z_data, i0, mask_valid);
    Expr v1_z = gather<Expr>(cie1931_z_data, i1, mask_valid);

    Expr w1 = t - Expr(i0);
    Expr w0 = (Float) 1 - w1;

    return {
        (w0 * v0_x + w1 * v1_x) & mask_valid,
        (w0 * v0_y + w1 * v1_y) & mask_valid,
        (w0 * v0_z + w1 * v1_z) & mask_valid
    };
#endif
}

/**
 * \brief Compute the CIE 1931 Y color matching function given a wavelength
 * in nanometers
 *
 * Based on "Simple Analytic Approximations to the CIE XYZ Color Matching
 * Functions" by Chris Wyman, Peter-Pike Sloan, and Peter Shirley
 * Journal of Computer Graphics Techniques Vol 2, No 2, 2013
 */
template <typename T, typename Expr = expr_t<T>>
Expr cie1931_y(T lambda) {
#if 0
    Expr result[2];

    for (int i=0; i<2; ++i) {
        /* Coefficients of Gaussian fits */
        int j = 3 + i;
        Float alpha(cie1931_fits[j][0]), beta (cie1931_fits[j][1]),
              gamma(cie1931_fits[j][2]), delta(cie1931_fits[j][3]);

        Expr tmp = select(lambda < beta, T(gamma),
                                         T(delta)) * (lambda - beta);

        result[i] = alpha * exp(-.5f * tmp * tmp);
    }

    return result[0] + result[1];
#else
    using Mask = mask_t<Expr>;
    using Index = int_array_t<Expr>;

    Expr t = (lambda - 360.f) * 0.2f;
    Mask mask_valid = lambda >= 360.f & lambda <= 830.f;

    Index i0 = min(max(Index(t), zero<Index>()), Index(95-2));
    Index i1 = i0 + 1;

    Expr v0 = gather<Expr>(cie1931_y_data, i0, mask_valid);
    Expr v1 = gather<Expr>(cie1931_y_data, i1, mask_valid);

    Expr w1 = t - Expr(i0);
    Expr w0 = (Float) 1 - w1;

    return (w0 * v0 + w1 * v1) & mask_valid;
#endif
}

template <typename T, typename Expr = expr_t<T>>
Expr rgb_spectrum(const Color3f &rgb, T lambda) {
    const Float data[54] = {
        0.424537460743542f,  66.59311145791196f,  0.560757618949125f, /* 0: Cyan */
        0.246400896854156f,  79.07867913610922f,  0.216116362841135f,
        0.067666394964209f,  6.865886967165104f,  0.890716186803857f,
        0.092393363155047f,  531.5188158872747f,  0.425200104381996f, /* 3: Magenta */
        0.174734179228986f,  104.86530594120306f, 0.983929883263911f,
        0.613995338323662f,   79.16826549684968f, 1.003105061865860f,
        0.369673263739623f,  98.20082350314888f,  0.503666150930812f, /* 6: Yellow */
        0.558410218684172f,  21.681749660643593f, 0.878349029651678f,
        0.587945864428471f,  49.00953480003718f,  0.109960421083442f,
        0.574803873802654f, 408.25292226490063f,  0.670478585641923f, /* 9: Red */
        0.042753652345675f,  85.26554030088637f,  0.070884754752968f,
        0.669048230499984f, 145.09527307890576f,  0.957999219817480f,
        0.305242141596798f,  438.7067293053841f,  0.424248514020785f, /* 12: Green */
        0.476992126451749f,  170.7806891748844f,  0.815789194891182f,
        0.365833471799225f,  147.01853626031775f, 0.792406519710127f,
        0.144760614900738f,   69.47400741194751f, 0.993361426917213f, /* 15: Blue */
        0.600421286424602f,  134.8991351396275f,  0.074487377394544f,
        0.231505955455338f,  559.4892148020447f,  0.339396172335299f
    };

    const uint8_t cases[6][5] = {
        { 1, 4, 5,  9, 27 }, { 2, 5, 3, 18, 36 },
        { 2, 1, 0, 18, 27 }, { 0, 3, 4,  0, 45 },
        { 1, 0, 2,  9, 45 }, { 0, 2, 1,  0, 36 }
    };

    Float diff[6] = { rgb.r() - rgb.g(), rgb.g() - rgb.b(), rgb.b() - rgb.r() };
    diff[3] = -diff[0]; diff[4] = -diff[1]; diff[5] = -diff[2];

    const uint8_t *e = cases[
      (((diff[0] >  0 ? 1 : 0) |
        (diff[1] >  0 ? 2 : 0) |
        (diff[2] >  0 ? 4 : 0))) - 1];

    Expr t = (lambda - 380.f) * (1.f / (780.f - 380.f));

    const Float *g0 = data + e[3];
    Expr g0_0 = g0[2] * exp(-(t - g0[0]) * (t - g0[0]) * g0[1]);
    Expr g0_1 = g0[5] * exp(-(t - g0[3]) * (t - g0[3]) * g0[4]);
    Expr g0_2 = g0[8] * exp(-(t - g0[6]) * (t - g0[6]) * g0[7]);
    Expr g0_s = min(g0_0 + g0_1 + g0_2, Expr(1.f));

    const Float *g1 = data + e[4];
    Expr g1_0 = g1[2] * exp(-(t - g1[0]) * (t - g1[0]) * g1[1]);
    Expr g1_1 = g1[5] * exp(-(t - g1[3]) * (t - g1[3]) * g1[4]);
    Expr g1_2 = g1[8] * exp(-(t - g1[6]) * (t - g1[6]) * g1[7]);
    Expr g1_s = min(g1_0 + g1_1 + g1_2, Expr(1.f));

    return rgb[e[0]] + diff[e[1]] * g0_s + diff[e[2]] * g1_s;
}


NAMESPACE_END(mitsuba)
