#pragma once

#include <mitsuba/core/logger.h>
#include <mitsuba/core/simd.h>

NAMESPACE_BEGIN(mitsuba)

// =============================================================
//! @{ \name Elementary vector, point, and normal data types
// =============================================================

template <typename Type_, size_t Size_>
struct TVector : enoki::StaticArrayImpl<Type_, Size_, false,
                                        RoundingMode::Default,
                                        TVector<Type_, Size_>> {
    using Base = enoki::StaticArrayImpl<Type_, Size_, false,
                                        RoundingMode::Default,
                                        TVector<Type_, Size_>>;
    using Base::Base;
    using Base::operator=;

    using Vector = TVector<Type_, Size_>;
    using Point = TPoint<Type_, Size_>;

    TVector() { }
};


template <typename Type_, size_t Size_>
struct TPoint : enoki::StaticArrayImpl<Type_, Size_, false,
                                       RoundingMode::Default,
                                       TPoint<Type_, Size_>> {
    using Base = enoki::StaticArrayImpl<Type_, Size_, false,
                                        RoundingMode::Default,
                                        TPoint<Type_, Size_>>;
    using Base::Base;
    using Base::operator=;

    using Vector = TVector<Type_, Size_>;
    using Point = TPoint<Type_, Size_>;
};

template <typename Type>
struct TNormal : TVector<Type, 3> {
    using Base = TVector<Type, 3>;
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
