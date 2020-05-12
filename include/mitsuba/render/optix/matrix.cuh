#pragma once

#include <mitsuba/render/optix/vector.cuh>

#ifndef __CUDACC__
# include <enoki/matrix.h>
# include <mitsuba/core/transform.h>
#endif

namespace optix {
template <typename Value_, size_t Size_> struct Matrix {
    using Value = Value_;
    using Column = Array<Value, Size_>;
    static constexpr size_t Size = Size_;

#ifndef __CUDACC__
    Matrix() = default;
    Matrix(const enoki::Matrix<Value, Size> &a) { enoki::store(m, a); }
#else
    DEVICE Matrix() { }

    Matrix(const Matrix &) = default;

    template <typename T>
    DEVICE Matrix(const Matrix<T, Size> &a) {
        for (size_t i = 0; i < Size; ++i)
            m[i] = (Column) a.m[i];
    }

    DEVICE Matrix operator-() const {
        Matrix result;
        for (size_t i = 0; i < Size; ++i)
            result[i] = -m[i];
        return result;
    }

    DEVICE friend Matrix operator+(const Matrix &a, const Matrix &b) {
        Matrix result;
        for (size_t i = 0; i < Size; ++i)
            result[i] = a.m[i] + b.m[i];
        return result;
    }

    DEVICE Matrix& operator+=(const Matrix &a) {
        for (size_t i = 0; i < Size; ++i)
            m[i] += a.m[i];
        return *this;
    }

    DEVICE friend Matrix operator-(const Matrix &a, const Matrix &b) {
        Matrix result;
        for (size_t i = 0; i < Size; ++i)
            result[i] = a.m[i] - b.m[i];
        return result;
    }

    DEVICE Matrix& operator-=(const Matrix &a) {
        for (size_t i = 0; i < Size; ++i)
            m[i] -= a.m[i];
        return *this;
    }

    DEVICE friend Matrix operator*(const Matrix &a, const Matrix &b) {
        Matrix result;
        for (size_t j = 0; j < Size; ++j) {
            Column sum = a[0] * b[0][j];
            for (size_t i = 1; i < Size; ++i)
                sum = ::fmaf(b[i][j], a[i], sum);
            result[j] = sum;
        }
    }

    DEVICE bool operator==(const Matrix &a) const {
        for (size_t i = 0; i < Size; ++i) {
            if (m[i] != a.m[i])
                return false;
        }
        return true;
    }

    DEVICE bool operator!=(const Matrix &a) const {
        return !operator==(a);
    }

    DEVICE const Column &operator[](size_t i) const {
        assert(i < Size);
        return m[i];
    }

    DEVICE Column &operator[](size_t i) {
        assert(i < Size);
        return m[i];
    }

    /// Debug print
    DEVICE void print() const {
        printf("[\n");
        for (size_t i = 0; i < Size; i++) {
            printf("  [");
            for (size_t j = 0; j < Size; j++)
                printf((j < Size - 1 ? "%f, " : "%f"), m[j][i]);
            printf("]\n");
        }
        printf("]\n");
    }
#endif

    Column m[Size];
};

// Import some common Enoki types
using Matrix3f = Matrix<float, 3>;
using Matrix4f = Matrix<float, 4>;

struct Transform4f {
    Matrix4f matrix;
    Matrix4f inverse_transpose;

#ifndef __CUDACC__
    Transform4f() = default;

    Transform4f(const mitsuba::Transform<mitsuba::Point<float, 4>> &t)
        : matrix(t.matrix), inverse_transpose(t.inverse_transpose) {}
#else
    DEVICE Transform4f() { }

    DEVICE Vector3f transform_point(const Vector3f &p) {
        Vector4f result = matrix[3];
        for (size_t i = 0; i < 3; ++i)
            result = fmaf(p[i], matrix[i], result);
        return Vector3f(result.x(), result.y(), result.z());
    }

    DEVICE Vector3f transform_normal(const Vector3f &n) {
        Vector4f result = inverse_transpose[0];
        result *= n.x();
        for (size_t i = 1; i < 3; ++i)
            result = fmaf(n[i], inverse_transpose[i], result);
        return Vector3f(result.x(), result.y(), result.z());
    }

    DEVICE Vector3f transform_vector(const Vector3f &v) {
        Vector4f result = matrix[0];
        result *= v.x();
        for (size_t i = 1; i < 3; ++i)
            result = fmaf(v[i], matrix[i], result);
        return Vector3f(result.x(), result.y(), result.z());
    }

    /// Debug print
    DEVICE void print() const {
        printf("matrix: ");
        matrix.print();
        printf("inverse_transpose: ");
        inverse_transpose.print();
    }
#endif
};
} // end namespace optix