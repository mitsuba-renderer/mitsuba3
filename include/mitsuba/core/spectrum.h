#pragma once

#include <mitsuba/core/simd.h>
#include <mitsuba/core/object.h>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

#if !defined(MTS_WAVELENGTH_SAMPLES)
#  define MTS_WAVELENGTH_SAMPLES 4
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
struct Color
    : enoki::StaticArrayImpl<Type_, Size_,
                             enoki::detail::approx_default<Type_>::value,
                             RoundingMode::Default, Color<Type_, Size_>> {

    using Base =
        enoki::StaticArrayImpl<Type_, Size_,
                               enoki::detail::approx_default<Type_>::value,
                               RoundingMode::Default, Color<Type_, Size_>>;

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

template <typename Type_>
struct Spectrum
    : enoki::StaticArrayImpl<Type_, MTS_WAVELENGTH_SAMPLES,
                             enoki::detail::approx_default<Type_>::value,
                             RoundingMode::Default, Spectrum<Type_>> {

    using Base =
        enoki::StaticArrayImpl<Type_, MTS_WAVELENGTH_SAMPLES,
                               enoki::detail::approx_default<Type_>::value,
                               RoundingMode::Default, Spectrum<Type_>>;

    ENOKI_DECLARE_CUSTOM_ARRAY(Base, Spectrum)

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceType = Spectrum<T>;
};

/**
 * The following types are used for computations involving data that
 * is sampled at a fixed number of points in the wavelength domain.
 *
 * Note that this is not restricted to radiance data -- probabilities or
 * sampling weights often need to be expressed in a spectrally varying manner,
 * and this type also applies to these situations.
 */
using Spectrumf  = Spectrum<Float>;
using SpectrumfP = Spectrum<FloatP>;
using SpectrumfX = Spectrum<FloatX>;

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
    using Mask = mask_t<FloatP>;

    /**
     * Evaluate the value of the spectral power distribution
     * at a set of wavelengths
     *
     * \param lambda
     *     List of wavelengths specified in nanometers
     */
    virtual Spectrumf eval(const Spectrumf &lambda) const = 0;

    /// Vectorized version of \ref eval()
    virtual SpectrumfP eval(const SpectrumfP &lambda,
                            const Mask &active = true) const = 0;

    /**
     * \brief Importance sample the spectral power distribution
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     *
     * \param sample
     *     A uniform variate
     *
     * \return
     *     1. Set of sampled wavelengths specified in nanometers
     *
     *     2. The Monte Carlo sampling weight (SPD value divided
     *        by the sampling density)
     *
     *     3. Sample probability per unit wavelength (in units of 1/nm)
     */
    virtual std::tuple<Spectrumf, Spectrumf, Spectrumf>
    sample(const Spectrumf &sample) const;

    /// Vectorized version of \ref sample()
    virtual std::tuple<SpectrumfP, SpectrumfP, SpectrumfP>
    sample(const SpectrumfP &sample,
           const Mask &active = true) const;

    /**
     * \brief Return the probability distribution of the \ref sample() method
     * as a probability per unit wavelength (in units of 1/nm).
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     */
    virtual Spectrumf pdf(const Spectrumf &lambda) const;

    /// Vectorized version of \ref pdf()
    virtual SpectrumfP pdf(const SpectrumfP &lambda,
                           const Mask &active = true) const;

    /**
     * Return the integral over the spectrum over its support
     *
     * Not every implementation necessarily provides this function. The default
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
    /**
     * \brief Construct a linearly interpolated spectrum
     *
     * \param lambda_min
     *      Lowest wavelength value associated with a sample
     *
     * \param lambda_max
     *      Largest wavelength value associated with a sample
     *
     * \param size
     *      Number of sample values
     *
     * \param data
     *      Pointer to the sample values. The data is copied,
     *      hence there is no need to keep 'data' alive.
     */
    InterpolatedSpectrum(Float lambda_min, Float lambda_max,
                         size_t size, const Float *data);

    // =======================================================================
    //! @{ \name Implementation of the ContinuousSpectrum interface
    // =======================================================================

    virtual Spectrumf eval(const Spectrumf &lambda) const override;

    virtual SpectrumfP eval(const SpectrumfP &lambda,
                            const Mask &active) const override;

    virtual std::tuple<Spectrumf, Spectrumf, Spectrumf>
    sample(const Spectrumf &sample) const override;

    virtual std::tuple<SpectrumfP, SpectrumfP, SpectrumfP>
    sample(const SpectrumfP &sample,
           const Mask &active) const override;

    virtual Spectrumf pdf(const Spectrumf &lambda) const override;

    virtual SpectrumfP pdf(const SpectrumfP &lambda,
                           const Mask &active) const override;

    virtual Float integral() const override;

    //! @}
    // =======================================================================

    MTS_DECLARE_CLASS()

protected:
    virtual ~InterpolatedSpectrum() = default;

    template <typename Value>
    auto eval_impl(Value lambda, mask_t<Value> active = true) const;

    template <typename Value>
    auto sample_impl(Value sample, mask_t<Value> active = true) const;

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
extern MTS_EXPORT_CORE const Float cie1931_x_data[95];
extern MTS_EXPORT_CORE const Float cie1931_y_data[95];
extern MTS_EXPORT_CORE const Float cie1931_z_data[95];

/**
 * \brief Evaluate the CIE 1931 XYZ color matching functions given a wavelength
 * in nanometers
 */
template <typename T, typename Expr = expr_t<T>>
std::tuple<Expr, Expr, Expr> cie1931_xyz(const T &lambda, const mask_t<Expr> &active_ = true) {
    mask_t<Expr> active(active_);
    using Index = int_array_t<Expr>;

    Expr t = (lambda - 360.f) * 0.2f;
    active &= lambda >= 360.f & lambda <= 830.f;

    Index i0 = min(max(Index(t), zero<Index>()), Index(95-2));
    Index i1 = i0 + 1;

    Expr v0_x = gather<Expr>(cie1931_x_data, i0, active);
    Expr v1_x = gather<Expr>(cie1931_x_data, i1, active);
    Expr v0_y = gather<Expr>(cie1931_y_data, i0, active);
    Expr v1_y = gather<Expr>(cie1931_y_data, i1, active);
    Expr v0_z = gather<Expr>(cie1931_z_data, i0, active);
    Expr v1_z = gather<Expr>(cie1931_z_data, i1, active);

    Expr w1 = t - Expr(i0);
    Expr w0 = (Float) 1 - w1;

    return { (w0 * v0_x + w1 * v1_x) & active,
             (w0 * v0_y + w1 * v1_y) & active,
             (w0 * v0_z + w1 * v1_z) & active };
}

/**
 * \brief Evaluate the CIE 1931 Y color matching function given a wavelength in
 * nanometers
 */
template <typename T, typename Expr = expr_t<T>>
Expr cie1931_y(const T &lambda, const mask_t<Expr> &active_ = true) {
    using Index = int_array_t<Expr>;

    mask_t<Expr> active(active_);

    Expr t = (lambda - 360.f) * 0.2f;
    active &= lambda >= 360.f & lambda <= 830.f;

    Index i0 = min(max(Index(t), zero<Index>()), Index(95-2));
    Index i1 = i0 + 1;

    Expr v0 = gather<Expr>(cie1931_y_data, i0, active);
    Expr v1 = gather<Expr>(cie1931_y_data, i1, active);

    Expr w1 = t - Expr(i0);
    Expr w0 = (Float) 1 - w1;

    return (w0 * v0 + w1 * v1) & active;
}

template <typename T, typename Expr = expr_t<T>>
Expr rgb_spectrum(const Color3f &rgb, const T &lambda) {
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
    Expr g0_s = min(g0_0 + g0_1 + g0_2, 1.f);

    const Float *g1 = data + e[4];
    Expr g1_0 = g1[2] * exp(-(t - g1[0]) * (t - g1[0]) * g1[1]);
    Expr g1_1 = g1[5] * exp(-(t - g1[3]) * (t - g1[3]) * g1[4]);
    Expr g1_2 = g1[8] * exp(-(t - g1[6]) * (t - g1[6]) * g1[7]);
    Expr g1_s = min(g1_0 + g1_1 + g1_2, 1.f);

    return rgb[e[0]] + diff[e[1]] * g0_s + diff[e[2]] * g1_s;
}


NAMESPACE_END(mitsuba)
