#pragma once

#include <enoki/fwd.h>
#include <mitsuba/mitsuba.h>
#include <type_traits>
#include <tuple>

NAMESPACE_BEGIN(mitsuba)

// =============================================================
//! @{ \name Single / double precision traits
// =============================================================

NAMESPACE_BEGIN(detail)
/// Single / double precision trait
template <typename T> struct is_double {};
template <> struct is_double<float> {
    static constexpr bool value = false;
};
template <> struct is_double<int32_t> {
    static constexpr bool value = false;
};
template <> struct is_double<uint32_t> {
    static constexpr bool value = false;
};
template <> struct is_double<double> {
    static constexpr bool value = true;
};
template <> struct is_double<int64_t> {
    static constexpr bool value = true;
};
template <> struct is_double<uint64_t> {
    static constexpr bool value = true;
};
NAMESPACE_END(detail)

template <typename T>
constexpr bool is_single_v = !detail::is_double<T>::value;
template <typename T>
constexpr bool is_double_v = detail::is_double<T>::value;

//! @}
// =============================================================

// =============================================================
//! @{ \name Color mode traits
// =============================================================
// Forward declaration
template <typename Value> using MuellerMatrix = enoki::Matrix<Value, 4, true>;

NAMESPACE_BEGIN(detail)

template <typename Spectrum> struct spectral_traits {};
template <typename Float>
struct spectral_traits<Color<Float, 1>> {
    static constexpr bool is_monochrome   = true;
    static constexpr bool is_rgb          = false;
    static constexpr bool is_spectral     = false;
    static constexpr bool is_polarized    = false;
    static constexpr size_t texture_channels = 1;
};
template <typename Float>
struct spectral_traits<Color<Float, 3>> {
    static constexpr bool is_monochrome    = false;
    static constexpr bool is_rgb           = true;
    static constexpr bool is_spectral      = false;
    static constexpr bool is_polarized     = false;
    static constexpr size_t texture_channels  = 3;
};
template <typename Float, int SpectralSamples>
struct spectral_traits<Spectrum<Float, SpectralSamples>> {
    static constexpr bool is_monochrome   = false;
    static constexpr bool is_rgb          = false;
    static constexpr bool is_spectral     = true;
    static constexpr bool is_polarized    = false;
    // The 3 sRGB spectral upsampling model coefficients are stored in textures
    static constexpr size_t texture_channels = 3;
};
template <typename Float>
struct spectral_traits<MuellerMatrix<Color<Float, 1>>> {
    static constexpr bool is_monochrome   = true;
    static constexpr bool is_rgb          = false;
    static constexpr bool is_spectral     = false;
    static constexpr bool is_polarized    = true;
    static constexpr size_t texture_channels = 1;
};
template <typename Float>
struct spectral_traits<MuellerMatrix<Color<Float, 3>>> {
    static constexpr bool is_monochrome   = false;
    static constexpr bool is_rgb          = true;
    static constexpr bool is_spectral     = false;
    static constexpr bool is_polarized    = true;
    static constexpr size_t texture_channels = 3;
};
template <typename Float, int SpectralSamples>
struct spectral_traits<MuellerMatrix<Spectrum<Float, SpectralSamples>>> {
    static constexpr bool is_monochrome   = false;
    static constexpr bool is_rgb          = false;
    static constexpr bool is_spectral     = true;
    static constexpr bool is_polarized    = true;
    // The 3 sRGB spectral upsampling model coefficients are stored in textures
    static constexpr size_t texture_channels = 3;
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

//! @}
// =============================================================

// =============================================================
//! @{ \name Buffer types
// =============================================================

NAMESPACE_BEGIN(detail)
template <typename Float, typename Enable = void> struct float_buffer_t {};
template <typename Float>
struct float_buffer_t<Float, std::enable_if_t<!is_array_v<Float>>> {
    static constexpr size_t PacketSize = enoki::max_packet_size / sizeof(float);
    using type = DynamicArray<Packet<Float, PacketSize>>;
};
template <typename FloatP>
struct float_buffer_t<FloatP, std::enable_if_t<is_array_v<FloatP>
                                               && !is_cuda_array_v<FloatP>>> {
    using type = DynamicArray<FloatP>;
};
template <typename FloatX>
struct float_buffer_t<FloatX, std::enable_if_t<is_dynamic_array_v<FloatX>>> {
    using type = FloatX;
};
template <typename FloatCD>
struct float_buffer_t<FloatCD, std::enable_if_t<is_cuda_array_v<FloatCD>
                                                || is_diff_array_v<FloatCD>>> {
    using type = FloatCD;
};
NAMESPACE_END(detail)

template <typename Float>
using FloatBuffer = typename detail::float_buffer_t<Float>::type;

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

NAMESPACE_END(mitsuba)
