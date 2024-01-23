#pragma once

#include <mitsuba/render/optix/vector.cuh>
#include <mitsuba/render/optix/ray.cuh>

#ifndef __CUDACC__
# include <drjit/matrix.h>
# include <mitsuba/core/transform.h>
#endif

namespace optix {
template <typename Value_, size_t Size_> struct Matrix {
    using Value = Value_;
    using Row = Array<Value, Size_>;
    static constexpr size_t Size = Size_;

#ifndef __CUDACC__
    Matrix() = default;
    Matrix(const drjit::Matrix<Value, Size> &a) { drjit::store(m, a); }
#else
    DEVICE Matrix() { }

    Matrix(const Matrix &) = default;

    template <typename T>
    DEVICE Matrix(const Matrix<T, Size> &a) {
        for (size_t i = 0; i < Size; ++i)
            m[i] = (Row) a.m[i];
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

    DEVICE const Row &operator[](size_t i) const {
        assert(i < Size);
        return m[i];
    }

    DEVICE Row &operator[](size_t i) {
        assert(i < Size);
        return m[i];
    }

    /// Debug print
    DEVICE void print() const {
        printf("[\n");
        for (size_t j = 0; j < Size; j++) {
            printf("  [");
            for (size_t i = 0; i < Size; i++)
                printf((i < Size - 1 ? "%f, " : "%f"), m[i][j]);
            printf("]\n");
        }
        printf("]\n");
    }
#endif

    Row m[Size];
};

// Import some common Dr.Jit types
using Matrix3f = Matrix<float, 3>;
using Matrix4f = Matrix<float, 4>;

struct Transform4f {
    Matrix4f matrix;
    Matrix4f inverse_transpose;

#ifndef __CUDACC__
    Transform4f() = default;

    template <typename T>
    Transform4f(const mitsuba::Transform<mitsuba::Point<T, 4>> &t)
        : matrix((Matrix4f) t.matrix),
          inverse_transpose((Matrix4f) t.inverse_transpose) {}
#else
    DEVICE Transform4f() { }

    DEVICE Transform4f(float m[12], float inv[12]) {
        for (size_t j = 0; j < 3; ++j)
            for (size_t i = 0; i < 4; ++i)
                matrix[i][j] = m[j*4 + i];
        matrix[3][0] = matrix[3][1] = matrix[3][2] = 0.f;
        matrix[3][3] = 1.f;

        for (size_t j = 0; j < 3; ++j)
            for (size_t i = 0; i < 4; ++i)
                inverse_transpose[j][i] = inv[j*4 + i];
        inverse_transpose[0][3] = inverse_transpose[1][3] = inverse_transpose[2][3] = 0.f;
        inverse_transpose[3][3] = 1.f;
    }

    DEVICE Vector3f transform_point(const Vector3f &p) const {
        Vector4f result;
        Vector4f point = Vector4f(p.x(), p.y(), p.z(), 1);

        for (size_t i = 0; i < 3; ++i)
            result[i] = dot(matrix[i], point);
        return Vector3f(result.x(), result.y(), result.z());
    }

    DEVICE Vector3f transform_normal(const Vector3f &n) const {
        Vector4f result;
        Vector4f vec = Vector4f(n.x(), n.y(), n.z(), 0);

        for (size_t i = 0; i < 3; ++i)
            result[i] = dot(inverse_transpose[i], vec);
        return Vector3f(result.x(), result.y(), result.z());
    }

    DEVICE Vector3f transform_vector(const Vector3f &v) const {
        Vector4f result;
        Vector4f vec = Vector4f(v.x(), v.y(), v.z(), 0);

        for (size_t i = 0; i < 3; ++i)
            result[i] = dot(matrix[i], vec);
        return Vector3f(result.x(), result.y(), result.z());
    }

    DEVICE Ray3f transform_ray(const Ray3f &ray) const {
        return Ray3f(transform_point(ray.o), transform_vector(ray.d),
                     ray.maxt, ray.time);
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
