#pragma once

#include <mitsuba/core/logger.h>
#include <simdarray/array.h>

NAMESPACE_BEGIN(mitsuba)

using AlignedAllocator = simd::AlignedAllocator<>;

// =============================================================
//! @{ \name Packet data types for vectorization
// =============================================================

/* Packet data types for vectorization */
constexpr size_t PacketSize = simd::Array<Float>::Size;

using FloatPacket  = simd::Array<Float, PacketSize>;
using UInt64Packet = simd::Array<uint64_t, PacketSize>;
using UInt32Packet = simd::Array<uint32_t, PacketSize>;

//! @}
// =============================================================

// =============================================================
//! @{ \name Elementary vector, point, and normal data types
// =============================================================

template <typename> struct TNormal;

/* ===================================================================
    This file contains a few templates and specializations, which
    provide 2/3D points, vectors, and normals over different
    underlying data types. Points, vectors, and normals are distinct
    in Mitsuba, because they transform differently under homogeneous
    coordinate transformations.
 * =================================================================== */

template <typename Scalar, int Dimension_>
struct TVector : simd::ArrayBase<Scalar, Dimension_, false,
                                 simd::RoundingMode::Default,
                                 TVector<Scalar, Dimension_>> {
    using Base = simd::ArrayBase<Scalar, Dimension_, false,
                                 simd::RoundingMode::Default,
                                 TVector<Scalar, Dimension_>>;
    using Base::Base;

    enum { Dimension = Dimension_ };

    using Vector = TVector<Scalar, Dimension>;
    using Point = TPoint<Scalar, Dimension>;

    TVector() { }

    template <typename Derived>
    explicit TVector(const Eigen::MatrixBase<Derived> &m) : Base(Base::Load(m.derived().data())) { }

    /// Convert to an Eigen vector (definition in transform.h)
    inline operator Eigen::Matrix<Scalar, Dimension, 1, 0, Dimension, 1>() const;
};

template <typename Scalar, int Dimension_>
struct TPoint : simd::ArrayBase<Scalar, Dimension_, false,
                                simd::RoundingMode::Default,
                                TPoint<Scalar, Dimension_>> {
    using Base = simd::ArrayBase<Scalar, Dimension_, false,
                                 simd::RoundingMode::Default,
                                 TPoint<Scalar, Dimension_>>;
    using Base::Base;

    enum { Dimension = Dimension_ };

    using Vector = TVector<Scalar, Dimension>;
    using Point = TPoint<Scalar, Dimension>;

    /// Convert to an Eigen vector (definition in transform.h)
    inline operator Eigen::Matrix<Scalar, Dimension, 1, 0, Dimension, 1>() const;
};

/// 3-dimensional surface normal representation
template <typename Scalar>
struct TNormal : TVector<Scalar, 3> {
    using Base = TVector<Scalar, 3>;
    using Base::Base;
};

//! @}
// =============================================================

/// Complete the set {a} to an orthonormal basis {a, b, c}
inline std::pair<Vector3f, Vector3f> coordinateSystem(const Vector3f &n) {
    Assert(std::abs(norm(n) - 1) < 1e-5f);

    /* Based on "Building an Orthonormal Basis from a 3D Unit Vector Without
       Normalization" by Jeppe Revall Frisvad */
    if (n.z() > Float(-1+1e-7)) {
        const Float a = 1.0f / (1.0f + n.z());
        const Float b = -n.x() * n.y() * a;

        return std::make_pair(
            Vector3f(1.0f - n.x() * n.x() * a, b, -n.x()),
            Vector3f(b, 1.0f - n.y() * n.y() * a, -n.y())
        );
    } else {
        return std::make_pair(
            Vector3f( 0.0f, -1.0f, 0.0f),
            Vector3f(-1.0f,  0.0f, 0.0f)
        );
    }
}

NAMESPACE_END(mitsuba)
