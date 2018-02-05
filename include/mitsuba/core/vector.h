#pragma once

#include <mitsuba/core/logger.h>
#include <mitsuba/core/simd.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Elementary vector, point, and normal data types
// =======================================================================

template <typename Value, size_t Size>
struct Vector : enoki::StaticArrayImpl<Value, Size, enoki::detail::approx_default<Value>::value,
                                       RoundingMode::Default,
                                       Vector<Value, Size>> {

    static constexpr bool Approx = enoki::detail::approx_default<Value>::value;

    using Base = enoki::StaticArrayImpl<Value, Size, Approx, RoundingMode::Default,
                                        Vector<Value, Size>>;

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceType = Vector<T, Size>;

    using MaskType = enoki::Mask<Value, Size, Approx, RoundingMode::Default>;

    using Point  = mitsuba::Point<Value, Size>;
    using Normal = mitsuba::Normal<Value, Size>;

    ENOKI_DECLARE_ARRAY(Base, Vector)
};


template <typename Value, size_t Size>
struct Point : enoki::StaticArrayImpl<Value, Size, enoki::detail::approx_default<Value>::value,
                                      RoundingMode::Default,
                                      Point<Value, Size>> {

    static constexpr bool Approx = enoki::detail::approx_default<Value>::value;

    using Base = enoki::StaticArrayImpl<Value, Size, Approx, RoundingMode::Default,
                                        Point<Value, Size>>;

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceType = Point<T, Size>;

    using MaskType = enoki::Mask<Value, Size, Approx, RoundingMode::Default>;

    using Vector = mitsuba::Vector<Value, Size>;
    using Normal = mitsuba::Normal<Value, Size>;

    ENOKI_DECLARE_ARRAY(Base, Point)
};


template <typename Value, size_t Size>
struct Normal : enoki::StaticArrayImpl<Value, Size, enoki::detail::approx_default<Value>::value,
                                      RoundingMode::Default,
                                      Normal<Value, Size>> {

    static constexpr bool Approx = enoki::detail::approx_default<Value>::value;

    using Base = enoki::StaticArrayImpl<Value, Size, Approx, RoundingMode::Default,
                                        Normal<Value, Size>>;

    /// Helper alias used to transition between vector types (used by enoki::vectorize)
    template <typename T> using ReplaceType = Normal<T, Size>;

    using MaskType = enoki::Mask<Value, Size, Approx, RoundingMode::Default>;

    using Vector = mitsuba::Vector<Value, Size>;
    using Point  = mitsuba::Point<Value, Size>;

    ENOKI_DECLARE_ARRAY(Base, Normal)
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
template <typename Vector3,
          std::enable_if_t<!enoki::is_dynamic_nested<Vector3>::value, int> = 0>
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


/// Complete the set {a} to an orthonormal basis {a, b, c}
template <typename Vector3,
          std::enable_if_t<enoki::is_dynamic_nested<Vector3>::value, int> = 0>
std::pair<Vector3, Vector3> coordinate_system(const Vector3 &n) {
    return enoki::vectorize([](auto &&n_) {
        return coordinate_system<Vector3fP>(n_);
    }, n);
}
NAMESPACE_END(mitsuba)
