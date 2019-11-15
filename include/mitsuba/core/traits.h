#pragma once

#include <enoki/fwd.h>
#include <mitsuba/mitsuba.h>
#include <type_traits>
#include <tuple>

NAMESPACE_BEGIN(mitsuba)

// =============================================================
//! @{ \name Color mode traits
// =============================================================
// Forward declaration
template <typename Value> using MuellerMatrix = enoki::Matrix<Value, 4, true>;

NAMESPACE_BEGIN(detail)

template <typename Spectrum> struct spectral_traits {};
template <typename Float>
struct spectral_traits<Color<Float, 1>> {
    using ScalarSpectrumType                 = Color<scalar_t<Float>, 1>;
    static constexpr bool is_monochrome      = true;
    static constexpr bool is_rgb             = false;
    static constexpr bool is_spectral        = false;
    static constexpr bool is_polarized       = false;
    static constexpr size_t texture_channels = 1;
};

template <typename Float>
struct spectral_traits<Color<Float, 3>> {
    using ScalarSpectrumType                 = Color<scalar_t<Float>, 3>;
    static constexpr bool is_monochrome      = false;
    static constexpr bool is_rgb             = true;
    static constexpr bool is_spectral        = false;
    static constexpr bool is_polarized       = false;
    static constexpr size_t texture_channels = 3;
};

template <typename Float, int SpectralSamples>
struct spectral_traits<Spectrum<Float, SpectralSamples>> {
    using ScalarSpectrumType                 = Spectrum<scalar_t<Float>, SpectralSamples>;
    static constexpr bool is_monochrome      = false;
    static constexpr bool is_rgb             = false;
    static constexpr bool is_spectral        = true;
    static constexpr bool is_polarized       = false;
    // The 3 sRGB spectral upsampling model coefficients are stored in textures
    static constexpr size_t texture_channels = 3;
};

template <typename T>
struct spectral_traits<MuellerMatrix<T>> {
    using ScalarSpectrumType                 = typename spectral_traits<T>::ScalarSpectrumType;
    static constexpr bool is_monochrome      = spectral_traits<T>::is_monochrome;
    static constexpr bool is_rgb             = spectral_traits<T>::is_rgb;
    static constexpr bool is_spectral        = spectral_traits<T>::is_spectral;
    static constexpr bool is_polarized       = true;
    static constexpr size_t texture_channels = spectral_traits<T>::texture_channels;
};

template <>
struct spectral_traits<void> {
    using ScalarSpectrumType = void;
};

NAMESPACE_END(detail)

template <typename T>
constexpr bool is_monochrome_v = detail::spectral_traits<T>::is_monochrome;
template <typename T>
constexpr bool is_rgb_v = detail::spectral_traits<T>::is_rgb;
template <typename T>
constexpr bool is_spectral_v = detail::spectral_traits<T>::is_spectral;
template <typename T>
constexpr bool is_polarized_v = detail::spectral_traits<T>::is_polarized;
template <typename T>
constexpr size_t texture_channels_v = detail::spectral_traits<T>::texture_channels;
template <typename T>
using scalar_spectrum_t = typename detail::spectral_traits<T>::ScalarSpectrumType;

//! @}
// =============================================================

// =============================================================
//! @{ \name Buffer types
// =============================================================

NAMESPACE_BEGIN(detail)
template <typename Value, typename Enable = void>
struct dynamic_buffer_t {};

template <typename Value>
struct dynamic_buffer_t<Value, std::enable_if_t<!is_dynamic_array_v<Value>>> {
    using type = DynamicArray<Packet<scalar_t<Value>>>;
};

template <typename Value>
struct dynamic_buffer_t<Value, std::enable_if_t<is_dynamic_array_v<Value>>> {
    using type = Value;
};
NAMESPACE_END(detail)

template <typename Value>
using DynamicBuffer = typename detail::dynamic_buffer_t<Value>::type;

// =============================================================
//! @{ \name Host vector
// =============================================================

template <typename T>
using host_allocator = std::conditional_t<is_cuda_array_v<T>,
                                          cuda_host_allocator<scalar_t<T>>,
                                          std::allocator<scalar_t<T>>>;

template <typename T> using host_vector = std::vector<scalar_t<T>, host_allocator<T>>;

//! @}
// =============================================================

// =============================================================
//! @{ \name Function traits
// =============================================================

/// Type trait to inspect the return and argument types of functions
template <typename T, typename SFINAE = void> struct function_traits { };

/// Vanilla function
template <typename R, typename... A> struct function_traits<R(*)(A...)> {
    using Args = std::tuple<A...>;
    using Return = R;
};

/// Method
template <typename C, typename R, typename... A> struct function_traits<R(C::*)(A...)> {
    using Class = C;
    using Args = std::tuple<A...>;
    using Return = R;
};

/// Method (const)
template <typename C, typename R, typename... A> struct function_traits<R(C::*)(A...) const> {
    using Class = C;
    using Args = std::tuple<A...>;
    using Return = R;
};

/// Lambda function -- strip lambda closure and delegate back to ``function_traits``
template <typename F>
struct function_traits<
    F, std::enable_if_t<std::is_member_function_pointer_v<decltype(
           &std::remove_reference_t<F>::operator())>>>
    : function_traits<decltype(&std::remove_reference_t<F>::operator())> { };

//! @}
// =============================================================

NAMESPACE_BEGIN(detail)

/// Type trait to strip away dynamic/masking-related type wrappers
template <typename T> struct underlying {
    using type = expr_t<T>;
};

template <typename T> struct underlying<enoki::DynamicArray<T>> {
    using type = typename underlying<T>::type;
};

template <> struct underlying<void> {
    using type = void;
};

template <typename T> struct underlying<enoki::DynamicArrayReference<T>> {
    using type = typename underlying<T>::type;
};

template <typename T> struct underlying<enoki::detail::MaskedArray<T>> {
    using type = typename underlying<T>::type;
};

template <typename T, size_t Size> struct underlying<Color<T, Size>> {
    using type = Color<typename underlying<T>::type, Size>;
};

template <typename T, size_t Size> struct underlying<Spectrum<T, Size>> {
    using type = Spectrum<typename underlying<T>::type, Size>;
};

template <typename T> struct underlying<MuellerMatrix<T>> {
    using type = MuellerMatrix<typename underlying<T>::type>;
};

NAMESPACE_END(detail)

template <typename T> using underlying_t = typename detail::underlying<T>::type;

NAMESPACE_END(mitsuba)
