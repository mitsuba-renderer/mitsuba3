#pragma once

#include <mitsuba/core/logger.h>
#include <mitsuba/core/simd.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Elementary vector, point, and normal data types
// =======================================================================

template <typename Value_, size_t Size_>
struct Vector
    : enoki::StaticArrayImpl<Value_, Size_, enoki::array_approx_v<Value_>,
                             RoundingMode::Default, false,
                             Vector<Value_, Size_>> {

    static constexpr bool Approx = enoki::array_approx_v<Value_>;

    using Base = enoki::StaticArrayImpl<Value_, Size_, Approx, RoundingMode::Default, false,
                                        Vector<Value_, Size_>>;

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceValue = Vector<T, Size_>;

    using ArrayType = Vector;
    using MaskType = enoki::Mask<Value_, Size_, Approx, RoundingMode::Default>;

    using Point  = mitsuba::Point<Value_, Size_>;
    using Normal = mitsuba::Normal<Value_, Size_>;

    ENOKI_ARRAY_IMPORT(Base, Vector)
};

template <typename Value_, size_t Size_>
struct Point
    : enoki::StaticArrayImpl<Value_, Size_, enoki::array_approx_v<Value_>,
                             RoundingMode::Default, false, Point<Value_, Size_>> {

    static constexpr bool Approx = enoki::array_approx_v<Value_>;

    using Base = enoki::StaticArrayImpl<Value_, Size_, Approx, RoundingMode::Default,
                                        false, Point<Value_, Size_>>;

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceValue = Point<T, Size_>;

    using ArrayType = Point;
    using MaskType = enoki::Mask<Value_, Size_, Approx, RoundingMode::Default>;

    using Vector = mitsuba::Vector<Value_, Size_>;
    using Normal = mitsuba::Normal<Value_, Size_>;

    ENOKI_ARRAY_IMPORT(Base, Point)
};

template <typename Value_, size_t Size_>
struct Normal
    : enoki::StaticArrayImpl<Value_, Size_, enoki::array_approx_v<Value_>,
                             RoundingMode::Default, false,
                             Normal<Value_, Size_>> {

    static constexpr bool Approx = enoki::array_approx_v<Value_>;

    using Base = enoki::StaticArrayImpl<Value_, Size_, Approx, RoundingMode::Default,
                                        false, Normal<Value_, Size_>>;

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceValue = Normal<T, Size_>;

    using ArrayType = Normal;
    using MaskType = enoki::Mask<Value_, Size_, Approx, RoundingMode::Default>;

    using Vector = mitsuba::Vector<Value_, Size_>;
    using Point  = mitsuba::Point<Value_, Size_>;

    ENOKI_ARRAY_IMPORT(Base, Normal)
};

//! @}
// =======================================================================

/// Subtracting two points should always yield a vector
template <typename T1, size_t S1, typename T2, size_t S2>
auto operator-(const Point<T1, S1> &p1, const Point<T2, S2> &p2) {
    return Vector<T1, S1>(p1) - Vector<T2, S2>(p2);
}

/// Ssubtracting a vector from a point should always yield a point
template <typename T1, size_t S1, typename T2, size_t S2>
auto operator-(const Point<T1, S1> &p1, const Vector<T2, S2> &v2) {
    return p1 - Point<T2, S2>(v2);
}

/// Adding a vector to a point should always yield a point
template <typename T1, size_t S1, typename T2, size_t S2>
auto operator+(const Point<T1, S1> &p1, const Vector<T2, S2> &v2) {
    return p1 + Point<T2, S2>(v2);
}

// =======================================================================
//! @{ \name Masking support for vector, point, and normal data types
// =======================================================================

template <typename Value_, size_t Size_>
struct Vector<enoki::detail::MaskedArray<Value_>, Size_> : enoki::detail::MaskedArray<Vector<Value_, Size_>> {
    using Base = enoki::detail::MaskedArray<Vector<Value_, Size_>>;
    using Base::Base;
    using Base::operator=;
    Vector(const Base &b) : Base(b) { }
};

template <typename Value_, size_t Size_>
struct Point<enoki::detail::MaskedArray<Value_>, Size_> : enoki::detail::MaskedArray<Point<Value_, Size_>> {
    using Base = enoki::detail::MaskedArray<Point<Value_, Size_>>;
    using Base::Base;
    using Base::operator=;
    Point(const Base &b) : Base(b) { }
};

template <typename Value_, size_t Size_>
struct Normal<enoki::detail::MaskedArray<Value_>, Size_> : enoki::detail::MaskedArray<Normal<Value_, Size_>> {
    using Base = enoki::detail::MaskedArray<Normal<Value_, Size_>>;
    using Base::Base;
    using Base::operator=;
    Normal(const Base &b) : Base(b) { }
};

//! @}
// =======================================================================

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
template <typename Vector3,
          std::enable_if_t<!enoki::is_dynamic_v<Vector3>, int> = 0>
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
                mulsign(b, n.z()), mulsign_neg(n.x(), n.z())),
        Vector3(b, sign + n.y() * n.y() * a, -n.y())
    );
}

/// Complete the set {a} to an orthonormal basis {a, b, c}
template <typename Vector3,
          std::enable_if_t<enoki::is_dynamic_v<Vector3>, int> = 0>
std::pair<Vector3, Vector3> coordinate_system(const Vector3 &n) {
    return enoki::vectorize([](auto &&n_) {
        return coordinate_system<Vector3fP>(n_);
    }, n);
}

NAMESPACE_END(mitsuba)
