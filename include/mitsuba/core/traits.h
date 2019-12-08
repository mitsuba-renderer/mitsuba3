#pragma once

#include <enoki/fwd.h>
#include <mitsuba/mitsuba.h>
#include <type_traits>
#include <tuple>

NAMESPACE_BEGIN(mitsuba)

// =============================================================
//! @{ \name Color mode traits
// =============================================================

template <typename Value> using MuellerMatrix = enoki::Matrix<Value, 4, true>;

NAMESPACE_BEGIN(detail)

template <typename Spectrum> struct spectrum_traits { };

template <typename Float>
struct spectrum_traits<Color<Float, 1>> {
    using Scalar                             = Color<scalar_t<Float>, 1>;
    using Wavelength                         = Color<Float, 1>;
    using Unpolarized                        = Color<Float, 1>;
    static constexpr bool is_monochromatic   = true;
    static constexpr bool is_rgb             = false;
    static constexpr bool is_spectral        = false;
    static constexpr bool is_polarized       = false;
};

template <typename Float>
struct spectrum_traits<Color<Float, 3>> {
    using Scalar                             = Color<scalar_t<Float>, 3>;
    using Wavelength                         = Color<Float, 3>; // TODO don't bother storing a wavelength
    using Unpolarized                        = Color<Float, 3>;
    static constexpr bool is_monochromatic   = false;
    static constexpr bool is_rgb             = true;
    static constexpr bool is_spectral        = false;
    static constexpr bool is_polarized       = false;
};

template <typename Float, size_t Size>
struct spectrum_traits<Spectrum<Float, Size>> {
    using Scalar                             = Spectrum<scalar_t<Float>, Size>;
    using Wavelength                         = Spectrum<Float, Size>;
    using Unpolarized                        = Spectrum<Float, Size>;
    static constexpr bool is_monochromatic   = false;
    static constexpr bool is_rgb             = false;
    static constexpr bool is_spectral        = true;
    static constexpr bool is_polarized       = false;
};

template <typename T>
struct spectrum_traits<MuellerMatrix<T>> : spectrum_traits<T> {
    using Scalar                             = MuellerMatrix<typename spectrum_traits<T>::Scalar>;
    using Unpolarized                        = T;
    static constexpr bool is_polarized       = true;
};

template <>
struct spectrum_traits<void> {
    using Scalar      = void;
    using Wavelength  = void;
    using Unpolarized = void;
};

template <typename T>
struct spectrum_traits<enoki::detail::MaskedArray<T>> : spectrum_traits<T> {
    using Scalar       = enoki::detail::MaskedArray<typename spectrum_traits<T>::Scalar>;
    using Wavelength   = enoki::detail::MaskedArray<typename spectrum_traits<T>::Wavelength>;
    using Unpolarized  = enoki::detail::MaskedArray<typename spectrum_traits<T>::Unpolarized>;
};

NAMESPACE_END(detail)

template <typename T> constexpr bool is_monochromatic_v = detail::spectrum_traits<T>::is_monochromatic;
template <typename T> constexpr bool is_rgb_v = detail::spectrum_traits<T>::is_rgb;
template <typename T> constexpr bool is_spectral_v = detail::spectrum_traits<T>::is_spectral;
template <typename T> constexpr bool is_polarized_v = detail::spectrum_traits<T>::is_polarized;
template <typename T> using scalar_spectrum_t = typename detail::spectrum_traits<T>::Scalar;
template <typename T> using wavelength_t = typename detail::spectrum_traits<T>::Wavelength;
template <typename T> using depolarize_t = typename detail::spectrum_traits<T>::Unpolarized;

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

template <> struct underlying<void> {
    using type = void;
};

template <typename T> struct underlying<enoki::DynamicArray<T>> {
    using type = typename underlying<T>::type;
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

/// A variable that always evaluates to false (useful for static_assert)
template <typename... > constexpr bool false_v = false;

/// Helper function to convert GPU array resulting from horizontal operation to CPU scalar
template <typename T>
scalar_t<T> scalar_cast(const T &v) {
    if constexpr (is_cuda_array_v<T>) {
#if !defined(NDEBUG)
        if (slices(v) != 1)
            Throw("scalar_cast(): array should be of size 1!");
#endif
        return v[0];
    } else {
        return v;
    }
}

NAMESPACE_END(mitsuba)
