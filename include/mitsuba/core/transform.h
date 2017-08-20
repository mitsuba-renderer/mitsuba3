#pragma once

#include <mitsuba/core/ray.h>
#include <enoki/transform.h>

NAMESPACE_BEGIN(mitsuba)

using Matrix3f     = enoki::Matrix<Float, 3>;
using Matrix4f     = enoki::Matrix<Float, 4>;
using Quaternion4f = enoki::Quaternion<Float>;

/**
 * \brief Encapsulates a 4x4 homogeneous coordinate transformation and its inverse
 *
 * This class provides a set of overloaded matrix-vector multiplication
 * operators for vectors, points, and normals (all of them transform
 * differently under homogeneous coordinate transformations)
 */
class MTS_EXPORT_CORE Transform {
    using Column = column_t<Matrix4f>;
public:
    /// Create the identity transformation
    Transform()
        : m_value(identity<Matrix4f>()), m_inverse(identity<Matrix4f>()) { }

    /// Initialize the transformation from the given matrix (and compute its inverse)
    Transform(Matrix4f value) : m_value(value), m_inverse(enoki::inverse(value)) { }

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

    /**
     * \brief Transform a given 3D point
     * \remark In the Python API, this method is named \c transform_point
     */
    template <typename Scalar>
    MTS_INLINE Point<Scalar, 3> operator*(const Point<Scalar, 3> &vec) const {
        auto result = m_value.coeff(0) * enoki::fill<Column>(vec.x()) +
                      m_value.coeff(1) * enoki::fill<Column>(vec.y()) +
                      m_value.coeff(2) * enoki::fill<Column>(vec.z()) +
                      m_value.coeff(3);

        Scalar w = result.w();
        if (unlikely(w != 1.f))
            result *= rcp<Point<Scalar, 3>::Approx>(w);

        return head<3>(result);
    }

    /// Transform a 3D point (for affine/non-perspective transformations)
    template <typename Scalar>
    MTS_INLINE Point<Scalar, 3> transform_affine(const Point<Scalar, 3> &vec) const {
        auto result = m_value.coeff(0) * enoki::fill<Column>(vec.x()) +
                      m_value.coeff(1) * enoki::fill<Column>(vec.y()) +
                      m_value.coeff(2) * enoki::fill<Column>(vec.z()) +
                      m_value.coeff(3);

        return head<3>(result);
    }

    /**
     * \brief Transform a given 3D vector
     * \remark In the Python API, this method is named \c transform_vector
     */
    template <typename Scalar>
    MTS_INLINE Vector<Scalar, 3> operator*(const Vector<Scalar, 3> &vec) const {
        auto result = m_value.coeff(0) * enoki::fill<Column>(vec.x()) +
                      m_value.coeff(1) * enoki::fill<Column>(vec.y()) +
                      m_value.coeff(2) * enoki::fill<Column>(vec.z());

        return head<3>(result);
    }

    /// Transform a 3D vector (for affine/non-perspective transformations)
    template <typename Scalar>
    MTS_INLINE Vector<Scalar, 3> transform_affine(const Vector<Scalar, 3> &vec) const {
        /// Identical to operator*(Vector)
        auto result = m_value.coeff(0) * enoki::fill<Column>(vec.x()) +
                      m_value.coeff(1) * enoki::fill<Column>(vec.y()) +
                      m_value.coeff(2) * enoki::fill<Column>(vec.z());

        return head<3>(result);
    }

    /**
     * \brief Transform a given 3D normal vector
     * \remark In the Python API, this method is named \c transform_normal
     */
    template <typename Scalar>
    MTS_INLINE Normal<Scalar, 3> operator*(const Normal<Scalar, 3> &vec) const {
        Matrix4f inv_t = enoki::transpose(m_inverse);

        auto result = inv_t.coeff(0) * enoki::fill<Column>(vec.x()) +
                      inv_t.coeff(1) * enoki::fill<Column>(vec.y()) +
                      inv_t.coeff(2) * enoki::fill<Column>(vec.z());

        return head<3>(result);
    }

    /// Transform a 3D normal (for affine/non-perspective transformations)
    template <typename Scalar>
    MTS_INLINE Normal<Scalar, 3> transform_affine(const Normal<Scalar, 3> &vec) const {
        /// Identical to operator*(Normal)
        Matrix4f inv_t = enoki::transpose(m_inverse);

        auto result = inv_t.coeff(0) * enoki::fill<Column>(vec.x()) +
                      inv_t.coeff(1) * enoki::fill<Column>(vec.y()) +
                      inv_t.coeff(2) * enoki::fill<Column>(vec.z());

        return head<3>(result);
    }

    /// Transform a ray (for affine/non-perspective transformations)
    template <typename Point>
    MTS_INLINE Ray<Point> operator*(const Ray<Point> &ray) const {
        return Ray<Point>(operator*(ray.o),
                          operator*(ray.d),
                          ray.mint, ray.maxt);
    }

    /// Transform a ray (for affine/non-perspective transformations)
    template <typename Point>
    MTS_INLINE Ray<Point> transform_affine(const Ray<Point> &ray) const {
        return Ray<Point>(transform_affine(ray.o),
                          transform_affine(ray.d),
                          ray.mint, ray.maxt);
    }

    /// Create a translation transformation
    static Transform translate(const Vector3f &v) {
        return Transform(enoki::translate<Matrix4f>(v), enoki::translate<Matrix4f>(-v));
    }

    /// Create a scale transformation
    static Transform scale(const Vector3f &v) {
        return Transform(enoki::scale<Matrix4f>(v), enoki::scale<Matrix4f>(rcp(v)));
    }

    /// Create a rotation transformation around an arbitrary axis. The angle is specified in degrees
    static Transform rotate(const Vector3f &axis, Float angle) {
        Matrix4f matrix = enoki::rotate<Matrix4f>(axis, deg_to_rad(angle));
        return Transform(matrix, transpose(matrix));
    }

    /** \brief Create a perspective transformation.
     *   (Maps [near, far] to [0, 1])
     *
     * \param fov Field of view in degrees
     * \param near Near clipping plane
     * \param far  Far clipping plane
     */
    static Transform perspective(Float fov, Float near, Float far);

    /** \brief Create an orthographic transformation, which maps Z to [0,1]
     * and leaves the X and Y coordinates untouched.
     *
     * \param near Near clipping plane
     * \param far  Far clipping plane
     */
    static Transform orthographic(Float near, Float far);

    /** \brief Create a look-at camera transformation
     *
     * \param origin Camera position
     * \param target Target vector
     * \param up     Up vector
     */
    static Transform look_at(const Point3f &origin,
                             const Point3f &target,
                             const Vector3f &up);

    ENOKI_ALIGNED_OPERATOR_NEW()

private:
    Matrix4f m_value, m_inverse;
};

class MTS_EXPORT_CORE AnimatedTransform {
public:
    struct Keyframe {
        Float time;
        Matrix3f scale;
        Quaternion4f quat;
        Vector3f trans;
    };

    AnimatedTransform() {

    }

    /// Append a keyframe to the current animated transform
    void append_keyframe(Float time, const Transform &trafo);

    template <typename Value>
    auto lookup(const Value &time, const mask_t<Value> &active_ = true) const {
        static_assert(std::is_same<scalar_t<Value>, Float>::value, "Expected a 'float'-valued time parameter");

        auto active = disable_mask_if_scalar(active_);

        /* Compute constants describing the layout of the 'Keyframe' data structure */
        constexpr uint32_t stride       = uint32_t(sizeof(Keyframe) / sizeof(Float));
        constexpr uint32_t scale_offset = offsetof(Keyframe, scale) / sizeof(Float);
        constexpr uint32_t quat_offset  = offsetof(Keyframe, quat)  / sizeof(Float);
        constexpr uint32_t trans_offset = offsetof(Keyframe, trans) / sizeof(Float);

        using Index       = uint32_array_t<Value>;
        using Matrix3     = Matrix<Value, 3>;
        using Vector3     = Vector<Value, 3>;
        using Quaternion4 = Quaternion<Value>;

        /* Perhaps the transformation isn't animated */
        if (likely(size() <= 1))
            return Matrix<Value, 4>(m_transform.matrix());

        /* Look up the interval containing 'time' */
        Index idx = math::find_interval(
            (uint32_t) size(),
            [&](Index idx, mask_t<Value> active) {
                return gather<Value>(m_keyframes.data(), idx * stride, active) <= time;
            },
            active);

        Index idx0 = idx * stride, idx1 = idx0 + stride;

        /* Compute the relative time value in [0, 1] */
        Value t0 = gather<Value>(m_keyframes.data(), idx0, active);
        Value t1 = gather<Value>(m_keyframes.data(), idx1, active);
        Value t = min(max((time - t0) / (t1 - t0), 0.f), 1.f);

        /* Interpolate the scale matrix */
        Matrix3 scale0 = gather<Matrix3>((Float *) m_keyframes.data() + scale_offset, idx0, active);
        Matrix3 scale1 = gather<Matrix3>((Float *) m_keyframes.data() + scale_offset, idx1, active);
        Matrix3 scale = scale0 * (1 - t) + scale1 * t;

        /* Interpolate the rotation quaternion */
        Quaternion4 quat0 = gather<Quaternion4>((Float *) m_keyframes.data() + quat_offset, idx0, active);
        Quaternion4 quat1 = gather<Quaternion4>((Float *) m_keyframes.data() + quat_offset, idx1, active);
        Quaternion4 quat = enoki::slerp(quat0, quat1, t);

        /* Interpolate the translation component */
        Vector3 trans0 = gather<Vector3>((Float *) m_keyframes.data() + trans_offset, idx0, active);
        Vector3 trans1 = gather<Vector3>((Float *) m_keyframes.data() + trans_offset, idx1, active);
        Vector3 trans = trans0 * (1 - t) + trans1 * t;

        return transform_compose(scale, quat, trans);
    }

    size_t size() const { return m_keyframes.size(); }

private:
    Transform m_transform;
    std::vector<Keyframe> m_keyframes;
};

extern MTS_EXPORT_CORE std::ostream &operator<<(std::ostream &os, const Transform &t);

NAMESPACE_END(mitsuba)
