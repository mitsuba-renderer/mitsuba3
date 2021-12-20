#pragma once

#include <enoki/packet.h>
#include <mitsuba/core/simd.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Elementary vector, point, and normal data types
// =======================================================================

template <typename Value_, size_t Size_>
struct Vector : ek::StaticArrayImpl<Value_, Size_, false, Vector<Value_, Size_>> {
    using Base = ek::StaticArrayImpl<Value_, Size_, false, Vector<Value_, Size_>>;

    /// Helper alias used to implement type promotion rules
    template <typename T> using ReplaceValue = Vector<T, Size_>;

    using ArrayType = Vector;
    using MaskType = ek::Mask<Value_, Size_>;

    using Point  = mitsuba::Point<Value_, Size_>;
    using Normal = mitsuba::Normal<Value_, Size_>;

    ENOKI_ARRAY_IMPORT(Vector, Base)
};

template <typename Value_, size_t Size_>
struct Point : ek::StaticArrayImpl<Value_, Size_, false, Point<Value_, Size_>> {
    using Base = ek::StaticArrayImpl<Value_, Size_, false, Point<Value_, Size_>>;

    /// Helper alias used to implement type promotion rules
    template <typename T> using ReplaceValue = Point<T, Size_>;

    using ArrayType = Point;
    using MaskType = ek::Mask<Value_, Size_>;

    using Vector = mitsuba::Vector<Value_, Size_>;
    using Normal = mitsuba::Normal<Value_, Size_>;

    ENOKI_ARRAY_IMPORT(Point, Base)
};

template <typename Value_, size_t Size_>
struct Normal : ek::StaticArrayImpl<Value_, Size_, false, Normal<Value_, Size_>> {
    using Base = ek::StaticArrayImpl<Value_, Size_, false, Normal<Value_, Size_>>;

    /// Helper alias used to implement type promotion rules
    template <typename T> using ReplaceValue = Normal<T, Size_>;

    using ArrayType = Normal;
    using MaskType = ek::Mask<Value_, Size_>;

    using Vector = mitsuba::Vector<Value_, Size_>;
    using Point  = mitsuba::Point<Value_, Size_>;

    ENOKI_ARRAY_IMPORT(Normal, Base)
};

/// Subtracting two points should always yield a vector
template <typename T1, size_t S1, typename T2, size_t S2>
auto operator-(const Point<T1, S1> &p1, const Point<T2, S2> &p2) {
    return Vector<T1, S1>(p1) - Vector<T2, S2>(p2);
}

/// Subtracting a vector from a point should always yield a point
template <typename T1, size_t S1, typename T2, size_t S2>
auto operator-(const Point<T1, S1> &p1, const Vector<T2, S2> &v2) {
    return p1 - Point<T2, S2>(v2);
}

/// Adding a vector to a point should always yield a point
template <typename T1, size_t S1, typename T2, size_t S2>
auto operator+(const Point<T1, S1> &p1, const Vector<T2, S2> &v2) {
    return p1 + Point<T2, S2>(v2);
}

//! @}
// =======================================================================

// =======================================================================
//! @{ \name Masking support for vector, point, and normal data types
// =======================================================================

template <typename Value_, size_t Size_>
struct Vector<ek::detail::MaskedArray<Value_>, Size_>
    : ek::detail::MaskedArray<Vector<Value_, Size_>> {
    using Base = ek::detail::MaskedArray<Vector<Value_, Size_>>;
    using Base::Base;
    using Base::operator=;
    Vector(const Base &b) : Base(b) { }
};

template <typename Value_, size_t Size_>
struct Point<ek::detail::MaskedArray<Value_>, Size_>
    : ek::detail::MaskedArray<Point<Value_, Size_>> {
    using Base = ek::detail::MaskedArray<Point<Value_, Size_>>;
    using Base::Base;
    using Base::operator=;
    Point(const Base &b) : Base(b) { }
};

template <typename Value_, size_t Size_>
struct Normal<ek::detail::MaskedArray<Value_>, Size_>
    : ek::detail::MaskedArray<Normal<Value_, Size_>> {
    using Base = ek::detail::MaskedArray<Normal<Value_, Size_>>;
    using Base::Base;
    using Base::operator=;
    Normal(const Base &b) : Base(b) { }
};

//! @}
// =======================================================================

/// Complete the set {a} to an orthonormal basis {a, b, c}
template <typename Vector3f> std::pair<Vector3f, Vector3f> coordinate_system(const Vector3f &n) {
    static_assert(Vector3f::Size == 3, "coordinate_system() expects a 3D vector as input!");

    using Float = ek::value_t<Vector3f>;

    /* Based on "Building an Orthonormal Basis, Revisited" by
       Tom Duff, James Burgess, Per Christensen,
       Christophe Hery, Andrew Kensler, Max Liani,
       and Ryusuke Villemin (JCGT Vol 6, No 1, 2017) */

    Float sign = ek::sign(n.z()),
          a    = -ek::rcp(sign + n.z()),
          b    = n.x() * n.y() * a;

    return {
        Vector3f(ek::mulsign(ek::sqr(n.x()) * a, n.z()) + 1,
                 ek::mulsign(b, n.z()),
                 ek::mulsign_neg(n.x(), n.z())),
        Vector3f(b, ek::fmadd(n.y(), n.y() * a, sign), -n.y())
    };
}

NAMESPACE_END(mitsuba)
