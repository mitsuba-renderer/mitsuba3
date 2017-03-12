#pragma once

#include <mitsuba/core/logger.h>
#include <mitsuba/core/simd.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Elementary vector, point, and normal data types
// =======================================================================

template <typename Type_, size_t Size_>
struct Vector : enoki::StaticArrayImpl<Type_, Size_, enoki::detail::approx_default<Type_>::value,
                                       RoundingMode::Default,
                                       Vector<Type_, Size_>> {
    using Base = enoki::StaticArrayImpl<Type_, Size_, enoki::detail::approx_default<Type_>::value,
                                        RoundingMode::Default,
                                        Vector<Type_, Size_>>;
    template <typename T> using ReplaceType = Vector<T, Size_>;
    using Base::Base;
    using Base::operator=;

    using Point  = mitsuba::Point<Type_, Size_>;
    using Normal = mitsuba::Normal<Type_, Size_>;

    Vector() { }
};


template <typename Type_, size_t Size_>
struct Point : enoki::StaticArrayImpl<Type_, Size_, enoki::detail::approx_default<Type_>::value,
                                      RoundingMode::Default,
                                      Point<Type_, Size_>> {
    using Base = enoki::StaticArrayImpl<Type_, Size_, enoki::detail::approx_default<Type_>::value,
                                        RoundingMode::Default,
                                        Point<Type_, Size_>>;
    template <typename T> using ReplaceType = Point<T, Size_>;
    using Base::Base;
    using Base::operator=;

    using Vector = mitsuba::Vector<Type_, Size_>;
    using Normal = mitsuba::Normal<Type_, Size_>;
};

template <typename Type_, size_t Size_>
struct Normal : enoki::StaticArrayImpl<Type_, Size_, enoki::detail::approx_default<Type_>::value,
                                      RoundingMode::Default,
                                      Normal<Type_, Size_>> {
    using Base = enoki::StaticArrayImpl<Type_, Size_, enoki::detail::approx_default<Type_>::value,
                                        RoundingMode::Default,
                                        Normal<Type_, Size_>>;
    template <typename T> using ReplaceType = Normal<T, Size_>;
    using Base::Base;
    using Base::operator=;

    using Vector = mitsuba::Vector<Type_, Size_>;
    using Point  = mitsuba::Point<Type_, Size_>;
};

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Convenient point/vector type aliases that use the value
//           type underlying another given point or vector type
// =======================================================================

template <typename T> using vector2_t = Vector<value_t<T>, 2>;
template <typename T> using vector3_t = Vector<value_t<T>, 3>;
template <typename T> using point2_t  = Point<value_t<T>, 2>;
template <typename T> using point3_t  = Point<value_t<T>, 3>;
template <typename T> using normal2_t = Normal<value_t<T>, 2>;
template <typename T> using normal3_t = Normal<value_t<T>, 3>;

//! @}
// =======================================================================


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
