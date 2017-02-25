#pragma once

#include <mitsuba/core/ray.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Bare-bones 4x4 matrix for homogeneous coordinate transformations
 * stored in *column-major* form
 *
 * The storage order is intentional, since allows permits an efficient
 * vectorizable mapping of the key operations onto Enoki arrays.
 */
struct MTS_EXPORT_CORE Matrix4f : Array<Vector4f, 4> {
    using Base = Array<Vector4f, 4>;
    using Base::Base;
    using Base::operator=;

    /// Create an unitialized 4x4 matrix
    Matrix4f() { }

    /// Copy constructor
    Matrix4f(const Array<Vector4f, 4> &x) : Base(x) { }

    /// Construct from a set of coefficients
    Matrix4f(Float m00, Float m01, Float m02, Float m03,
             Float m10, Float m11, Float m12, Float m13,
             Float m20, Float m21, Float m22, Float m23,
             Float m30, Float m31, Float m32, Float m33)
        : Base(Vector4f(m00, m10, m20, m30), Vector4f(m01, m11, m21, m31),
               Vector4f(m02, m12, m22, m32), Vector4f(m03, m13, m23, m33)) { }

    /// Matrix-matrix multiplication
    Matrix4f operator*(const Matrix4f &m) const {
        Matrix4f result;
        /* Reduced to 4 multiplications, 12 fused multiply-adds, and
           16 broadcasts (also fused on AVX512VL) */
        for (int j = 0; j< 4; ++j) {
            Vector4f sum = col(0) * m(0, j);
            for (int i = 1; i < 4; ++i)
                sum += col(i) * m(i, j);
            result.col(j) = sum;
        }

        return result;
    }

    /// Compute the inverse of this matrix
    Matrix4f inverse() const;

    /// Return the j-th row
    MTS_INLINE Vector4f &col(size_t j) { return derived().coeff(j); }

    /// Return the j-th row (const)
    MTS_INLINE const Vector4f &col(size_t j) const { return derived().coeff(j); }

    /// Return a reference to the (i, j) element
    MTS_INLINE Float& operator()(size_t i, size_t j) { return col(j).coeff(i); }

    /// Return a reference to the (i, j) element (const)
    MTS_INLINE const Float& operator()(size_t i, size_t j) const { return col(j).coeff(i); }

    /// Return the identity matrix
    static Matrix4f identity();
};


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
        : m_value(Matrix4f::identity()), m_inverse(Matrix4f::identity()) { }

    /// Initialize the transformation from the given matrix (and compute its inverse)
    Transform(Matrix4f value) : m_value(value), m_inverse(value.inverse()) { }

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
        using Point3 = Point<Scalar, 3>;

        auto result = m_value.col(0) * broadcast<Column>(vec.x()) +
                      m_value.col(1) * broadcast<Column>(vec.y()) +
                      m_value.col(2) * broadcast<Column>(vec.z()) +
                      m_value.col(3);

        Scalar w = result.w();
        if (unlikely(w != 1.f))
            result *= rcp<Point3::Approx>(w);

        return truncate_3d<Point3>(result);
    }

    /// Transform a 3D point (for affine/non-perspective transformations)
    template <typename Scalar>
    MTS_INLINE Point<Scalar, 3> transform_affine(Point<Scalar, 3> vec) const {
        using Point3 = Point<Scalar, 3>;

        auto result = m_value.col(0) * broadcast<Column>(vec.x()) +
                      m_value.col(1) * broadcast<Column>(vec.y()) +
                      m_value.col(2) * broadcast<Column>(vec.z()) +
                      m_value.col(3);

        return truncate_3d<Point3>(result);
    }

    /// Transform a 3D vector
    template <typename Scalar>
    MTS_INLINE Vector<Scalar, 3> operator*(Vector<Scalar, 3> vec) const {
        auto result = m_value.col(0) * broadcast<Column>(vec.x()) +
                      m_value.col(1) * broadcast<Column>(vec.y()) +
                      m_value.col(2) * broadcast<Column>(vec.z());

        return truncate_3d<Vector<Scalar, 3>>(result);
    }

    /// Transform a 3D vector (for affine/non-perspective transformations)
    template <typename Scalar>
    MTS_INLINE Vector<Scalar, 3> transform_affine(Vector<Scalar, 3> vec) const {
        /// Identical to operator*(Vector)
        auto result = m_value.col(0) * broadcast<Column>(vec.x()) +
                      m_value.col(1) * broadcast<Column>(vec.y()) +
                      m_value.col(2) * broadcast<Column>(vec.z());

        return truncate_3d<Vector<Scalar, 3>>(result);
    }

    /// Transform a 3D normal
    template <typename Scalar>
    MTS_INLINE Normal<Scalar, 3> operator*(Normal<Scalar, 3> vec) const {
        Matrix4f inv_t = enoki::transpose(m_inverse);

        auto result = inv_t.col(0) * broadcast<Column>(vec.x()) +
                      inv_t.col(1) * broadcast<Column>(vec.y()) +
                      inv_t.col(2) * broadcast<Column>(vec.z());

        return truncate_3d<Normal<Scalar, 3>>(result);
    }

    /// Transform a 3D normal (for affine/non-perspective transformations)
    template <typename Scalar>
    MTS_INLINE Normal<Scalar, 3> transform_affine(Normal<Scalar, 3> vec) const {
        /// Identical to operator*(Normal)
        Matrix4f inv_t = enoki::transpose(m_inverse);

        auto result = inv_t.col(0) * broadcast<Column>(vec.x()) +
                      inv_t.col(1) * broadcast<Column>(vec.y()) +
                      inv_t.col(2) * broadcast<Column>(vec.z());

        return truncate_3d<Normal<Scalar, 3>>(result);
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
     * \param fov Field of view in degrees
     * \param clip_near Near clipping plane
     * \param clip_far Far clipping plane
     */
    static Transform perspective(Float fov, Float clip_near, Float clip_far);

    /** \brief Create an orthographic transformation, which maps Z to [0,1]
     * and leaves the X and Y coordinates untouched.
     * \param clip_near Near clipping plane
     * \param clip_far Far clipping plane
     */
    static Transform orthographic(Float clip_near, Float clip_far);

    /** \brief Create a look-at camera transformation
     * \param p Camera position
     * \param t Target vector
     * \param u Up vector
     */
    static Transform look_at(const Point3f &p, const Point3f &t, const Vector3f &u);

private:
    /// Truncate a 4D vector/point/normal to a 3D vector/point/normal
    template <typename T2, typename T1,
              std::enable_if_t<T1::ActualSize == T2::ActualSize, int> = 0>
    static MTS_INLINE T2 truncate_3d(const T1 &value) {
        return value;
    }

    template <typename T2, typename T1,
              std::enable_if_t<T1::ActualSize != T2::ActualSize, int> = 0>
    static MTS_INLINE T2 truncate_3d(const T1 &value) {
        return { value.x(), value.y(), value.z() };
    }

private:
    Matrix4f m_value, m_inverse;
};

extern MTS_EXPORT_CORE std::ostream &operator<<(std::ostream &os, const Transform &t);

NAMESPACE_END(mitsuba)
