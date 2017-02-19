#pragma once

#include <mitsuba/core/logger.h>
#include <mitsuba/core/simd.h>

NAMESPACE_BEGIN(mitsuba)

// =============================================================
//! @{ \name Elementary vector, point, and normal data types
// =============================================================

template <typename Type_, size_t Size_>
struct Vector : enoki::StaticArrayImpl<Type_, Size_, false,
                                       RoundingMode::Default,
                                       Vector<Type_, Size_>> {
    using Base = enoki::StaticArrayImpl<Type_, Size_, false,
                                        RoundingMode::Default,
                                        Vector<Type_, Size_>>;
    using Base::Base;
    using Base::operator=;

    using Point = mitsuba::Point<Type_, Size_>;

    Vector() { }
};


template <typename Type_, size_t Size_>
struct Point : enoki::StaticArrayImpl<Type_, Size_, false,
                                      RoundingMode::Default,
                                      Point<Type_, Size_>> {
    using Base = enoki::StaticArrayImpl<Type_, Size_, false,
                                        RoundingMode::Default,
                                        Point<Type_, Size_>>;
    using Base::Base;
    using Base::operator=;

    using Vector = mitsuba::Vector<Type_, Size_>;
};

template <typename Type, size_t Size_>
struct Normal : Vector<Type, Size_> {
    using Base = Vector<Type, Size_>;
    using Base::Base;
    using Base::operator=;

};

//! @}
// =============================================================

/// Complete the set {a} to an orthonormal basis {a, b, c}
template <typename Vector3>
std::pair<Vector3, Vector3> coordinate_system(const Vector3 &n) {
    Assert(all(abs(norm(n) - 1) < 1e-5f));

    /* Based on "Building an Orthonormal Basis from a 3D Unit Vector Without
       Normalization" by Jeppe Revall Frisvad */
    auto c0 = rcp(1.0f + n.z());
    auto c1 = -n.x() * n.y() * c0;

    Vector3 b(1.0f - n.x() * n.x() * c0, c1, -n.x());
    Vector3 c(c1, 1.0f - n.y() * n.y() * c0, -n.y());

    auto valid_mask = n.z() > -1.f + 1e-7f;

    return std::make_pair(
        select(valid_mask, b, Vector3( 0.0f, -1.0f, 0.0f)),
        select(valid_mask, c, Vector3(-1.0f,  0.0f, 0.0f))
    );
}


NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for static & dynamic vectorization
// -----------------------------------------------------------------------

#define MITSUBA_VECTOR_TYPE(Name)                                              \
    NAMESPACE_BEGIN(enoki)                                                     \
    /* Is this type dynamic? */                                                \
    template <typename T, size_t Size> struct is_dynamic_impl<Name<T, Size>> { \
        static constexpr bool value = is_dynamic<T>::value;                    \
    };                                                                         \
                                                                               \
    /* Create a dynamic version of this type on demand */                      \
    template <typename T, size_t Size> struct dynamic_impl<Name<T, Size>> {    \
        using type = Name<dynamic_t<T>, Size>;                                 \
    };                                                                         \
                                                                               \
    /* How many packets are stored in this instance? */                        \
    template <typename T, size_t Size>                                         \
    size_t packets(const Name<T, Size> &v) {                                   \
        return packets(v.x());                                                 \
    }                                                                          \
                                                                               \
    /* What is the size of the dynamic dimension of this instance? */          \
    template <typename T, size_t Size>                                         \
    size_t dynamic_size(const Name<T, Size> &v) {                              \
        return dynamic_size(v.x());                                            \
    }                                                                          \
                                                                               \
    /* Resize the dynamic dimension of this instance */                        \
    template <typename T, size_t Size>                                         \
    void dynamic_resize(Name<T, Size> &v, size_t size) {                       \
        for (size_t i = 0; i < Size; ++i)                                      \
            dynamic_resize(v.coeff(i), size);                                  \
    }                                                                          \
                                                                               \
    template <typename T, size_t Size, size_t... Index>                        \
    auto ref_wrap(Name<T, Size> &v, std::index_sequence<Index...>) {           \
        using T2 = decltype(ref_wrap(v.coeff(0)));                             \
        return Name<T2, Size>{ ref_wrap(v.coeff(Index))... };                  \
    }                                                                          \
                                                                               \
    template <typename T, size_t Size, size_t... Index>                        \
    auto ref_wrap(const Name<T, Size> &v, std::index_sequence<Index...>) {     \
        using T2 = decltype(ref_wrap(v.coeff(0)));                             \
        return Name<T2, Size>{ ref_wrap(v.coeff(Index))... };                  \
    }                                                                          \
                                                                               \
    /* Construct a wrapper that references the data of this instance */        \
    template <typename T, size_t Size> auto ref_wrap(Name<T, Size> &v) {       \
        return ref_wrap(v, std::make_index_sequence<Size>());                  \
    }                                                                          \
                                                                               \
    /* Construct a wrapper that references the data of this instance (const)   \
     */                                                                        \
    template <typename T, size_t Size> auto ref_wrap(const Name<T, Size> &v) { \
        return ref_wrap(v, std::make_index_sequence<Size>());                  \
    }                                                                          \
                                                                               \
    template <typename T, size_t Size, size_t... Index>                        \
    auto packet(Name<T, Size> &v, size_t i, std::index_sequence<Index...>) {   \
        using T2 = decltype(packet(v.coeff(0), i));                            \
        return Name<T2, Size>{ packet(v.coeff(Index), i)... };                 \
    }                                                                          \
                                                                               \
    template <typename T, size_t Size, size_t... Index>                        \
    auto packet(const Name<T, Size> &v, size_t i,                              \
                std::index_sequence<Index...>) {                               \
        using T2 = decltype(packet(v.coeff(0), i));                            \
        return Name<T2, Size>{ packet(v.coeff(Index), i)... };                 \
    }                                                                          \
                                                                               \
    /* Return the i-th packet */                                               \
    template <typename T, size_t Size>                                         \
    auto packet(Name<T, Size> &v, size_t i) {                                  \
        return packet(v, i, std::make_index_sequence<Size>());                 \
    }                                                                          \
                                                                               \
    /* Return the i-th packet (const) */                                       \
    template <typename T, size_t Size>                                         \
    auto packet(const Name<T, Size> &v, size_t i) {                            \
        return packet(v, i, std::make_index_sequence<Size>());                 \
    }                                                                          \
    NAMESPACE_END(enoki)

MITSUBA_VECTOR_TYPE(mitsuba::Vector)
MITSUBA_VECTOR_TYPE(mitsuba::Point)
MITSUBA_VECTOR_TYPE(mitsuba::Normal)

//! @}
// -----------------------------------------------------------------------
