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
template <typename VectorN> struct Transform {
    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    static constexpr size_t Size = std::decay_t<VectorN>::Size;

    using Value   = value_t<std::decay_t<VectorN>>;
    using Matrix  = enoki::Matrix<Value, Size>;
    using Mask    = mask_t<Value>;
    using Scalar  = scalar_t<Value>;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    Matrix matrix            = identity<Matrix>();
    Matrix inverse_transpose = identity<Matrix>();

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Constructors, methods, etc.
    // =============================================================

    /// Initialize the transformation from the given matrix (and compute its inverse transpose)
    Transform(const Matrix &value)
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
    Vector<Value, Size - 1> translation() const {
        return head<Size - 1>(matrix.coeff(Size - 1));
    }

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

    /// Transform a point (handles affine/non-perspective transformations only)
    template <typename T, typename Expr = expr_t<Value, T>>
    MTS_INLINE Point<Expr, Size - 1> transform_affine(const Point<T, Size - 1> &arg) const {
        Array<Expr, Size> result = matrix.coeff(Size - 1);

        ENOKI_UNROLL for (size_t i = 0; i < Size - 1; ++i)
            result = fmadd(matrix.coeff(i), arg.coeff(i), result);

        return head<Size - 1>(result); // no-op
    }

    /**
     * \brief Transform a 3D point
     * \remark In the Python API, this method is named \c transform_point
     */
    template <typename T, typename Expr = expr_t<Value, T>>
    MTS_INLINE Point<Expr, Size - 1> operator*(const Point<T, Size - 1> &arg) const {
        Array<Expr, Size> result = matrix.coeff(Size - 1);

        ENOKI_UNROLL for (size_t i = 0; i < Size - 1; ++i)
            result = fmadd(matrix.coeff(i), arg.coeff(i), result);

        Expr w = result.coeff(Size - 1);
        if (unlikely(any_nested(neq(w, 1.f))))
            result *= rcp<Point<T, Size - 1>::Approx>(w);

        return head<Size - 1>(result); // no-op
    }

    /**
     * \brief Transform a 3D vector
     * \remark In the Python API, this method is named \c transform_vector
     */
    template <typename T, typename Expr = expr_t<Value, T>>
    MTS_INLINE Vector<Expr, Size - 1> operator*(const Vector<T, Size - 1> &arg) const {
        Array<Expr, Size> result = matrix.coeff(0);
        result *= arg.x();

        ENOKI_UNROLL for (size_t i = 1; i < Size - 1; ++i)
            result = fmadd(matrix.coeff(i), arg.coeff(i), result);

        return head<Size - 1>(result); // no-op
    }

    /**
     * \brief Transform a 3D normal vector
     * \remark In the Python API, this method is named \c transform_normal
     */
    template <typename T, typename Expr = expr_t<Value, T>>
    MTS_INLINE Normal<Expr, Size - 1> operator*(const Normal<T, Size - 1> &arg) const {
        Array<Expr, Size> result = inverse_transpose.coeff(0);
        result *= arg.x();

        ENOKI_UNROLL for (size_t i = 1; i < Size - 1; ++i)
            result = fmadd(inverse_transpose.coeff(i), arg.coeff(i), result);

        return head<Size - 1>(result); // no-op
    }

    /// Transform a ray (for perspective transformations)
    template <typename T, typename Expr = expr_t<Value, T>,
              typename Result = Ray<Point<Expr, Size - 1>>>
    MTS_INLINE Result operator*(const Ray<Point<T, Size - 1>> &ray) const {
        return Result(operator*(ray.o), operator*(ray.d), ray.mint,
                      ray.maxt, ray.time, ray.wavelengths);
    }

    /// Transform a ray (for affine/non-perspective transformations)
    template <typename T, typename Expr = expr_t<Value, T>,
              typename Result = Ray<Point<Expr, Size - 1>>>
    MTS_INLINE Result transform_affine(const Ray<Point<T, Size - 1>> &ray) const {
        return Result(transform_affine(ray.o), transform_affine(ray.d),
                      ray.mint, ray.maxt, ray.time, ray.wavelengths);
    }

    /// Create a translation transformation
    static Transform translate(const Vector<Value, Size - 1> &v) {
        return Transform(enoki::translate<Matrix>(v),
                         transpose(enoki::translate<Matrix>(-v)));
    }

    /// Create a scale transformation
    static Transform scale(const Vector<Value, Size - 1> &v) {
        return Transform(enoki::scale<Matrix>(v),
                         // No need to transpose a diagonal matrix.
                         enoki::scale<Matrix>(rcp(v)));
    }

    /// Create a rotation transformation around an arbitrary axis in 3D. The angle is specified in degrees
    template <size_t N = Size, std::enable_if_t<N == 4, int> = 0>
    static Transform rotate(const Vector<Value, Size - 1> &axis, const Value &angle) {
        Matrix matrix = enoki::rotate<Matrix>(axis, deg_to_rad(angle));
        return Transform(matrix, matrix);
    }

    /// Create a rotation transformation in 2D. The angle is specified in degrees
    template <size_t N = Size, std::enable_if_t<N == 3, int> = 0>
    static Transform rotate(const Value &angle) {
        Matrix matrix = enoki::rotate<Matrix>(deg_to_rad(angle));
        return Transform(matrix, matrix);
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
    template <size_t N = Size, std::enable_if_t<N == 4, int> = 0>
    static Transform perspective(const Value &fov, const Value &near,
                                 const Value &far) {
        Value recip = 1.f / (far - near);

        /* Perform a scale so that the field of view is mapped
           to the interval [-1, 1] */
        Value tan = enoki::tan(deg_to_rad(fov * .5f)),
              cot = 1.f / tan;

        Matrix trafo = diag<Matrix>(Vector<Value, Size>(cot, cot, far * recip, 0.0f));
        trafo(2, 3) = -near * far * recip;
        trafo(3, 2) = 1.f;

        Matrix inv_trafo = diag<Matrix>(Vector<Value, Size>(tan, tan, 0.f, rcp(near)));
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
    template <size_t N = Size, std::enable_if_t<N == 4, int> = 0>
    static Transform orthographic(const Value &near, const Value &far) {
        return scale({1.f, 1.f, 1.f / (far - near)}) *
               translate({ 0.f, 0.f, -near });
    }

    /** \brief Create a look-at camera transformation
     *
     * \param origin Camera position
     * \param target Target vector
     * \param up     Up vector
     */
    template <size_t N = Size, std::enable_if_t<N == 4, int> = 0>
    static Transform look_at(const Point<Value, 3> &origin,
                             const Point<Value, 3> &target,
                             const Vector<Value, 3> &up) {
        using Vector3 = Vector<Value, 3>;

        Vector3 dir = normalize(target - origin);
        dir = normalize(dir);
        Vector3 left = normalize(cross(up, dir));

        Vector3 new_up = cross(dir, left);

        Matrix result = Matrix::from_cols(
            concat(left, Scalar(0)),
            concat(new_up, Scalar(0)),
            concat(dir, Scalar(0)),
            concat(origin, Scalar(1))
        );

        Matrix inverse = Matrix::from_rows(
            concat(left, Scalar(0)),
            concat(new_up, Scalar(0)),
            concat(dir, Scalar(0)),
            Vector<Value, 4>(0.f, 0.f, 0.f, 1.f)
        );

        inverse[3] = inverse * concat(-origin, Scalar(1));

        return Transform(result, transpose(inverse));
    }

    /// Creates a transformation that converts from the standard basis to 'frame'
    template <typename Vector3, size_t N = Size, std::enable_if_t<N == 4, int> = 0>
    static Transform to_frame(const Frame<Vector3> &frame) {
        Matrix result = Matrix::from_cols(
            concat(frame.s, Scalar(0)),
            concat(frame.t, Scalar(0)),
            concat(frame.n, Scalar(0)),
            Vector<Value, 4>(0.f, 0.f, 0.f, 1.f)
        );

        return Transform(result, result);
    }

    /// Creates a transformation that converts from 'frame' to the standard basis
    template <typename Vector3, size_t N = Size, std::enable_if_t<N == 4, int> = 0>
    static Transform from_frame(const Frame<Vector3> &frame) {
        Matrix result = Matrix::from_rows(
            concat(frame.s, Scalar(0)),
            concat(frame.t, Scalar(0)),
            concat(frame.n, Scalar(0)),
            Vector<Value, 4>(0.f, 0.f, 0.f, 1.f)
        );

        return Transform(result, result);
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
    Mask has_scale() const {
        Mask mask(false);
        for (size_t i = 0; i < Size - 1; ++i) {
            for (size_t j = i; j < Size - 1; ++j) {
                Value sum(0);
                for (size_t k = 0; k < Size - 1; ++k)
                    sum += matrix[i][k] * matrix[j][k];

                mask |= enoki::abs(sum - (i == j ? 1.f : 0.f)) > 1e-3f;
            }
        }
        return mask;
    }

    /// Extract a lower-dimensional submatrix
    template <size_t ExtractedSize = Size - 1,
              typename Result = Transform<Vector<Value, ExtractedSize>>>
    MTS_INLINE Result extract() const {
        Result result;
        for (size_t i = 0; i < ExtractedSize - 1; ++i) {
            for (size_t j = 0; j < ExtractedSize - 1; ++j) {
                result.matrix.coeff(i, j) = matrix.coeff(i, j);
                result.inverse_transpose.coeff(i, j) =
                    inverse_transpose.coeff(i, j);
            }
            result.matrix.coeff(ExtractedSize - 1, i) =
                matrix.coeff(Size - 1, i);
            result.inverse_transpose.coeff(i, ExtractedSize - 1) =
                inverse_transpose.coeff(i, Size - 1);
        }

        result.matrix.coeff(ExtractedSize - 1, ExtractedSize - 1) =
            matrix.coeff(Size - 1, Size - 1);

        result.inverse_transpose.coeff(ExtractedSize - 1, ExtractedSize - 1) =
            inverse_transpose.coeff(Size - 1, Size - 1);

        return result;
    }

    //! @}
    // =============================================================

    ENOKI_STRUCT(Transform, matrix, inverse_transpose)
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
            return (time == f.time && scale == f.scale
                 && quat == f.quat && trans == f.trans);
        }

        bool operator!=(const Keyframe &f) const {
            return !operator==(f);
        }
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
        return !operator==(t);
    }

    /// Return a human-readable summary of this bitmap
    virtual std::string to_string() const override;

    MTS_DECLARE_CLASS()

protected:
    template <typename Value>
    Transform<Vector<Value, 4>> eval_impl(Value time, mask_t<Value> active) const;

private:
    Transform4f m_transform;
    std::vector<Keyframe> m_keyframes;
};

// -----------------------------------------------------------------------
//! @{ \name Printing
// -----------------------------------------------------------------------

template <typename VectorN>
std::ostream &operator<<(std::ostream &os, const Transform<VectorN> &t) {
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

ENOKI_STRUCT_SUPPORT(mitsuba::Transform, matrix, inverse_transpose)

//! @}
// -----------------------------------------------------------------------
