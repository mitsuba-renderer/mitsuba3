#pragma once

#include <mitsuba/core/logger.h>
#include <simdfloat/static.h>

NAMESPACE_BEGIN(mitsuba)

template <typename> struct TNormal;

/* ===================================================================
    This file contains a few templates and specializations, which
    provide 2/3D points, vectors, and normals over different
    underlying data types. Points, vectors, and normals are distinct
    in Nori, because they transform differently under homogeneous
    coordinate transformations.
 * =================================================================== */

template <typename Scalar, int Dimension_>
struct TVector : public simd::StaticFloatBase<Scalar, Dimension_,
                                              std::is_same<Scalar, float>::value,
                                              TVector<Scalar, Dimension_>> {

public:
    enum {
        Dimension = Dimension_
    };

    typedef simd::StaticFloatBase<Scalar, Dimension,
                                std::is_same<Scalar, float>::value,
                                TVector<Scalar, Dimension>> Base;

    typedef TVector<Scalar, Dimension> VectorType;
    typedef TPoint<Scalar, Dimension>  PointType;

    using Base::Base;
    TVector() : Base() { }
    TVector(const Base &f) : Base(f) { }
    TVector(const PointType &f) : Base((const VectorType &) f) { }

    /// Convert to an Eigen vector (definition in transform.h)
    inline operator Eigen::Matrix<Scalar, Dimension, 1, 0, Dimension, 1>() const;
};

template <typename Scalar, int Dimension_>
struct TPoint : public simd::StaticFloatBase<Scalar, Dimension_,
                                             std::is_same<Scalar, float>::value,
                                             TPoint<Scalar, Dimension_>> {

public:
    enum {
        Dimension = Dimension_
    };

    typedef simd::StaticFloatBase<Scalar, Dimension,
                                  std::is_same<Scalar, float>::value,
                                  TPoint<Scalar, Dimension>> Base;

    typedef TVector<Scalar, Dimension> VectorType;
    typedef TPoint<Scalar, Dimension>  PointType;

    using Base::Base;
    TPoint() : Base() { }
    TPoint(const Base &f) : Base(f) { }
    TPoint(const VectorType &f) : Base((const PointType &) f) { }

    /// Convert to an Eigen vector (definition in transform.h)
    inline operator Eigen::Matrix<Scalar, Dimension, 1, 0, Dimension, 1>() const;
};

/// 3-dimensional surface normal representation
template <typename Scalar>
struct TNormal : public TVector<Scalar, 3> {
public:
    using Base = TVector<Scalar, 3>;

    enum {
        Dimension = 3
    };

    using Base::Base;
};

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
            Vector3f(0.0f, -1.0f, 0.0f),
            Vector3f(-1.0f, 0.0f, 0.0f)
        );
    }
}

NAMESPACE_END(mitsuba)
