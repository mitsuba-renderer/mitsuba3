#pragma once

#if defined(__GNUG__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdouble-promotion"
#elif defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdouble-promotion"
#endif

#include <mitsuba/core/ray.h>
#include <drjit/transform.h>
#include <drjit/sphere.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Unified homogeneous coordinate transformation
 *
 * This class represents homogeneous coordinate transformations, i.e.,
 * composable mappings that include rotations, scaling, translations, and
 * perspective transformation. As a special case, the implementation can also be
 * specialized to *affine* (non-perspective) transformations, which imposes a
 * simpler structure that can be exploited to simplify certain operations (e.g.,
 * transformation of points, compsition, initialization from a matrix).
 *
 * The class internally stores the matrix and its inverse transpose. The latter
 * is precomputed so that the class admits efficient transformation of surface
 * normals.
 */
template <typename Point_, bool Affine>
struct Transform {
    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    static constexpr size_t Size = Point_::Size;
    static constexpr bool IsAffine = Affine;

    using Float   = dr::value_t<Point_>;
    using Matrix  = dr::Matrix<Float, Size>;
    using Mask    = dr::mask_t<Float>;
    using Scalar  = dr::scalar_t<Float>;
    using Point   = Point_;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    Matrix matrix            = dr::identity<Matrix>();
    Matrix inverse_transpose = dr::identity<Matrix>();

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Constructors, methods, etc.
    // =============================================================

    /// Initialize the transformation from the given matrix
    Transform(const Matrix &value) {
        matrix = value;
        update(); // Compute the inverse transpose
    }

    /// Initialize the transformation from the given matrix and its inverse
    Transform(const Matrix &matrix, const Matrix &inverse_transpose)
        : matrix(matrix), inverse_transpose(inverse_transpose) { }

    /// Copy constructor with type conversion
    template <typename OtherPoint, bool OtherAffine>
    Transform(const Transform<OtherPoint, OtherAffine> &other)
        : matrix(Matrix(other.matrix)),
          inverse_transpose(Matrix(other.inverse_transpose)) { }

    MI_INLINE Transform inverse() const {
        return Transform(dr::transpose(inverse_transpose),
                         dr::transpose(matrix));
    }

    MI_INLINE Transform transpose() const {
        return Transform(dr::transpose(matrix),
                         dr::transpose(inverse_transpose));
    }

    /// Update the inverse transpose part following a modification to 'matrix'
    Transform update() {
        if constexpr (Affine) {
            using RotMatrix = dr::Matrix<Float, Size - 1>;
            RotMatrix invt_rot = dr::inverse_transpose(RotMatrix(matrix));
            Vector<Float, Size - 1> inv_trans = -dr::transpose(invt_rot) * translation();

            for (size_t i = 0; i < Size - 1; ++i) {
                for (size_t j = 0; j < Size - 1; ++j)
                    inverse_transpose.entry(i, j) = invt_rot.entry(i, j);
                inverse_transpose.entry(Size - 1, i) = inv_trans.entry(i);
            }
        } else {
            inverse_transpose = dr::inverse_transpose(matrix);
        }

        return *this;
    }

    /// Get the translation part of a matrix
    Vector<Float, Size - 1> translation() const {
        Vector<Float, Size - 1> result;
        for (size_t i = 0; i < Size - 1; ++i)
            result.entry(i) = matrix.entry(i, Size - 1);
        return result;
    }

    template<typename OtherPoint, bool OtherAffine>
    Transform& operator=(const Transform<OtherPoint, OtherAffine> &t) {
        matrix = Matrix(t.matrix);
        inverse_transpose = Matrix(t.inverse_transpose);
        return *this;
    }

    /// Equality comparison operator
    bool operator==(const Transform &t) const {
        return dr::all_nested(matrix == t.matrix);
    }

    /// Inequality comparison operator
    bool operator!=(const Transform &t) const {
        return dr::all_nested(matrix != t.matrix);
    }

    /// Create a translation transformation
    static Transform translate(const Vector<Float, Size - 1> &v) {
        return Transform(dr::translate<Matrix>(v),
                         dr::transpose(dr::translate<Matrix>(-v)));
    }

    /// Create a scale transformation
    static Transform scale(const Vector<Float, Size - 1> &v) {
        return Transform(dr::scale<Matrix>(v), dr::scale<Matrix>(dr::rcp(v)));
    }

    /// Create a rotation transformation around an arbitrary axis in 3D. The angle is specified in degrees
    template <size_t N = Size, dr::enable_if_t<N == 4> = 0>
    static Transform rotate(const Vector<Float, Size - 1> &axis, const Float &angle) {
        Matrix matrix = dr::rotate<Matrix>(axis, dr::deg_to_rad(angle));
        return Transform(matrix, matrix);
    }

    /// Create a rotation transformation in 2D. The angle is specified in degrees
    template <size_t N = Size, dr::enable_if_t<N == 3> = 0>
    static Transform rotate(const Float &angle) {
        Matrix matrix = dr::rotate<Matrix>(dr::deg_to_rad(angle));
        return Transform(matrix, matrix);
    }

    /** \brief Create an orthographic transformation, which maps Z to [0,1]
     * and leaves the X and Y coordinates untouched.
     *
     * \param near Near clipping plane
     * \param far  Far clipping plane
     */
    template <size_t N = Size, dr::enable_if_t<N == 4> = 0>
    static Transform orthographic(Float near_, Float far_) {
        return scale({1.f, 1.f, 1.f / (far_ - near_)}) *
               translate({ 0.f, 0.f, -near_ });
    }

    /** \brief Create a look-at camera transformation
     *
     * \param origin Camera position
     * \param target Target vector
     * \param up     Up vector
     */
    template <size_t N = Size, dr::enable_if_t<N == 4> = 0>
    static Transform look_at(const mitsuba::Point<Float, 3> &origin,
                             const mitsuba::Point<Float, 3> &target,
                             const Vector<Float, 3> &up) {
        using Vector1 = dr::Array<Scalar, 1>;
        using Vector3 = Vector<Float, 3>;

        Vector3 dir    = dr::normalize(target - origin);
        Vector3 left   = dr::normalize(dr::cross(up, dir));
        Vector3 new_up = dr::cross(dir, left);

        Vector1 z(0);
        Matrix result = dr::transpose(Matrix(
            dr::concat(left, z),
            dr::concat(new_up, z),
            dr::concat(dir, z),
            dr::concat(origin, Vector1(1))
        ));

        Matrix inverse = dr::transpose(Matrix(
            dr::concat(left, z),
            dr::concat(new_up, z),
            dr::concat(dir, z),
            Vector<Float, 4>(0.f, 0.f, 0.f, 1.f)
        ));

        inverse[3] = dr::transpose(inverse) * dr::concat(-origin, Vector1(1));

        return Transform(result, inverse);
    }

    /// Creates a transformation that converts from the standard basis to 'frame'
    template <typename Value, size_t N = Size, dr::enable_if_t<N == 4> = 0>
    static Transform to_frame(const Frame<Value> &frame) {
        dr::Array<Scalar, 1> z(0);

        Matrix result = dr::transpose(Matrix(
            dr::concat(frame.s, z),
            dr::concat(frame.t, z),
            dr::concat(frame.n, z),
            Vector<Float, 4>(0.f, 0.f, 0.f, 1.f))
        );

        return Transform(result, result);
    }

    /// Creates a transformation that converts from 'frame' to the standard basis
    template <typename Value, size_t N = Size, dr::enable_if_t<N == 4> = 0>
    static Transform from_frame(const Frame<Value> &frame) {
        dr::Array<Scalar, 1> z(0);

        Matrix result = Matrix(
            dr::concat(frame.s, z),
            dr::concat(frame.t, z),
            dr::concat(frame.n, z),
            Vector<Float, 4>(0.f, 0.f, 0.f, 1.f)
        );

        return Transform(result, result);
    }

    /**
     * \brief Test for a scale component in each transform matrix by checking
     * whether <tt>M . M^T == I</tt> (where <tt>M</tt> is the matrix in
     * question and <tt>I</tt> is the identity).
     */
    Mask has_scale() const {
        Mask mask(false);
        for (size_t i = 0; i < Size - 1; ++i) {
            for (size_t j = i; j < Size - 1; ++j) {
                Float sum = 0.f;
                for (size_t k = 0; k < Size - 1; ++k)
                    sum += matrix[i][k] * matrix[j][k];

                mask |= dr::abs(sum - (i == j ? 1.f : 0.f)) > 1e-3f;
            }
        }
        return mask;
    }

    /**
     * \brief Transform a 3D vector
     * \remark In the Python API, this maps to the \c @ operator
     */
    template <typename T, typename Expr = dr::expr_t<Float, T>>
    MI_INLINE Vector<Expr, Size - 1> operator*(const Vector<T, Size - 1> &arg) const {
        Vector<Expr, Size - 1> result;

        for (size_t i = 0; i < Size - 1; ++i)
            result.entry(i) = matrix.entry(i, 0) * arg.entry(0);

        for (size_t j = 1; j < Size - 1; ++j)
            for (size_t i = 0; i < Size - 1; ++i)
                result.entry(i) = dr::fmadd(matrix.entry(i, j), arg.entry(j), result.entry(i));

        return result;
    }

    /**
     * \brief Transform a 3D normal vector
     * \remark In the Python API, one should use the \c @ operator
     */
    template <typename T, typename Expr = dr::expr_t<Float, T>>
    MI_INLINE Normal<Expr, Size - 1> operator*(const Normal<T, Size - 1> &arg) const {
        Normal<Expr, Size - 1> result;

        for (size_t i = 0; i < Size - 1; ++i)
            result.entry(i) = inverse_transpose.entry(i, 0) * arg.entry(0);

        for (size_t j = 1; j < Size - 1; ++j)
            for (size_t i = 0; i < Size - 1; ++i)
                result.entry(i) = dr::fmadd(inverse_transpose.entry(i, j), arg.entry(j), result.entry(i));

        return result;
    }

    /**
     * \brief Transform a 3D point with or without perspective division
     * \remark In the Python API, this maps to the \c @ operator
     */
    template <typename T, typename Expr = dr::expr_t<Float, T>>
    MI_INLINE mitsuba::Point<Expr, Size - 1> operator*(const mitsuba::Point<T, Size - 1> &arg) const {
        if constexpr (Affine) { // Affine case
            mitsuba::Point<Expr, Size - 1> result;

            for (size_t i = 0; i < Size - 1; ++i)
                result.entry(i) = matrix.entry(i, Size - 1);

            for (size_t j = 0; j < Size - 1; ++j)
                for (size_t i = 0; i < Size - 1; ++i)
                    result.entry(i) = dr::fmadd(matrix.entry(i, j), arg.entry(j), result.entry(i));

            return result;
        } else { // General case with perspective division
            dr::Array<Expr, Size> result;

            for (size_t i = 0; i < Size; ++i)
                result.entry(i) = matrix.entry(i, Size - 1);

            for (size_t j = 0; j < Size - 1; ++j)
                for (size_t i = 0; i < Size; ++i)
                    result.entry(i) = dr::fmadd(matrix.entry(i, j), arg.entry(j), result.entry(i));

            return dr::head<Size - 1>(result) / result.entry(Size - 1);
        }
    }

    /// Transform a ray (optimized for affine case)
    template <typename T, typename Spectrum, typename Expr = dr::expr_t<Float, T>,
              typename Result = Ray<mitsuba::Point<Expr, Size - 1>, Spectrum>>
    MI_INLINE Result operator*(const Ray<mitsuba::Point<T, Size - 1>, Spectrum> &ray) const {
        return Result(operator*(ray.o), operator*(ray.d), ray.maxt, ray.time, ray.wavelengths);
    }

    /// Concatenate transformations
    Transform operator*(const Transform &o) const {
        if constexpr (Affine) {
            // Optimized multiplication for affine transforms
            Transform result;

            for (size_t i = 0; i < Size - 1; ++i) {
                for (size_t j = 0; j < Size - 1; ++j) {
                    Float sum = 0.f, sum_it = 0.f;

                    for (size_t k = 0; k < Size - 1; ++k)
                        sum = dr::fmadd(matrix.entry(i, k),
                                        o.matrix.entry(k, j), sum);

                    for (size_t k = 0; k < Size - 1; ++k)
                        sum_it = dr::fmadd(inverse_transpose.entry(i, k),
                                           o.inverse_transpose.entry(k, j), sum_it);

                    result.matrix.entry(i, j) = sum;
                    result.inverse_transpose.entry(i, j) = sum_it;
                }
            }

            // Last row/column
            for (size_t l = 0; l < Size - 1; ++l) {
                Float sum = matrix.entry(l, Size - 1),
                      sum_it = o.inverse_transpose.entry(Size - 1, l);

                for (size_t k = 0; k < Size - 1; ++k)
                    sum = dr::fmadd(matrix.entry(l, k),
                                    o.matrix.entry(k, Size - 1), sum);

                for (size_t k = 0; k < Size - 1; ++k)
                    sum_it = dr::fmadd(inverse_transpose.entry(Size - 1, k),
                                       o.inverse_transpose.entry(k, l), sum_it);

                result.matrix.entry(l, Size - 1) = sum;
                result.inverse_transpose.entry(Size - 1, l) = sum_it;
            }

            return result;
        } else {
            return Transform(matrix * o.matrix,
                             inverse_transpose * o.inverse_transpose);
        }
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
    template <size_t N = Size, dr::enable_if_t<N == 4 && !Affine> = 0>
    static Transform perspective(Float fov, Float near_, Float far_) {
        Float recip = 1.f / (far_ - near_);

        /* Perform a scale so that the field of view is mapped
           to the interval [-1, 1] */
        Float tan = dr::tan(dr::deg_to_rad(fov * .5f)),
              cot = 1.f / tan;

        Matrix trafo = dr::diag(Vector<Float, Size>(cot, cot, far_ * recip, 0.f));
        trafo(2, 3) = -near_ * far_ * recip;
        trafo(3, 2) = 1.f;

        Matrix inv_trafo = dr::diag(Vector<Float, Size>(tan, tan, 0.f, dr::rcp(near_)));
        inv_trafo(2, 3) = 1.f;
        inv_trafo(3, 2) = (near_ - far_) / (far_ * near_);

        return Transform(trafo, dr::transpose(inv_trafo));
    }

    /// Extract a lower-dimensional submatrix (only for affine transforms)
    template <bool A = Affine, dr::enable_if_t<A> = 0>
    auto extract() const {
        Transform<mitsuba::Point<Float, Size - 1>, true> result;

        for (size_t i = 0; i < Size - 2; ++i) {
            for (size_t j = 0; j < Size - 2; ++j) {
                result.matrix.entry(i, j) = matrix.entry(i, j);
                result.inverse_transpose.entry(i, j) =
                    inverse_transpose.entry(i, j);
            }
            result.matrix.entry(i, Size - 2) = matrix.entry(i, Size - 1);
            result.inverse_transpose.entry(i, Size - 2) =
                inverse_transpose.entry(i, Size - 1);
        }

        return result;
    }

    /// Expand to a higher-dimensional (inverse of extract)
    template <bool A = Affine, dr::enable_if_t<A> = 0>
    auto expand() const {
        Transform<mitsuba::Point<Float, Size + 1>, true> result;

        for (size_t i = 0; i < Size - 1; ++i) {
            for (size_t j = 0; j < Size - 1; ++j) {
                result.matrix.entry(j, i) = matrix.entry(j, i);
                result.inverse_transpose.entry(j, i) =
                    inverse_transpose.entry(j, i);
            }
            result.matrix.entry(i, Size) = matrix.entry(i, Size - 1);
            result.inverse_transpose.entry(Size, i) =
                inverse_transpose.entry(Size - 1, i);
        }

        result.matrix.entry(Size, Size) = 1;
        result.inverse_transpose.entry(Size, Size) = 1;

        return result;
    }

    template <typename T>
    [[deprecated("Please use operator*")]]
    auto transform_affine(const T &value) const { return operator*(value); }

    DRJIT_STRUCT(Transform, matrix, inverse_transpose)

    //! @}
    // =============================================================
};

// -----------------------------------------------------------------------

template <typename Point, bool Affine>
std::ostream &operator<<(std::ostream &os, const Transform<Point, Affine> &t) {
    os << t.matrix;
    return os;
}


#if defined(__GNUG__)
#  pragma GCC diagnostic pop
#elif defined(__clang__)
#  pragma clang diagnostic pop
#endif

NAMESPACE_END(mitsuba)
