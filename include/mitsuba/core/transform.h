#pragma once

#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

struct Matrix4f : Array<Array<float, 4>, 4> {
    using Base = Array<Array<float, 4>, 4>;
    using Base::Base;
    using Base::operator=;

    Matrix4f transpose() const {
        Vector4f r0 = coeff(0), r1 = coeff(1), r2 = coeff(2), r3 = coeff(3);
        enoki::transpose4x4(r0, r1, r2, r3);
        return Matrix4f(r0, r1, r2, r3);
    }

    /// Matrix-matrix multiplication
    Matrix4f operator*(const Matrix4f &m_) {
        Matrix4f result, m = m_.transpose();
        for (int i = 0; i < 4; ++i)
            result.coeff(i) = enoki::dot4x4(m.coeff(0), m.coeff(1), m.coeff(2),
                                            m.coeff(3), coeff(i));
        return result;
    }

    friend std::ostream &operator<<(std::ostream &os, const Matrix4f &m) {
        os << "[" << m.coeff(0) << std::endl
           << " " << m.coeff(1) << std::endl
           << " " << m.coeff(2) << std::endl
           << " " << m.coeff(3) << "]";
        return os;
    }

    static Matrix4f identity() {
        return Matrix4f(
            Vector4f(1.f, 0.f, 0.f, 0.f),
            Vector4f(0.f, 1.f, 0.f, 0.f),
            Vector4f(0.f, 0.f, 1.f, 0.f),
            Vector4f(0.f, 0.f, 0.f, 1.f)
        );
    }
};

NAMESPACE_END(mitsuba)

#if 0

    Matrix4f inverse() const {
        Matrix4f I(*this);

        int ipiv[4] = { 0, 1, 2, 3 };
        for (size_t k = 0; k < 4; ++k) {
            Float largest = 0.f;
            int piv = 0;

            /* Find the largest pivot in the current column */
            for (size_t j = k; j < 4; ++j) {
                Float value = std::abs(I[j][k]);
                if (value > largest) {
                    largest = value;
                    piv = j;
                }
            }

            if (largest == 0.f)
                throw std::runtime_error("Singular matrix!");

            /* Row exchange */
            if (piv != k) {
                std::swap(I[k], I[piv]);
                std::swap(ipiv[k], ipiv[piv]);
            }

            Float scale = 1.f / I[k][k];
            I[k][k] = 1.f;
            for (int j = 0; j < 4; j++)
                I[k][j] *= scale;

            /* Jordan reduction */
            for (int i = 0; i < 4; i++) {
                if (i != k) {
                    Float tmp = I[i][k];
                    I[i][k] = 0;

                    for (int j = 0; j < 4; j++)
                        I[i][j] -= I[k][j] * tmp;
                }
            }
        }

        /* Backward permutation */
        Matrix4f out;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j)
                out[i][ipiv[j]] = I[i][j];
        }
        return out;
    }

    /// Transform a 3D point
    template <typename Scalar>
    TPoint<Scalar, 3> operator()(TPoint<Scalar, 3> x_) const {
        Array<Scalar, 4> x(x_.coeff(0), x_.coeff(1), x_.coeff(2), Scalar(1.f));

        return TPoint<Scalar, 3>(dot(coeff(0), x), dot(coeff(1), x),
                                 dot(coeff(2), x)) *
               rcp<TPoint<Scalar, 3>::Approx>(dot(coeff(3), x));
    }

    /// Transform a 3D vector
    template <typename Scalar>
    TVector<Scalar, 3> operator()(TVector<Scalar, 3> x_) const {
        Array<Scalar, 4> x(x_.coeff(0), x_.coeff(1), x_.coeff(2), zero<Scalar>());

        return TVector<Scalar, 3>(dot(coeff(0), x), dot(coeff(1), x),
                                  dot(coeff(2), x));
    }

    /// Transform a 3D normal
    template <typename Scalar>
    TNormal<Scalar> operator()(TNormal<Scalar> x_) const {
        auto transp = transpose();
        Array<Scalar, 4> x(x_.coeff(0), x_.coeff(1), x_.coeff(2), zero<Scalar>());

        return TNormal<Scalar>(dot(transp.coeff(0), x), dot(transp.coeff(1), x),
                               dot(transp.coeff(2), x));
    }

    /// Transform a 4D vector
    template <typename Scalar>
    TVector<Scalar, 4> operator()(TVector<Scalar, 4> x_) const {
        Array <Scalar, 4> x(x_);
        return Array<Scalar, 4>(dot(coeff(0), x), dot(coeff(1), x),
                                dot(coeff(2), x), dot(coeff(3), x));
    }
#endif
