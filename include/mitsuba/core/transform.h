#pragma once

#include <mitsuba/core/ray.h>
#include <enoki/matrix.h>

NAMESPACE_BEGIN(mitsuba)

using Matrix4f = enoki::Matrix<Float, 4>;

/// Compute the inverse of a matrix
extern MTS_EXPORT_CORE Matrix4f inv(const Matrix4f &m);

/**
 * \brief Encapsulates a 4x4 homogeneous coordinate transformation and its inverse
 *
 * This class provides a set of overloaded matrix-vector multiplication
 * operators for vectors, points, and normals (all of them transform
 * differently under homogeneous coordinate transformations)
 */
class MTS_EXPORT_CORE Transform {
    using Column = value_t<Matrix4f>;
public:
    /// Create the identity transformation
    Transform()
        : m_value(identity<Matrix4f>()), m_inverse(identity<Matrix4f>()) { }

    /// Initialize the transformation from the given matrix (and compute its inverse)
    Transform(Matrix4f value) : m_value(value), m_inverse(mitsuba::inv(value)) { }

    /// Initialize the transformation from the given matrix and inverse
    Transform(Matrix4f value, Matrix4f inverse) : m_value(value), m_inverse(inverse) { }

    /// Concatenate transformations
    Transform operator*(const Transform &other) const {
        return Transform(m_value * other.m_value, other.m_inverse * m_inverse);
    }

    /// Compute the inverse of this transformation (for free, since it is already known)
    MTS_INLINE Transform inverse() const {
        return Transform(m_inverse, m_value);
    }

    /// Return the underlying 4x4 matrix
    Matrix4f matrix() const { return m_value; }

    /// Return the underlying 4x4 inverse matrix
    Matrix4f inverse_matrix() const { return m_inverse; }

    /// Equality comparison operator
    bool operator==(const Transform &t) const { return m_value == t.m_value && m_inverse == t.m_inverse; }

    /// Inequality comparison operator
    bool operator!=(const Transform &t) const { return m_value != t.m_value || m_inverse != t.m_inverse; }

    /// Transform a 3D point
    template <typename Scalar>
    MTS_INLINE Point<Scalar, 3> operator*(Point<Scalar, 3> vec) const {
        auto result = m_value.coeff(0) * broadcast<Column>(vec.x()) +
                      m_value.coeff(1) * broadcast<Column>(vec.y()) +
                      m_value.coeff(2) * broadcast<Column>(vec.z()) +
                      m_value.coeff(3);

        Scalar w = result.w();
        if (unlikely(w != 1.f))
            result *= rcp<Point<Scalar, 3>::Approx>(w);

        return head<3>(result);
    }

    /// Transform a 3D point (for affine/non-perspective transformations)
    template <typename Scalar>
    MTS_INLINE Point<Scalar, 3> transform_affine(Point<Scalar, 3> vec) const {
        auto result = m_value.coeff(0) * broadcast<Column>(vec.x()) +
                      m_value.coeff(1) * broadcast<Column>(vec.y()) +
                      m_value.coeff(2) * broadcast<Column>(vec.z()) +
                      m_value.coeff(3);

        return head<3>(result);
    }

    /// Transform a 3D vector
    template <typename Scalar>
    MTS_INLINE Vector<Scalar, 3> operator*(Vector<Scalar, 3> vec) const {
        auto result = m_value.coeff(0) * broadcast<Column>(vec.x()) +
                      m_value.coeff(1) * broadcast<Column>(vec.y()) +
                      m_value.coeff(2) * broadcast<Column>(vec.z());

        return head<3>(result);
    }

    /// Transform a 3D vector (for affine/non-perspective transformations)
    template <typename Scalar>
    MTS_INLINE Vector<Scalar, 3> transform_affine(Vector<Scalar, 3> vec) const {
        /// Identical to operator*(Vector)
        auto result = m_value.coeff(0) * broadcast<Column>(vec.x()) +
                      m_value.coeff(1) * broadcast<Column>(vec.y()) +
                      m_value.coeff(2) * broadcast<Column>(vec.z());

        return head<3>(result);
    }

    /// Transform a 3D normal
    template <typename Scalar>
    MTS_INLINE Normal<Scalar, 3> operator*(Normal<Scalar, 3> vec) const {
        Matrix4f inv_t = enoki::transpose(m_inverse);

        auto result = inv_t.coeff(0) * broadcast<Column>(vec.x()) +
                      inv_t.coeff(1) * broadcast<Column>(vec.y()) +
                      inv_t.coeff(2) * broadcast<Column>(vec.z());

        return head<3>(result);
    }

    /// Transform a 3D normal (for affine/non-perspective transformations)
    template <typename Scalar>
    MTS_INLINE Normal<Scalar, 3> transform_affine(Normal<Scalar, 3> vec) const {
        /// Identical to operator*(Normal)
        Matrix4f inv_t = enoki::transpose(m_inverse);

        auto result = inv_t.coeff(0) * broadcast<Column>(vec.x()) +
                      inv_t.coeff(1) * broadcast<Column>(vec.y()) +
                      inv_t.coeff(2) * broadcast<Column>(vec.z());

        return head<3>(result);
    }

    /// Transform a ray (for affine/non-perspective transformations)
    template <typename Point>
    MTS_INLINE Ray<Point> operator*(Ray<Point> ray) const {
        return Ray<Point>(operator*(ray.o),
                          operator*(ray.d),
                          ray.mint, ray.maxt);
    }

    /// Transform a ray (for affine/non-perspective transformations)
    template <typename Point>
    MTS_INLINE Ray<Point> transform_affine(Ray<Point> ray) const {
        return Ray<Point>(transform_affine(ray.o),
                          transform_affine(ray.d),
                          ray.mint, ray.maxt);
    }

    /// Create a translation transformation
    static Transform translate(const Vector3f &v);

    /// Create a rotation transformation around an arbitrary axis. The angle is specified in degrees
    static Transform rotate(const Vector3f &axis, Float angle);

    /// Create a scale transformation
    static Transform scale(const Vector3f &v);

    /** \brief Create a perspective transformation.
     *   (Maps [near, far] to [0, 1])
     *
     * \param fov Field of view in degrees
     * \param clip_near Near clipping plane
     * \param clip_far Far clipping plane
     */
    static Transform perspective(Float fov, Float clip_near, Float clip_far);

    /** \brief Create an orthographic transformation, which maps Z to [0,1]
     * and leaves the X and Y coordinates untouched.
     *
     * \param clip_near Near clipping plane
     * \param clip_far Far clipping plane
     */
    static Transform orthographic(Float clip_near, Float clip_far);

    /** \brief Create a look-at camera transformation
     *
     * \param p Camera position
     * \param t Target vector
     * \param u Up vector
     */
    static Transform look_at(const Point3f &p, const Point3f &t, const Vector3f &u);

    ENOKI_ALIGNED_OPERATOR_NEW()

private:
    Matrix4f m_value, m_inverse;
};

extern MTS_EXPORT_CORE std::ostream &operator<<(std::ostream &os, const Transform &t);

NAMESPACE_END(mitsuba)
