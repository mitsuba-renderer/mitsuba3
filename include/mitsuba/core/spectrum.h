#pragma once

#include <utility>

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/simd.h>

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

template <typename Value_, size_t Size_>
struct Color
    : enoki::StaticArrayImpl<Value_, Size_,
                             enoki::array_approx_v<Value_>,
                             RoundingMode::Default, false, Color<Value_, Size_>> {

    static constexpr bool Approx = enoki::array_approx_v<Value_>;

    using Base = enoki::StaticArrayImpl<Value_, Size_, Approx, RoundingMode::Default,
                                        false, Color<Value_, Size_>>;

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceValue = Color<T, Size_>;

    using ArrayType = Color;
    using MaskType = enoki::Mask<Value_, Size_, Approx, RoundingMode::Default>;

    decltype(auto) r() const { return Base::x(); }
    decltype(auto) r() { return Base::x(); }

    decltype(auto) g() const { return Base::y(); }
    decltype(auto) g() { return Base::y(); }

    decltype(auto) b() const { return Base::z(); }
    decltype(auto) b() { return Base::z(); }

    ENOKI_ARRAY_IMPORT(Base, Color)
};

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Data types for discretized spectral data
// =======================================================================

template <typename Value_>
struct Spectrum
    : enoki::StaticArrayImpl<Value_, MTS_WAVELENGTH_SAMPLES,
                             enoki::array_approx_v<Value_>,
                             RoundingMode::Default, false, Spectrum<Value_>> {

    static constexpr bool Approx = enoki::array_approx_v<Value_>;

    using Base = enoki::StaticArrayImpl<Value_, MTS_WAVELENGTH_SAMPLES, Approx,
                                        RoundingMode::Default, false, Spectrum<Value_>>;

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceValue = Spectrum<T>;

    using ArrayType = Spectrum;
    using MaskType = enoki::Mask<Value_, MTS_WAVELENGTH_SAMPLES,
                                 Approx, RoundingMode::Default>;

    ENOKI_ARRAY_IMPORT(Base, Spectrum)
};

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Masking support for color and spectrum data types
// =======================================================================

template <typename Value_, size_t Size_>
struct Color<enoki::detail::MaskedArray<Value_>, Size_> : enoki::detail::MaskedArray<Color<Value_, Size_>> {
    using Base = enoki::detail::MaskedArray<Color<Value_, Size_>>;
    using Base::Base;
    using Base::operator=;
    Color(const Base &b) : Base(b) { }
};

template <typename Value_>
struct Spectrum<enoki::detail::MaskedArray<Value_>> : enoki::detail::MaskedArray<Spectrum<Value_>> {
    using Base = enoki::detail::MaskedArray<Spectrum<Value_>>;
    using Base::Base;
    using Base::operator=;
    Spectrum(const Base &b) : Base(b) { }
};

//! @}
// =======================================================================

#define MTS_CIE_MIN     360.f
#define MTS_CIE_MAX     830.f
#define MTS_CIE_SAMPLES 95

/// Table with fits for \ref cie1931_xyz and \ref cie1931_y
extern MTS_EXPORT_CORE const Float cie1931_x_data[MTS_CIE_SAMPLES];
extern MTS_EXPORT_CORE const Float cie1931_y_data[MTS_CIE_SAMPLES];
extern MTS_EXPORT_CORE const Float cie1931_z_data[MTS_CIE_SAMPLES];

/**
 * \brief Evaluate the CIE 1931 XYZ color matching functions given a wavelength
 * in nanometers
 */
template <typename T, typename Expr = expr_t<T>>
Array<Expr, 3> cie1931_xyz(const T &wavelengths,
                           mask_t<Expr> active = true) {
    using Index = int_array_t<Expr>;

    Expr t = (wavelengths - MTS_CIE_MIN) *
             ((MTS_CIE_SAMPLES - 1) / (MTS_CIE_MAX - MTS_CIE_MIN));

    active &= wavelengths >= MTS_CIE_MIN && wavelengths <= MTS_CIE_MAX;

    Index i0 = min(max(Index(t), zero<Index>()), Index(MTS_CIE_SAMPLES - 2)),
          i1 = i0 + 1;

    Expr v0_x = gather<Expr>(cie1931_x_data, i0, active),
         v1_x = gather<Expr>(cie1931_x_data, i1, active),
         v0_y = gather<Expr>(cie1931_y_data, i0, active),
         v1_y = gather<Expr>(cie1931_y_data, i1, active),
         v0_z = gather<Expr>(cie1931_z_data, i0, active),
         v1_z = gather<Expr>(cie1931_z_data, i1, active);

    Expr w1 = t - Expr(i0),
         w0 = (Float) 1 - w1;

    return Array<Expr, 3>(fmadd(w0, v0_x, w1 * v1_x),
                          fmadd(w0, v0_y, w1 * v1_y),
                          fmadd(w0, v0_z, w1 * v1_z)) &
           Array<mask_t<Expr>, 3>(active);
}

/**
 * \brief Evaluate the CIE 1931 Y color matching function given a wavelength in
 * nanometers
 */
template <typename T, typename Expr = expr_t<T>>
Expr cie1931_y(const T &wavelengths, mask_t<Expr> active = true) {
    using Index = int_array_t<Expr>;

    Expr t = (wavelengths - MTS_CIE_MIN) *
             ((MTS_CIE_SAMPLES - 1) / (MTS_CIE_MAX - MTS_CIE_MIN));

    active &= wavelengths >= MTS_CIE_MIN && wavelengths <= MTS_CIE_MAX;

    Index i0 = min(max(Index(t), zero<Index>()), Index(MTS_CIE_SAMPLES - 2)),
          i1 = i0 + 1;

    Expr v0 = gather<Expr>(cie1931_y_data, i0, active),
         v1 = gather<Expr>(cie1931_y_data, i1, active);

    Expr w1 = t - Expr(i0),
         w0 = (Float) 1 - w1;

    return fmadd(w0, v0, w1 * v1) & active;
}

template <typename Spectrum, typename Value = value_t<Spectrum>>
Array<Value, 3> to_xyz(const Spectrum &value, const Spectrum &wavelengths,
                       mask_t<Value> active = true) {
    auto XYZw = cie1931_xyz(wavelengths, active);
    return Array<Value, 3>(enoki::mean(XYZw.x() * value), enoki::mean(XYZw.y() * value),
                           enoki::mean(XYZw.z() * value));
}

template <typename Spectrum>
value_t<Spectrum> luminance(const Spectrum &value,
                            const Spectrum &wavelengths,
                            mask_t<Spectrum> active = true) {
    return mean(cie1931_y(wavelengths, active) * value);
}

template <typename Value> Value luminance(const Color<Value, 3> &c) {
    return c[0] * 0.212671f + c[1] * 0.715160f + c[2] * 0.072169f;
}

/**
 * Importance sample a "importance spectrum" that concentrates the computation
 * on wavelengths that are relevant for rendering of RGB data
 *
 * Based on "An Improved Technique for Full Spectral Rendering"
 * by Radziszewski, Boryczko, and Alda
 *
 * Returns a tuple with the sampled wavelength and inverse PDF
 */
template <typename Value>
std::pair<Value, Value> sample_rgb_spectrum(const Value &sample) {
    if constexpr (MTS_WAVELENGTH_MIN == 360.f && MTS_WAVELENGTH_MAX == 830.f) {
        Value wavelengths =
            Float(538) - atanh(Float(0.8569106254698279) -
                               Float(1.8275019724092267) * sample) *
                             Float(138.88888888888889);

        Value tmp    = cosh(Float(0.0072) * (wavelengths - Float(538)));
        Value weight = Float(253.82) * tmp * tmp;

        return { wavelengths, weight };
    } else {
        // Fall back to uniform sampling for other wavelength ranges
        return sample_uniform_spectrum(sample);
    }
}

/**
 * PDF for the \ref sample_rgb_spectrum strategy.
 * It is valid to call this function for a single wavelength (Float), a set
 * of wavelengths (Spectrumf), a packet of wavelengths (SpectrumfP), etc. In all
 * cases, the PDF is returned per wavelength.
 */
template <typename Spectrum>
Spectrum pdf_rgb_spectrum(const Spectrum &wavelengths) {
    if constexpr (MTS_WAVELENGTH_MIN == 360.f && MTS_WAVELENGTH_MAX == 830.f) {
        auto tmp = sech(Float(0.0072) * (wavelengths - Float(538)));
        return select(wavelengths >= MTS_WAVELENGTH_MIN && wavelengths <= MTS_WAVELENGTH_MAX,
                      Float(0.003939804229326285) * tmp * tmp, zero<Spectrum>());
    } else {
        return pdf_uniform_spectrum(wavelengths);
    }
}

template <typename Value>
std::pair<Value, Value> sample_uniform_spectrum(const Value &sample) {
    return { sample * (MTS_CIE_MAX - MTS_CIE_MIN) + MTS_CIE_MIN,
             MTS_CIE_MAX - MTS_CIE_MIN };
}

template <typename Spectrum>
Spectrum pdf_uniform_spectrum(const Spectrum &/*wavelengths*/) {
    return Spectrum(1.f / (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN));
}

NAMESPACE_END(mitsuba)
