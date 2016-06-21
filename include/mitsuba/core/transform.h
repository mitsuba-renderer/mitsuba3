#pragma once

#if !defined(NDEBUG)
#  define EIGEN_INITIALIZE_MATRICES_BY_NAN 1
#endif

#define EIGEN_DONT_PARALLELIZE 1

#include <mitsuba/core/vector.h>
#include <Eigen/Core>
#include <Eigen/Geometry>

template <typename T, size_t N, int K>
mitsuba::TVector<T, N> operator*(const Eigen::Matrix<T, K, K> &m, const mitsuba::TVector<T, N> &x) {
    static_assert(N == K, "Incompatible size in matrix-vector product!");
    static_assert((Eigen::Matrix<T, K, K>::Flags & Eigen::RowMajorBit) == 0,
                  "Expected column-major matrix!");

    typedef mitsuba::TVector<T, N> Vec;
    Vec accum = Vec::Zero(), col;

    for (int i = 0; i<N; ++i)
        accum += col.load(m.data() + i*N) * Vec(x[i]);

    return accum;
}

template <typename T, size_t N, int K>
mitsuba::TVector<T, N> operator*(const mitsuba::TVector<T, N> &x, const Eigen::Matrix<T, K, K> &m) {
    static_assert(N == K, "Incompatible size in matrix-vector product!");
    static_assert((Eigen::Matrix<T, K, K>::Flags & Eigen::RowMajorBit) == 0,
                  "Expected column-major matrix!");

    typedef mitsuba::TVector<T, N> Vec;
    Vec accum = Vec::Zero(), col;

    for (int i = 0; i<N; ++i)
        accum[i] = dot(col.load(m.data() + i*N), x);

    return accum;
}

template <typename Derived> std::ostream& operator<<(std::ostream &os, const Eigen::MatrixBase<Derived>& m) {
    constexpr bool isColVector = Eigen::MatrixBase<Derived>::ColsAtCompileTime == 1;
    if (isColVector)
        os << m.transpose().format(Eigen::IOFormat(4, isColVector ? Eigen::DontAlignCols : 0, ", ", ";\n", "", "", "[", "]"));
    else
        os << m.format(Eigen::IOFormat(4, isColVector ? Eigen::DontAlignCols : 0, ", ", ";\n", "", "", "[", "]"));
    return os;
}

template <typename Derived> std::ostream& operator<<(std::ostream &os, const Eigen::ArrayBase<Derived>& a) {
    os << a.matrix();
    return os;
}

NAMESPACE_BEGIN(mitsuba)
template <typename Scalar, int Dimension>
inline TVector<Scalar, Dimension>::operator Eigen::Matrix<Scalar, Dimension, 1, 0, Dimension, 1>() const {
    Eigen::Matrix<Scalar, Dimension, 1, 0, Dimension, 1> result;
    store(result.data());
    return result;
}

template <typename Scalar, int Dimension>
inline TPoint<Scalar, Dimension>::operator Eigen::Matrix<Scalar, Dimension, 1, 0, Dimension, 1>() const {
    Eigen::Matrix<Scalar, Dimension, 1, 0, Dimension, 1> result;
    store(result.data());
    return result;
}

template <typename Scalar>
inline TNormal<Scalar>::operator Eigen::Matrix<Scalar, 3, 1, 0, 3, 1>() const {
    Eigen::Matrix<Scalar, 3, 1, 0, 3, 1> result;
    storeu(result.data());
    return result;
}
NAMESPACE_END(mitsuba)
