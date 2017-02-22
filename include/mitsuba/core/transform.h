#pragma once

#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Bare-bones 4x4 matrix for homogeneous coordinate transformations
 *
 * All operations are efficiently mapped onto Enoki arrays
 */
struct MTS_EXPORT_CORE Matrix4f : Array<Array<Float, 4>, 4> {
    using Base = Array<Array<Float, 4>, 4>;
    using Base::Base;
    using Base::operator=;

    Matrix4f() { }
    Matrix4f(const Array<Array<Float, 4>, 4> &x) : Base(x) { }

    /// Matrix-matrix multiplication
    Matrix4f operator*(const Matrix4f &m_) const {
        Matrix4f result, m = enoki::transpose(m_);
        for (int i = 0; i < 4; ++i)
            result.coeff(i) = enoki::mvprod(m, coeff(i));
        return result;
    }

    static Matrix4f identity();
    Matrix4f inverse() const;
};


struct MTS_EXPORT_CORE Transform {
    Matrix4f m_value, m_inverse;

    Transform()
        : m_value(Matrix4f::identity()), m_inverse(Matrix4f::identity()) {}

    Transform(Matrix4f value) : m_value(value), m_inverse(value.inverse()) {}

    Transform(Matrix4f value, Matrix4f inverse) : m_value(value), m_inverse(inverse) { }

    Transform operator*(const Transform &other) const;

    bool operator==(const Transform &t) const { return m_value == t.m_value && m_inverse == t.m_inverse; }
    bool operator!=(const Transform &t) const { return m_value != t.m_value || m_inverse != t.m_inverse; }

    /// Transform a 3D point
    template <typename Scalar>
    ENOKI_INLINE Point<Scalar, 3> operator*(Point<Scalar, 3> x_) const {
        using Point3 = Point<Scalar, 3>;

        Array<Scalar, 4> x(x_.coeff(0), x_.coeff(1), x_.coeff(2), Scalar(1.f));

        Point3 result(dot(m_value.coeff(0), x),
                      dot(m_value.coeff(1), x),
                      dot(m_value.coeff(2), x));

        Scalar w = dot(m_value.coeff(3), x);

        if (likely(w == 1.f))
            return result;
        else
            return result * rcp<Point3::Approx>(w);
    }

    /// Transform a 3D point
    template <typename Scalar,
              std::enable_if_t<Point<Scalar, 3>::ActualSize == 4, int> = 0>
    ENOKI_INLINE Point<Scalar, 3> bla(Point<Scalar, 3> x) const {
        using Point = Point<Scalar, 3>;
        x.coeff(3) = 1.f;

        Point result = mvprod(m_value, x);

        if (unlikely(result.coeff(3) == 1.f)) {
            printf("%f\n", rcp<true>(result.coeff(3)));
            result = result * rcp<true>(result.coeff(3));
        }

        return result;
    }

};

#if 0

    /// Transform a 3D vector
    template <typename Scalar>
    Vector<Scalar, 3> operator()(Vector<Scalar, 3> x_) const {
        using Vector = Vector<Scalar, 3>;

        Array<Scalar, 4> x(x_.coeff(0), x_.coeff(1), x_.coeff(2), zero<Scalar>());

        return Vector(dot(m_value.coeff(0), x),
                      dot(m_value.coeff(1), x),
                      dot(m_value.coeff(2), x));
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
    Vector<Scalar, 4> operator()(Vector<Scalar, 4> x_) const {
        Array <Scalar, 4> x(x_);
        return Array<Scalar, 4>(dot(coeff(0), x), dot(coeff(1), x),
                                dot(coeff(2), x), dot(coeff(3), x));
    }
#endif

extern MTS_EXPORT_CORE std::ostream &operator<<(std::ostream &os, const Matrix4f &m);

NAMESPACE_END(mitsuba)
