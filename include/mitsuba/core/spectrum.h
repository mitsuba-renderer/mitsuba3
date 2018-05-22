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
    return mean(cie1931_y(wavelengths, active) * spectrum);
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

NAMESPACE_END(mitsuba)
