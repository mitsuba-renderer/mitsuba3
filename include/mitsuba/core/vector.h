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

    ENOKI_DECLARE_CUSTOM_ARRAY(Base, Vector)

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceType = Vector<T, Size_>;

    using Point  = mitsuba::Point<Type_, Size_>;
    using Normal = mitsuba::Normal<Type_, Size_>;
};


template <typename Type_, size_t Size_>
struct Point : enoki::StaticArrayImpl<Type_, Size_, enoki::detail::approx_default<Type_>::value,
                                      RoundingMode::Default,
                                      Point<Type_, Size_>> {

    using Base = enoki::StaticArrayImpl<Type_, Size_, enoki::detail::approx_default<Type_>::value,
                                        RoundingMode::Default,
                                        Point<Type_, Size_>>;

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceType = Point<T, Size_>;

    ENOKI_DECLARE_CUSTOM_ARRAY(Base, Point)

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

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceType = Normal<T, Size_>;

    ENOKI_DECLARE_CUSTOM_ARRAY(Base, Normal)

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
template <typename T> using point2_t  = Point <value_t<T>, 2>;
template <typename T> using point3_t  = Point <value_t<T>, 3>;
template <typename T> using normal2_t = Normal<value_t<T>, 2>;
template <typename T> using normal3_t = Normal<value_t<T>, 3>;

//! @}
// =======================================================================


/// Complete the set {a} to an orthonormal basis {a, b, c}
template <typename Vector3>
std::pair<Vector3, Vector3> coordinate_system(const Vector3 &n) {
    using Value = value_t<Vector3>;
    using Scalar = scalar_t<Vector3>;

    /* Based on "Building an Orthonormal Basis, Revisited" by
       Tom Duff, James Burgess, Per Christensen,
       Christophe Hery, Andrew Kensler, Max Liani,
       and Ryusuke Villemin (JCGT Vol 6, No 1, 2017) */

    Value sign = enoki::sign(n.z());

    Value a = -rcp<Vector3::Approx>(sign + n.z());
    Value b = n.x() * n.y() * a;

    return std::make_pair(
        Vector3(mulsign(n.x() * n.x() * a, n.z()) + Scalar(1),
                mulsign(b, n.z()),
                mulsign(-n.x(), n.z())),
        Vector3(b, sign + n.y() * n.y() * a, -n.y())
    );
}

NAMESPACE_END(mitsuba)
