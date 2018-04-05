#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/simd.h>
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

template <typename Value, size_t Size>
struct Color
    : enoki::StaticArrayImpl<Value, Size,
                             enoki::detail::approx_default<Value>::value,
                             RoundingMode::Default, Color<Value, Size>> {

    static constexpr bool Approx = enoki::detail::approx_default<Value>::value;

    using Base = enoki::StaticArrayImpl<Value, Size, Approx, RoundingMode::Default,
                                        Color<Value, Size>>;

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceType = Color<T, Size>;

    using MaskType = enoki::Mask<Value, Size, Approx, RoundingMode::Default>;

    using typename Base::Scalar;

    const Scalar &r() const { return Base::x(); }
    Scalar &r() { return Base::x(); }

    const Scalar &g() const { return Base::y(); }
    Scalar &g() { return Base::y(); }

    const Scalar &b() const { return Base::z(); }
    Scalar &b() { return Base::z(); }

    ENOKI_DECLARE_ARRAY(Base, Color)
};

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Data types for discretized spectral data
// =======================================================================

template <typename Value>
struct Spectrum
    : enoki::StaticArrayImpl<Value, MTS_WAVELENGTH_SAMPLES,
                             enoki::detail::approx_default<Value>::value,
                             RoundingMode::Default, Spectrum<Value>> {

    static constexpr bool Approx = enoki::detail::approx_default<Value>::value;

    using Base = enoki::StaticArrayImpl<Value, MTS_WAVELENGTH_SAMPLES, Approx,
                                        RoundingMode::Default, Spectrum<Value>>;

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceType = Spectrum<T>;

    using MaskType = enoki::Mask<Value, MTS_WAVELENGTH_SAMPLES,
                                 Approx, RoundingMode::Default>;

    ENOKI_DECLARE_ARRAY(Base, Spectrum)
};

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Masking support for color and spectrum data types
// =======================================================================

template <typename Value, size_t Size>
struct Color<enoki::detail::MaskedArray<Value>, Size> : enoki::detail::MaskedArray<Color<Value, Size>> {
    using Base = enoki::detail::MaskedArray<Color<Value, Size>>;
    using Base::Base;
    using Base::operator=;
    Color(const Base &b) : Base(b) { }
};

template <typename Value>
struct Spectrum<enoki::detail::MaskedArray<Value>> : enoki::detail::MaskedArray<Spectrum<Value>> {
    using Base = enoki::detail::MaskedArray<Spectrum<Value>>;
    using Base::Base;
    using Base::operator=;
    Spectrum(const Base &b) : Base(b) { }
};

//! @}
// =======================================================================


/**
 * \brief Abstract continuous spectral power distribution data type,
 * which supports evaluation at arbitrary wavelengths.
 *
 * \remark The term 'continuous' does not imply that the underlying spectrum
 * must be continuous, but rather emphasizes that it is a function defined on
 * the set of real numbers (as opposed to the discretely sampled spectrum,
 * which only stores samples at a finite set of wavelengths).
 */
class MTS_EXPORT_CORE ContinuousSpectrum : public Object {
public:
    /**
     * Evaluate the value of the spectral power distribution
     * at a set of wavelengths
     *
     * \param wavelengths
     *     List of wavelengths specified in nanometers
     */
    virtual Spectrumf eval(const Spectrumf &wavelengths) const;

    /// Vectorized version of \ref eval()
    virtual SpectrumfP eval(const SpectrumfP &wavelengths,
                            MaskP active = true) const;

    /// Wrapper for scalar \ref eval() with a mask (which will be ignored)
    Spectrumf eval(const Spectrumf &wavelengths, bool /*unused*/) const {
        return eval(wavelengths);
    }

    /**
     * \brief Importance sample the spectral power distribution
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     *
     * \param sample
     *     A uniform variate for each desired wavelength.
     *
     * \return
     *     1. Set of sampled wavelengths specified in nanometers
     *
     *     2. The Monte Carlo importance weight (Spectral power
     *        distribution value divided by the sampling density)
     */
    virtual std::pair<Spectrumf, Spectrumf>
    sample(const Spectrumf &sample) const;

    /// Vectorized version of \ref sample()
    virtual std::pair<SpectrumfP, SpectrumfP>
    sample(const SpectrumfP &sample, MaskP active = true) const;

    /// Wrapper for scalar \ref sample() with a mask (which will be ignored)
    std::pair<Spectrumf, Spectrumf>
    sample(const Spectrumf &s, bool /*mask*/) const {
        return sample(s);
    }

    /**
     * \brief Return the probability distribution of the \ref sample() method
     * as a probability per unit wavelength (in units of 1/nm).
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     */
    virtual Spectrumf pdf(const Spectrumf &wavelengths) const;

    /// Vectorized version of \ref pdf()
    virtual SpectrumfP pdf(const SpectrumfP &wavelengths,
                           MaskP active = true) const;

    /// Wrapper for scalar \ref pdf() with a mask (which will be ignored)
    Spectrumf pdf(const Spectrumf &wavelengths, bool /*mask*/) {
        return pdf(wavelengths);
    }

    /**
     * Return the integral over the spectrum over its support
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     *
     * Even if the operation is provided, it may only return an approximation.
     */
    virtual Float integral() const;

    /**
     * Convenience method returning the standard D65 illuminant.
     */
    static ref<ContinuousSpectrum> D65(Float scale = 1.0f);

    MTS_DECLARE_CLASS()

protected:
    virtual ~ContinuousSpectrum() = default;
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
std::tuple<Expr, Expr, Expr> cie1931_xyz(const T &wavelengths,
                                         mask_t<Expr> active = true) {
    using Index = int_array_t<Expr>;

    Expr t = (wavelengths - 360.f) * 0.2f;
    active = active && (wavelengths >= 360.f) && (wavelengths <= 830.f);

    Index i0 = min(max(Index(t), zero<Index>()), Index(95 - 2));
    Index i1 = i0 + 1;

    Expr v0_x = gather<Expr>(cie1931_x_data, i0, active);
    Expr v1_x = gather<Expr>(cie1931_x_data, i1, active);
    Expr v0_y = gather<Expr>(cie1931_y_data, i0, active);
    Expr v1_y = gather<Expr>(cie1931_y_data, i1, active);
    Expr v0_z = gather<Expr>(cie1931_z_data, i0, active);
    Expr v1_z = gather<Expr>(cie1931_z_data, i1, active);

    Expr w1 = t - Expr(i0);
    Expr w0 = (Float) 1 - w1;

    return { fmadd(w0, v0_x, w1 * v1_x) & active,
             fmadd(w0, v0_y, w1 * v1_y) & active,
             fmadd(w0, v0_z, w1 * v1_z) & active };
}

/**
 * \brief Evaluate the CIE 1931 Y color matching function given a wavelength in
 * nanometers
 */
template <typename T, typename Expr = expr_t<T>>
Expr cie1931_y(const T &wavelengths, mask_t<Expr> active = true) {
    using Index = int_array_t<Expr>;

    Expr t = (wavelengths - 360.f) * 0.2f;
    active = active && (wavelengths >= 360.f) && (wavelengths <= 830.f);

    Index i0 = min(max(Index(t), zero<Index>()), Index(95-2));
    Index i1 = i0 + 1;

    Expr v0 = gather<Expr>(cie1931_y_data, i0, active);
    Expr v1 = gather<Expr>(cie1931_y_data, i1, active);

    Expr w1 = t - Expr(i0);
    Expr w0 = (Float) 1 - w1;

    return (w0 * v0 + w1 * v1) & active;
}

template <typename Spectrum>
value_t<Spectrum> luminance(const Spectrum &spectrum, const Spectrum &wavelengths,
                            mask_t<Spectrum> active = true) {
    // CIE_Y_integral = 106.856895
    return mean(cie1931_y(wavelengths, active) * spectrum);
}

/**
 * Importance sample a "importance spectrum" that concentrates the computation
 * on wavelengths that are relevant for rendering of RGB data
 *
 * Based on "An Improved Technique for Full Spectral Rendering"
 * Radziszewski, Boryczko, and Alda
 *
 * Returns a tuple with the sampled wavelength and inverse PDF
 */
template <typename Value>
std::pair<Value, Value> sample_rgb_spectrum(const Value &sample) {
#if 1
    Value wavelengths = Float(538) -
               atanh(Float(0.8569106254698279) -
                     Float(1.8275019724092267) * sample) *
               Float(138.88888888888889);

    Value tmp = cosh(Float(0.0072) * (wavelengths - Float(538)));
    Value weight = Float(253.82) * tmp * tmp;

    return { wavelengths, weight };
#else
    return { sample * (830.f - 360.f) + 360.f, 830.f - 360.f };
#endif
}

/*
 * \brief These macros should be used in the definition of Spectrum
 * plugins to instantiate concrete versions of the the \c sample,
 * \c eval and \c pdf functions.
 */
#define MTS_IMPLEMENT_SPECTRUM()                                               \
    Spectrumf eval(const Spectrumf &wavelengths) const override {              \
        return eval_impl(wavelengths, true);                                   \
    }                                                                          \
    SpectrumfP eval(const SpectrumfP &wavelengths, MaskP active)               \
        const override {                                                       \
        return eval_impl(wavelengths, active);                                 \
    }                                                                          \
    Spectrumf pdf(const Spectrumf &wavelengths) const override {               \
        return pdf_impl(wavelengths, true);                                    \
    }                                                                          \
    SpectrumfP pdf(const SpectrumfP &wavelengths, MaskP active)                \
        const override {                                                       \
        return pdf_impl(wavelengths, active);                                  \
    }                                                                          \
    std::pair<Spectrumf, Spectrumf> sample(const Spectrumf &sample)            \
        const override {                                                       \
        return sample_impl(sample, true);                                      \
    }                                                                          \
    std::pair<SpectrumfP, SpectrumfP> sample(const SpectrumfP &sample,         \
                                             MaskP active) const override {    \
        return sample_impl(sample, active);                                    \
    }

NAMESPACE_END(mitsuba)
