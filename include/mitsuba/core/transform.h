#pragma once

#include <mitsuba/core/ray.h>
#include <enoki/transform.h>

NAMESPACE_BEGIN(mitsuba)

using Matrix2f     = enoki::Matrix<Float, 2>;
using Matrix3f     = enoki::Matrix<Float, 3>;
using Matrix4f     = enoki::Matrix<Float, 4>;
using Quaternion4f = enoki::Quaternion<Float>;

/**
 * \brief Encapsulates a 4x4 homogeneous coordinate transformation along with
 * its inverse transpose
 *
 * The Transform class provides a set of overloaded matrix-vector
 * multiplication operators for vectors, points, and normals (all of them
 * behave differently under homogeneous coordinate transformations, hence
 * the need to represent them using separate types)
 */
template <typename Value> struct Transform {
    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Matrix4 = Matrix<Value, 4>;
    using Vector3 = Vector<Value, 3>;
    using Vector4 = Vector<Value, 4>;
    using Mask    = mask_t<Value>;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    Matrix4 matrix            = identity<Matrix4>();
    Matrix4 inverse_transpose = identity<Matrix4>();

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Constructors, methods, etc.
    // =============================================================

    /// Initialize the transformation from the given matrix (and compute its inverse transpose)
    Transform(const Matrix4 &value)
        : matrix(value),
          inverse_transpose(enoki::inverse_transpose(value)) { }

    /// Concatenate transformations
    MTS_INLINE Transform operator*(const Transform &other) const {
        return Transform(matrix * other.matrix,
                         inverse_transpose * other.inverse_transpose);
    }

    /// Compute the inverse of this transformation (involves just shuffles, no arithmetic)
    MTS_INLINE Transform inverse() const {
        return Transform(transpose(inverse_transpose), transpose(matrix));
    }

    /// Get the translation part of a matrix
    Vector3 translation() const { return head<3>(matrix.coeff(3)); }

    /// Equality comparison operator
    bool operator==(const Transform &t) const {
        return matrix == t.matrix &&
               inverse_transpose == t.inverse_transpose;
    }

    /// Inequality comparison operator
    bool operator!=(const Transform &t) const {
        return matrix != t.matrix ||
               inverse_transpose != t.inverse_transpose;
    }

    /**
     * \brief Transform a 3D vector/point/normal/ray by a transformation that
     * is known to be an affine 3D transformation (i.e. no perspective)
     */
    template <typename T>
    MTS_INLINE auto transform_affine(const T &input) const {
        return operator*(input);
    }

    /// Transform a 3D point (handles affine/non-perspective transformations only)
    template <typename Value2, typename E = expr_t<Value, Value2>>
    MTS_INLINE auto transform_affine(const Point<Value2, 3> &vec) const {
        Array<E, 4> result = matrix.coeff(3);
        result = fmadd(matrix.coeff(0), vec.x(), result);
        result = fmadd(matrix.coeff(1), vec.y(), result);
        result = fmadd(matrix.coeff(2), vec.z(), result);

        return Point<E, 3>(head<3>(result)); // no-op
    }

    /**
     * \brief Transform a 3D point
     * \remark In the Python API, this method is named \c transform_point
     */
    template <typename Value2, typename E = expr_t<Value, Value2>>
    MTS_INLINE Point<E, 3> operator*(const Point<Value2, 3> &arg) const {
        Array<E, 4> result = matrix.coeff(3);
        result = fmadd(matrix.coeff(0), arg.x(), result);
        result = fmadd(matrix.coeff(1), arg.y(), result);
        result = fmadd(matrix.coeff(2), arg.z(), result);

        auto w = result.w();
        if (unlikely(any_nested(neq(w, 1.f))))
            result *= rcp<Point<Value2, 3>::Approx>(w);

        return head<3>(result); // no-op
    }

    /**
     * \brief Transform a 3D vector
     * \remark In the Python API, this method is named \c transform_vector
     */
    template <typename Value2, typename E = expr_t<Value, Value2>>
    MTS_INLINE Vector<E, 3> operator*(const Vector<Value2, 3> &arg) const {
        Array<E, 4> result = matrix.coeff(0); result *= arg.x();
        result = fmadd(matrix.coeff(1), arg.y(), result);
        result = fmadd(matrix.coeff(2), arg.z(), result);

        return head<3>(result); // no-op
    }
    /**
     * \brief Transform a 3D normal argtor
     * \remark In the Python API, this method is named \c transform_normal
     */
    template <typename Value2, typename E = expr_t<Value, Value2>>
    MTS_INLINE Normal<E, 3> operator*(const Normal<Value2, 3> &arg) const {
        Array<E, 4> result = inverse_transpose.coeff(0); result *= arg.x();
        result = fmadd(inverse_transpose.coeff(1), arg.y(), result);
        result = fmadd(inverse_transpose.coeff(2), arg.z(), result);

        return head<3>(result); // no-op
    }

    /// Transform a ray (for perspective transformations)
    template <typename Value2, typename E = expr_t<Value, Value2>,
              typename Result = Ray<Point<E, 3>>>
    MTS_INLINE Result operator*(const Ray<Point<Value2, 3>> &ray) const {
        return Result(operator*(ray.o), operator*(ray.d), ray.mint, ray.maxt,
                      ray.time, ray.wavelengths);
    }

    /// Transform a ray (for affine/non-perspective transformations)
    template <typename Value2, typename E = expr_t<Value, Value2>>
    MTS_INLINE auto transform_affine(const Ray<Point<Value2, 3>> &ray) const {
        return Ray<Point<E, 3>>(transform_affine(ray.o),
                                transform_affine(ray.d), ray.mint, ray.maxt,
                                ray.time, ray.wavelengths);
    }

    /// Create a translation transformation
    static Transform translate(const Vector3 &v) {
        return Transform(enoki::translate<Matrix4>(v),
                         transpose(enoki::translate<Matrix4>(-v)));
    }

    /// Create a scale transformation
    static Transform scale(const Vector3 &v) {
        return Transform(enoki::scale<Matrix4>(v),
                         // No need to transpose a diagonal matrix.
                         enoki::scale<Matrix4>(rcp(v)));
    }

    /// Create a rotation transformation around an arbitrary axis. The angle is specified in degrees
    static Transform rotate(const Vector3 &axis, const Value &angle) {
        Matrix4 matrix = enoki::rotate<Matrix4>(axis, deg_to_rad(angle));
        return Transform(matrix, transpose(matrix));
    }

    /** \brief Create a perspective transformation.
     *   (Maps [near, far] to [0, 1])
     *
     *  Projects vectors in camera space onto a plane at z=1:
     *
     *  x_proj = x / z
     *  y_proj = y / z
     *  z_proj = (far * (z - near)) / (z * (far-near))
     *
     *  Camera-space depths are not mapped linearly!
     *
     * \param fov Field of view in degrees
     * \param near Near clipping plane
     * \param far  Far clipping plane
     */
    static Transform perspective(const Value &fov, const Value &near,
                                 const Value &far) {
        Value recip = 1.f / (far - near);

        /* Perform a scale so that the field of view is mapped
           to the interval [-1, 1] */
        Value tan = enoki::tan(deg_to_rad(fov * .5f)),
              cot = 1.f / tan;

        Matrix4 trafo = diag<Matrix4>(Vector4(cot, cot, far * recip, 0.0f));
        trafo(2, 3) = -near * far * recip;
        trafo(3, 2) = 1.f;

        Matrix4 inv_trafo = diag<Matrix4>(Vector4(tan, tan, 0.f, rcp(near)));
        inv_trafo(2, 3) = 1.f;
        inv_trafo(3, 2) = (near - far) / (far * near);

        return Transform(trafo, transpose(inv_trafo));
    }

    /** \brief Create an orthographic transformation, which maps Z to [0,1]
     * and leaves the X and Y coordinates untouched.
     *
     * \param near Near clipping plane
     * \param far  Far clipping plane
     */
    static Transform orthographic(const Value &near, const Value &far) {
        return scale(Vector3(1.f, 1.f, 1.f / (far - near))) *
               translate(Vector3(0.f, 0.f, -near));
    }

    /** \brief Create a look-at camera transformation
     *
     * \param origin Camera position
     * \param target Target vector
     * \param up     Up vector
     */
    static Transform look_at(const Point3f &origin,
                             const Point3f &target,
                             const Vector3 &up) {
        Vector3 dir = normalize(target - origin);
        dir = normalize(dir);
        Vector3 left = normalize(cross(up, dir));

        Vector3 new_up = cross(dir, left);

        Matrix4 result = Matrix4::from_cols(
            concat(left, 0.f),
            concat(new_up, 0.f),
            concat(dir, 0.f),
            concat(origin, 1.f)
        );

        Matrix4 inverse = Matrix4::from_rows(
            concat(left, 0.f),
            concat(new_up, 0.f),
            concat(dir, 0.f),
            Vector4(0.f, 0.f, 0.f, 1.f)
        );

        inverse[3] = inverse * concat(-origin, 1.f);

        return Transform(result, transpose(inverse));
    }

    //! @}
    // =============================================================


    // =============================================================
    //! @{ \name Test for transform properties.
    // =============================================================

    /**
     * \brief Test for a scale component in each transform matrix by checking
     * whether <tt>M . M^T == I</tt> (where <tt>M</tt> is the matrix in
     * question and <tt>I</tt> is the identity).
     */
    inline Mask has_scale() const {
        Mask mask(false);
        for (int i = 0; i < 3; ++i) {
            for (int j = i; j < 3; ++j) {
                Value sum(0);
                for (int k = 0; k < 3; ++k)
                    sum += matrix[i][k] * matrix[j][k];

                mask |= enoki::abs(sum - (i == j ? 1.f : 0.f)) > 1e-3f;
            }
        }
        return mask;
    }


    //! @}
    // =============================================================

    ENOKI_STRUCT(Transform, matrix, inverse_transpose)
    ENOKI_ALIGNED_OPERATOR_NEW()
};

/**
 * \brief Encapsulates an animated 4x4 homogeneous coordinate transformation
 *
 * The animation is stored as keyframe animation with linear segments. The
 * implementation performs a polar decomposition of each keyframe into a 3x3
 * scale/shear matrix, a rotation quaternion, and a translation vector. These
 * will all be interpolated independently at eval time.
 */
class MTS_EXPORT_CORE AnimatedTransform : public Object {
public:
    /// Represents a single keyframe in an animated transform
    struct Keyframe {
        /// Time value associated with this keyframe
        Float time;

        /// 3x3 scale/shear matrix
        Matrix3f scale;

        /// Rotation quaternion
        Quaternion4f quat;

        /// 3D translation
        Vector3f trans;

        Keyframe(const Float time, const Matrix3f &scale, const Quaternion4f &quat, const Vector3f &trans)
            : time(time), scale(scale), quat(quat), trans(trans) { }

        bool operator==(const Keyframe &f) const {
            return (time == f.time
                 && scale == f.scale
                 && quat == f.quat
                 && trans == f.trans);
        }
        bool operator!=(const Keyframe &f) const {
            return !(*this == f);
        }

        ENOKI_ALIGNED_OPERATOR_NEW()
    };

    /// Create an empty animated transform
    AnimatedTransform() = default;

    /** Create a constant "animated" transform.
     * The provided transformation will be used as long as no keyframes
     * are specified. However, it will be overwritten as soon as the
     * first keyframe is appended.
     */
    AnimatedTransform(const Transform4f &trafo)
      : m_transform(trafo) { }

    virtual ~AnimatedTransform();

    /// Append a keyframe to the current animated transform
    void append(Float time, const Transform4f &trafo);

    /// Append a keyframe to the current animated transform
    void append(const Keyframe &keyframe);

    /// Look up an interpolated transform at the given time
    Transform4f eval(Float time) const;

    /// Vectorized version of \ref eval()
    Transform4fP eval(FloatP time, MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref eval()
    Transform4f eval(Float time, bool /* unused */) const { return eval(time); }

    /**
     * \brief Return an axis-aligned box bounding the amount of translation
     * throughout the animation sequence
     */
    BoundingBox3f translation_bounds() const;

    /// Determine whether the transformation involves any kind of scaling
    bool has_scale() const;

    /// Return the number of keyframes
    size_t size() const { return m_keyframes.size(); }

    /// Return a Keyframe data structure
    const Keyframe &operator[](size_t i) const { return m_keyframes[i]; }

    /// Equality comparison operator
    bool operator==(const AnimatedTransform &t) const {
        if (m_transform != t.m_transform ||
            m_keyframes.size() != t.m_keyframes.size()) {
            return false;
        }
        for (size_t i = 0; i < m_keyframes.size(); ++i) {
            if (m_keyframes[i] != t.m_keyframes[i])
                return false;
        }
        return true;
    }
    bool operator!=(const AnimatedTransform &t) const {
        return !(*this == t);
    }

    /// Return a human-readable summary of this bitmap
    virtual std::string to_string() const override;

    MTS_DECLARE_CLASS()

protected:
    template <typename Value>
    Transform<Value> eval_impl(Value time, mask_t<Value> active) const;

private:
    Transform4f m_transform;
    std::vector<Keyframe, enoki::aligned_allocator<Keyframe>> m_keyframes;
};

// -----------------------------------------------------------------------
//! @{ \name Printing
// -----------------------------------------------------------------------

template <typename Value>
std::ostream &operator<<(std::ostream &os, const Transform<Value> &t) {
    os << t.matrix;
    return os;
}

std::ostream &operator<<(std::ostream &os, const AnimatedTransform::Keyframe &frame);

std::ostream &operator<<(std::ostream &os, const AnimatedTransform &t);

//! @}
// -----------------------------------------------------------------------

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for dynamic vectorization
// -----------------------------------------------------------------------

ENOKI_STRUCT_DYNAMIC(mitsuba::Transform, matrix, inverse_transpose)

//! @}
// -----------------------------------------------------------------------
