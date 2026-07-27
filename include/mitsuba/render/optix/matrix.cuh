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

    DEVICE Vector3f transposed_prod(const Vector3f &v) const {
        Vector3f result = m[0];
        result *= v.x();
        for (size_t i = 1; i < 3; ++i)
            result = fmaf(v[i], m[i], result);
        return result;
    }
#endif

    Row m[Size];
};

// Import some common Dr.Jit types
using Matrix3f = Matrix<float, 3>;

#ifdef __CUDACC__
/// FMA dot of an affine row with a homogeneous point/vector (w=1 point, w=0 vector).
DEVICE float affine_dot(const float4 &r, const Vector3f &p, float w) {
    return ::fmaf(r.x, p.x(), ::fmaf(r.y, p.y(), ::fmaf(r.z, p.z(), r.w * w)));
}

/// Apply a row-major affine transformation to a point.
DEVICE Vector3f apply_affine_point(const float4 *m, const Vector3f &p) {
    return Vector3f(affine_dot(m[0], p, 1.f),
                    affine_dot(m[1], p, 1.f),
                    affine_dot(m[2], p, 1.f));
}

/// Apply a row-major affine transformation to a vector.
DEVICE Vector3f apply_affine_vector(const float4 *m, const Vector3f &v) {
    return Vector3f(affine_dot(m[0], v, 0.f),
                    affine_dot(m[1], v, 0.f),
                    affine_dot(m[2], v, 0.f));
}

/// Apply a row-major affine transformation to a ray.
DEVICE Ray3f apply_affine_ray(const float4 *m, const Ray3f &ray) {
    return Ray3f(apply_affine_point(m, ray.o), apply_affine_vector(m, ray.d),
                 ray.maxt, ray.time);
}
#endif
} // end namespace optix
