#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/traits.h>
#include <mitsuba/core/math.h>
#include <drjit/matrix.h>
#include <drjit/dynamic.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Data types for RGB data
// =======================================================================

template <typename Value_, size_t Size_ = 3>
struct Color : dr::StaticArrayImpl<Value_, Size_, false, Color<Value_, Size_>> {
    using Base = dr::StaticArrayImpl<Value_, Size_, false, Color<Value_, Size_>>;

    /// Helper alias used to implement type promotion rules
    template <typename T> using ReplaceValue = Color<T, Size_>;

    using ArrayType = Color;
    using MaskType = dr::Mask<Value_, Size_>;

    decltype(auto) r() const { return Base::x(); }
    decltype(auto) r() { return Base::x(); }

    decltype(auto) g() const { return Base::y(); }
    decltype(auto) g() { return Base::y(); }

    decltype(auto) b() const { return Base::z(); }
    decltype(auto) b() { return Base::z(); }

    decltype(auto) a() const { return Base::w(); }
    decltype(auto) a() { return Base::w(); }

    DRJIT_ARRAY_IMPORT(Color, Base)
};

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Data types for spectral quantities with sampled wavelengths
// =======================================================================

template <typename Value_, size_t Size_ = 4>
struct Spectrum : dr::StaticArrayImpl<Value_, Size_, false, Spectrum<Value_, Size_>> {
    using Base = dr::StaticArrayImpl<Value_, Size_, false, Spectrum<Value_, Size_>>;

    // Never allow matrix-vector products involving this type (important for polarized rendering)
    static constexpr bool IsVector = false;

    /// Helper alias used to implement type promotion rules
    template <typename T> using ReplaceValue = Spectrum<T, Size_>;

    using ArrayType = Spectrum;
    using MaskType = dr::Mask<Value_, Size_>;

    DRJIT_ARRAY_IMPORT(Spectrum, Base)
};

/**
 * Return the (1,1) entry of a Mueller matrix. Identity function for all other types.
 *
 * This is useful for places in the renderer where we do not care about the
 * additional information tracked by the Mueller matrix.
 * For instance when performing Russian Roulette based on the path throughput or
 * when writing a final RGB pixel value to the image block.
 */
template <typename T> unpolarized_spectrum_t<T> unpolarized_spectrum(const T& spectrum) {
    if constexpr (is_polarized_v<T>) {
        // First entry of the Mueller matrix is the unpolarized spectrum
        return spectrum(0, 0);
    } else {
        return spectrum;
    }
}

/**
 * Turn a spectrum into a Mueller matrix representation that only has a non-zero
 * (1,1) entry. For all non-polarized modes, this is the identity function.
 *
 * Apart from the obvious usecase as a depolarizing Mueller matrix (e.g. for a
 * Lambertian diffuse material), this is also currently used in many BSDFs and
 * emitters where it is not clear how they should interact with polarization.
 */
template <typename T> auto depolarizer(const T &spectrum = T(1)) {
    if constexpr (is_polarized_v<T>) {
        T result = dr::zeros<T>();
        result(0, 0) = spectrum(0, 0);
        return result;
    } else {
        return spectrum;
    }
}

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Masking support for color and spectrum data types
// =======================================================================

template <typename Value_, size_t Size_>
struct Color<dr::detail::MaskedArray<Value_>, Size_>
    : dr::detail::MaskedArray<Color<Value_, Size_>> {
    using Base = dr::detail::MaskedArray<Color<Value_, Size_>>;
    using Base::Base;
    using Base::operator=;
    Color(const Base &b) : Base(b) { }
};

template <typename Value_, size_t Size_>
struct Spectrum<dr::detail::MaskedArray<Value_>, Size_>
    : dr::detail::MaskedArray<Spectrum<Value_, Size_>> {
    using Base = dr::detail::MaskedArray<Spectrum<Value_, Size_>>;
    using Base::Base;
    using Base::operator=;
    Spectrum(const Base &b) : Base(b) { }
};

//! @}
// =======================================================================

#define MI_CIE_MIN           360.f
#define MI_CIE_MAX           830.f
#define MI_CIE_SAMPLES       95

/* Scaling the CIE curves by the following constant ensures that
   a unit-valued spectrum integrates to a luminance of 1.0 */
#define MI_CIE_Y_NORMALIZATION (1.0 / 106.7502593994140625)

/**
 * D65 illuminant data from CIE, expressed as relative spectral power
 * distribution, normalized relative to the power at 560nm.
 */
const float d65_table[MI_CIE_SAMPLES] = {
    46.6383f, 49.3637f, 52.0891f, 51.0323f, 49.9755f, 52.3118f, 54.6482f,
    68.7015f, 82.7549f, 87.1204f, 91.486f,  92.4589f, 93.4318f, 90.057f,
    86.6823f, 95.7736f, 104.865f, 110.936f, 117.008f, 117.41f,  117.812f,
    116.336f, 114.861f, 115.392f, 115.923f, 112.367f, 108.811f, 109.082f,
    109.354f, 108.578f, 107.802f, 106.296f, 104.79f,  106.239f, 107.689f,
    106.047f, 104.405f, 104.225f, 104.046f, 102.023f, 100.0f,   98.1671f,
    96.3342f, 96.0611f, 95.788f,  92.2368f, 88.6856f, 89.3459f, 90.0062f,
    89.8026f, 89.5991f, 88.6489f, 87.6987f, 85.4936f, 83.2886f, 83.4939f,
    83.6992f, 81.863f,  80.0268f, 80.1207f, 80.2146f, 81.2462f, 82.2778f,
    80.281f,  78.2842f, 74.0027f, 69.7213f, 70.6652f, 71.6091f, 72.979f,
    74.349f,  67.9765f, 61.604f,  65.7448f, 69.8856f, 72.4863f, 75.087f,
    69.3398f, 63.5927f, 55.0054f, 46.4182f, 56.6118f, 66.8054f, 65.0941f,
    63.3828f, 63.8434f, 64.304f,  61.8779f, 59.4519f, 55.7054f, 51.959f,
    54.6998f, 57.4406f, 58.8765f, 60.3125f
};

/* Scaling the CIE D65 spectrum curve by the following constant ensures that
   it integrates to a luminance of 1.0 */
#define MI_CIE_D65_NORMALIZATION (1.0 / 98.99741751876255)

/**
 * \brief Struct carrying color space tables with fits for \ref cie1931_xyz and
 * \ref cie1931_y as well as corresponding precomputed ITU-R Rec. BT.709 linear
 * RGB tables.
 */
NAMESPACE_BEGIN(detail)
template <typename Float> struct CIE1932Tables {
    using FloatStorage = DynamicBuffer<Float>;

    void initialize(const float* ptr) {
        if (initialized)
            return;
        initialized = true;

        xyz = Color<FloatStorage, 3>(
            dr::load<FloatStorage>(ptr, MI_CIE_SAMPLES),
            dr::load<FloatStorage>(ptr + MI_CIE_SAMPLES, MI_CIE_SAMPLES),
            dr::load<FloatStorage>(ptr + MI_CIE_SAMPLES * 2, MI_CIE_SAMPLES)
        );

        srgb = xyz_to_srgb(xyz);

        d65 = dr::load<FloatStorage>(d65_table, MI_CIE_SAMPLES);
    }

    void release() {
        if (!initialized)
            return;
        initialized = false;

        xyz = srgb = Color<FloatStorage, 3>();
        d65 = FloatStorage();
    }

    /// CIE 1931 XYZ color tables
    Color<FloatStorage, 3> xyz;
    /// ITU-R Rec. BT.709 linear RGB tables
    Color<FloatStorage, 3> srgb;
    /// CIE D65 illuminant spectrum table
    FloatStorage d65;

private:
    bool initialized = false;
};

extern MI_EXPORT_LIB CIE1932Tables<float> color_space_tables_scalar;
#if defined(MI_ENABLE_LLVM)
extern MI_EXPORT_LIB CIE1932Tables<dr::LLVMArray<float>> color_space_tables_llvm;
#endif
#if defined(MI_ENABLE_CUDA)
extern MI_EXPORT_LIB CIE1932Tables<dr::CUDAArray<float>> color_space_tables_cuda;
#endif

template <typename Float> auto get_color_space_tables() {
#if defined(MI_ENABLE_LLVM)
    if constexpr (dr::is_llvm_v<Float>)
        return color_space_tables_llvm;
    else
#endif
#if defined(MI_ENABLE_CUDA)
    if constexpr (dr::is_cuda_v<Float>)
        return color_space_tables_cuda;
    else
#endif
    return color_space_tables_scalar;
}
NAMESPACE_END(detail)

/// Allocate arrays for the color space tables
extern MI_EXPORT_LIB void color_management_static_initialization(bool cuda, bool llvm);
extern MI_EXPORT_LIB void color_management_static_shutdown();

/**
 * \brief Evaluate the CIE 1931 XYZ color matching functions given a wavelength
 * in nanometers
 */
template <typename Float, typename Result = Color<Float, 3>>
Result cie1931_xyz(Float wavelength, dr::mask_t<Float> active = true) {
    using UInt32      = dr::uint32_array_t<Float>;
    using Float32     = dr::float32_array_t<Float>;
    using ScalarFloat = dr::scalar_t<Float>;

    Float t = (wavelength - (ScalarFloat) MI_CIE_MIN) *
              ((MI_CIE_SAMPLES - 1) /
               ((ScalarFloat) MI_CIE_MAX - (ScalarFloat) MI_CIE_MIN));

    active &= wavelength >= (ScalarFloat) MI_CIE_MIN &&
              wavelength <= (ScalarFloat) MI_CIE_MAX;

    UInt32 i0 = dr::clamp(UInt32(t), dr::zeros<UInt32>(), UInt32(MI_CIE_SAMPLES - 2)),
           i1 = i0 + 1;

    auto tables = detail::get_color_space_tables<Float32>();
    Float v0_x = (Float) dr::gather<Float32>(tables.xyz.x(), i0, active);
    Float v1_x = (Float) dr::gather<Float32>(tables.xyz.x(), i1, active);
    Float v0_y = (Float) dr::gather<Float32>(tables.xyz.y(), i0, active);
    Float v1_y = (Float) dr::gather<Float32>(tables.xyz.y(), i1, active);
    Float v0_z = (Float) dr::gather<Float32>(tables.xyz.z(), i0, active);
    Float v1_z = (Float) dr::gather<Float32>(tables.xyz.z(), i1, active);

    Float w1 = t - Float(i0),
          w0 = (ScalarFloat) 1.f - w1;

    return Result(dr::fmadd(w0, v0_x, w1 * v1_x),
                  dr::fmadd(w0, v0_y, w1 * v1_y),
                  dr::fmadd(w0, v0_z, w1 * v1_z)) & dr::mask_t<Result>(active, active, active);
}

/**
 * \brief Evaluate the CIE 1931 Y color matching function given a wavelength in
 * nanometers
 */
template <typename Float>
Float cie1931_y(Float wavelength, dr::mask_t<Float> active = true) {
    using UInt32      = dr::uint32_array_t<Float>;
    using Float32     = dr::float32_array_t<Float>;
    using ScalarFloat = dr::scalar_t<Float>;

    Float t = (wavelength - (ScalarFloat) MI_CIE_MIN) *
              ((MI_CIE_SAMPLES - 1) /
               ((ScalarFloat) MI_CIE_MAX - (ScalarFloat) MI_CIE_MIN));

    active &= wavelength >= (ScalarFloat) MI_CIE_MIN &&
              wavelength <= (ScalarFloat) MI_CIE_MAX;

    UInt32 i0 = dr::clamp(UInt32(t), dr::zeros<UInt32>(), UInt32(MI_CIE_SAMPLES - 2)),
          i1 = i0 + 1;

    auto tables = detail::get_color_space_tables<Float32>();
    Float v0 = (Float) dr::gather<Float32>(tables.xyz.y(), i0, active);
    Float v1 = (Float) dr::gather<Float32>(tables.xyz.y(), i1, active);

    Float w1 = t - Float(i0),
          w0 = (ScalarFloat) 1.f - w1;

    return dr::select(active, dr::fmadd(w0, v0, w1 * v1), 0.f);
}

/**
 * \brief Evaluate the CIE D65 illuminant spectrum given a wavelength in
 * nanometers, normalized to ensures that it integrates to a luminance of 1.0.
 */
template <typename Float>
Float cie_d65(Float wavelength, dr::mask_t<Float> active = true) {
    using UInt32      = dr::uint32_array_t<Float>;
    using Float32     = dr::float32_array_t<Float>;
    using ScalarFloat = dr::scalar_t<Float>;

    Float t = (wavelength - (ScalarFloat) MI_CIE_MIN) *
              ((MI_CIE_SAMPLES - 1) /
               ((ScalarFloat) MI_CIE_MAX - (ScalarFloat) MI_CIE_MIN));

    active &= wavelength >= (ScalarFloat) MI_CIE_MIN &&
              wavelength <= (ScalarFloat) MI_CIE_MAX;

    UInt32 i0 = dr::clamp(UInt32(t), dr::zeros<UInt32>(), UInt32(MI_CIE_SAMPLES - 2)),
           i1 = i0 + 1;

    auto tables = detail::get_color_space_tables<Float32>();
    Float v0 = (Float) dr::gather<Float32>(tables.d65, i0, active);
    Float v1 = (Float) dr::gather<Float32>(tables.d65, i1, active);

    Float w1 = t - Float(i0),
          w0 = (ScalarFloat) 1.f - w1;

    Float v = dr::fmadd(w0, v0, w1 * v1) * (ScalarFloat) MI_CIE_D65_NORMALIZATION;

    return dr::select(active, v, Float(0.f));
}
/**
 * \brief Evaluate the ITU-R Rec. BT.709 linear RGB color matching functions
 * given a wavelength in nanometers
 */
template <typename Float, typename Result = Color<Float, 3>>
Result linear_rgb_rec(Float wavelength, dr::mask_t<Float> active = true) {
    using UInt32      = dr::uint32_array_t<Float>;
    using Float32     = dr::float32_array_t<Float>;
    using ScalarFloat = dr::scalar_t<Float>;

    Float t = (wavelength - (ScalarFloat) MI_CIE_MIN) *
              ((MI_CIE_SAMPLES - 1) /
               ((ScalarFloat) MI_CIE_MAX - (ScalarFloat) MI_CIE_MIN));

    active &= wavelength >= (ScalarFloat) MI_CIE_MIN &&
              wavelength <= (ScalarFloat) MI_CIE_MAX;

    UInt32 i0 = dr::clamp(UInt32(t), dr::zeros<UInt32>(), UInt32(MI_CIE_SAMPLES - 2)),
           i1 = i0 + 1;

    auto tables = detail::get_color_space_tables<Float32>();
    Float v0_r = (Float) dr::gather<Float32>(tables.srgb.x(), i0, active);
    Float v1_r = (Float) dr::gather<Float32>(tables.srgb.x(), i1, active);
    Float v0_g = (Float) dr::gather<Float32>(tables.srgb.y(), i0, active);
    Float v1_g = (Float) dr::gather<Float32>(tables.srgb.y(), i1, active);
    Float v0_b = (Float) dr::gather<Float32>(tables.srgb.z(), i0, active);
    Float v1_b = (Float) dr::gather<Float32>(tables.srgb.z(), i1, active);

    Float w1 = t - Float(i0),
          w0 = (ScalarFloat) 1.f - w1;

    return Result(dr::fmadd(w0, v0_r, w1 * v1_r),
                  dr::fmadd(w0, v0_g, w1 * v1_g),
                  dr::fmadd(w0, v0_b, w1 * v1_b)) & dr::mask_t<Result>(active, active, active);
}

/**
 * \brief Spectral responses to XYZ normalized according to the CIE curves to
 * ensure that a unit-valued spectrum integrates to a luminance of 1.0.
 */
template <typename Float, size_t Size>
Color<Float, 3> spectrum_to_xyz(const Spectrum<Float, Size> &value,
                                const Spectrum<Float, Size> &wavelengths,
                                dr::mask_t<Float> active = true) {
    dr::Array<Spectrum<Float, Size>, 3> XYZ = cie1931_xyz(wavelengths, active);
    Color<Float, 3> res = { dr::mean(XYZ.x() * value),
                            dr::mean(XYZ.y() * value),
                            dr::mean(XYZ.z() * value) };
    return res * (dr::scalar_t<Float>) MI_CIE_Y_NORMALIZATION;
}

/**
 * \brief Spectral responses to sRGB normalized according to the CIE curves to
 * ensure that a unit-valued spectrum integrates to a luminance of 1.0.
 */
template <typename Float, size_t Size>
Color<Float, 3> spectrum_to_srgb(const Spectrum<Float, Size> &value,
                                 const Spectrum<Float, Size> &wavelengths,
                                 dr::mask_t<Float> active = true) {
    dr::Array<Spectrum<Float, Size>, 3> rgb = linear_rgb_rec(wavelengths, active);
    Color<Float, 3> res = { dr::mean(rgb.x() * value),
                            dr::mean(rgb.y() * value),
                            dr::mean(rgb.z() * value) };
    return res * (dr::scalar_t<Float>) MI_CIE_Y_NORMALIZATION;
}

/// Convert ITU-R Rec. BT.709 linear RGB to XYZ tristimulus values
template <typename Float>
Color<Float, 3> srgb_to_xyz(const Color<Float, 3> &rgb, dr::mask_t<Float> /*active*/ = true) {
    using ScalarMatrix3f = dr::Matrix<dr::scalar_t<Float>, 3>;
    const ScalarMatrix3f M(0.412453f, 0.357580f, 0.180423f,
                           0.212671f, 0.715160f, 0.072169f,
                           0.019334f, 0.119193f, 0.950227f);
    return M * rgb;
}

/// Convert XYZ tristimulus values to ITU-R Rec. BT.709 linear RGB
template <typename Float>
Color<Float, 3> xyz_to_srgb(const Color<Float, 3> &xyz, dr::mask_t<Float> /*active*/ = true) {
    using ScalarMatrix3f = dr::Matrix<dr::scalar_t<Float>, 3>;
    const ScalarMatrix3f M(3.240479f, -1.537150f, -0.498535f,
                          -0.969256f,  1.875991f,  0.041556f,
                           0.055648f, -0.204043f,  1.057311f);
    return M * xyz;
}

template<typename Spectrum>
dr::value_t<Spectrum> luminance(const Spectrum &value,
                                const wavelength_t<Spectrum> &wavelengths,
                                dr::mask_t<Spectrum> active = true) {
    if constexpr (is_rgb_v<Spectrum>) {
        DRJIT_MARK_USED(active);
        DRJIT_MARK_USED(wavelengths);
        return luminance(value);
    } else if constexpr (is_monochromatic_v<Spectrum>) {
        DRJIT_MARK_USED(active);
        DRJIT_MARK_USED(wavelengths);
        return value[0];
    } else {
        return dr::mean(cie1931_y(wavelengths, active) * value);
    }
}

template <typename Float> Float luminance(const Color<Float, 3> &c) {
    using F = dr::scalar_t<Float>;
    return c[0] * F(0.212671f) + c[1] * F(0.715160f) + c[2] * F(0.072169f);
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
    Value wavelengths =
        538.f - dr::atanh(0.8569106254698279f -
                          1.8275019724092267f * sample) * 138.88888888888889f;

    Value tmp    = dr::cosh(0.0072f * (wavelengths - 538.f));
    Value weight = 253.82f * tmp * tmp;

    return { wavelengths, weight };
}

/**
 * PDF for the \ref sample_rgb_spectrum strategy.
 * It is valid to call this function for a single wavelength (Float), a set
 * of wavelengths (Spectrumf), a packet of wavelengths (SpectrumfP), etc. In all
 * cases, the PDF is returned per wavelength.
 */
template <typename Value> Value pdf_rgb_spectrum(const Value &wavelengths) {
    Value tmp = dr::sech(0.0072f * (wavelengths - 538.f));
    return dr::select(wavelengths >= MI_CIE_MIN && wavelengths <= MI_CIE_MAX,
                      0.003939804229326285f * tmp * tmp, dr::zeros<Value>());
}

/// Helper function to sample a wavelength (and a weight) given a random number
template <typename Float, typename Spectrum>
std::pair<wavelength_t<Spectrum>, Spectrum> sample_wavelength(Float sample) {
    if constexpr (!is_spectral_v<Spectrum>) {
        DRJIT_MARK_USED(sample);
        // Wavelengths should not be used when rendering in RGB or monochromatic modes.
        return { {}, 1.f };
    } else {
        auto wav_sample = math::sample_shifted<wavelength_t<Spectrum>>(sample);
        return sample_rgb_spectrum(wav_sample);
    }
}

/**
 * \brief Read a spectral power distribution from an ASCII file.
 *
 * The data should be arranged as follows:
 * The file should contain a single measurement per line, with the corresponding
 * wavelength in nanometers and the measured value separated by a space.
 * Comments are allowed.
 *
 * \param path
 *     Path of the file to be read
 * \param wavelengths
 *     Array that will be loaded with the wavelengths stored in the file
 * \param values
 *     Array that will be loaded with the values stored in the file
 */
template <typename Scalar>
MI_EXPORT_LIB void spectrum_from_file(const fs::path &path,
                                      std::vector<Scalar> &wavelengths,
                                      std::vector<Scalar> &values);

/**
 * \brief Write a spectral power distribution to an ASCII file.
 *
 * The format is identical to that parsed by \ref spectrum_from_file().
 *
 * \param path
 *     Path to the file to be written to
 * \param wavelengths
 *     Array with the wavelengths to be stored in the file
 * \param values
 *     Array with the values to be stored in the file
 */
template <typename Scalar>
MI_EXPORT_LIB void spectrum_to_file(const fs::path &path,
                                    const std::vector<Scalar> &wavelengths,
                                    const std::vector<Scalar> &values);

/**
 * \brief Transform a spectrum into a set of equivalent sRGB coefficients
 *
 * When ``bounded`` is set, the resulting sRGB coefficients will be at most 1.0.
 * In any case, sRGB coefficients will be clamped to 0 if they are negative.
 *
 * \param wavelengths
 *     Array with the wavelengths of the spectrum
 * \param values
 *     Array with the values at the previously specified wavelengths
 * \param bounded
 *     Boolean that controls if clamping is required. (default: True)
 * \param d65
 *     Should the D65 illuminant be included in the integration. (default: False)
 */
template <typename Scalar>
MI_EXPORT_LIB Color<Scalar, 3>
spectrum_list_to_srgb(const std::vector<Scalar> &wavelengths,
                      const std::vector<Scalar> &values,
                      bool bounded = true,
                      bool d65 = false);

NAMESPACE_END(mitsuba)
