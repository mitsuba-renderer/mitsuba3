#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

Transform Transform::operator*(const Transform &other) const {
    return Transform{ m_value * other.m_value, other.m_inverse * m_inverse };
}

template Point<Float, 3> Transform::bla(Point<Float, 3>) const;
template Point3f  MTS_EXPORT_CORE Transform::operator*(Point3f) const;
template Point3fP MTS_EXPORT_CORE Transform::operator*(Point3fP) const;

Matrix4f Matrix4f::inverse() const {
    Matrix4f I(*this);

    size_t ipiv[4] = { 0, 1, 2, 3 };
    for (size_t k = 0; k < 4; ++k) {
        Float largest = 0.f;
        size_t piv = 0;

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
        for (size_t j = 0; j < 4; j++)
            I[k][j] *= scale;

        /* Jordan reduction */
        for (size_t i = 0; i < 4; i++) {
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
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 4; ++j)
            out[i][ipiv[j]] = I[i][j];
    }
    return out;
}

Matrix4f Matrix4f::identity() {
    return Matrix4f(
        Vector4f(1.f, 0.f, 0.f, 0.f),
        Vector4f(0.f, 1.f, 0.f, 0.f),
        Vector4f(0.f, 0.f, 1.f, 0.f),
        Vector4f(0.f, 0.f, 0.f, 1.f)
    );
}

std::ostream &operator<<(std::ostream &os, const Matrix4f &m) {
    os << "[" << m.coeff(0) << std::endl
       << " " << m.coeff(1) << std::endl
       << " " << m.coeff(2) << std::endl
       << " " << m.coeff(3) << "]";
    return os;
}

NAMESPACE_END(mitsuba)
