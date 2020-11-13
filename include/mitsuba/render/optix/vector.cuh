#pragma once

#include <type_traits>
#include <cmath>
#include <cstdint>

#ifndef __CUDACC__
# include <enoki/array.h>
#endif

namespace optix {
#define DEVICE __forceinline__ __device__

template <typename Value_, size_t Size_> struct Array {
    using Value = Value_;
    static constexpr size_t Size = Size_;
    static constexpr size_t StorageSize = (Size == 3 ? 4 : Size);

#ifndef __CUDACC__
    Array() = default;
    Array(const enoki::Array<Value, Size> &a) { enoki::store(v, a); }
    Array(const mitsuba::Vector<Value, Size> &a) { enoki::store(v, a); }
    Array(const mitsuba::Point<Value, Size> &a) { enoki::store(v, a); }
    Array(const mitsuba::Normal<Value, Size> &a) { enoki::store(v, a); }
#else
    DEVICE Array() { }

    Array(const Array &) = default;

    template <typename T,
              std::enable_if_t<T::Size == Size &&
                               std::is_same<typename T::Value, Value>::value, int> = 0>
    DEVICE Array(const T &a) {
        for (size_t i = 0; i < Size; ++i)
            v[i] = (Value) a[i];
    }

    template <typename T>
    DEVICE Array(const Array<T, Size> &a) {
        for (size_t i = 0; i < Size; ++i)
            v[i] = (Value) a.v[i];
    }

    DEVICE Array(Value s) {
        for (size_t i = 0; i < Size; ++i)
            v[i] = s;
    }

    template <size_t S = Size, std::enable_if_t<S == 2, int> = 0>
    DEVICE Array(Value v0, Value v1) {
        v[0] = v0; v[1] = v1;
    }

    template <size_t S = Size, std::enable_if_t<S == 3, int> = 0>
    DEVICE Array(Value v0, Value v1, Value v2) {
        v[0] = v0; v[1] = v1; v[2] = v2;
    }

    template <size_t S = Size, std::enable_if_t<S == 4, int> = 0>
    DEVICE Array(Value v0, Value v1, Value v2, Value v3) {
        v[0] = v0; v[1] = v1; v[2] = v2; v[3] = v3;
    }

    DEVICE Array operator-() const {
        Array result;
        for (size_t i = 0; i < Size; ++i)
            result[i] = -v[i];
        return result;
    }

    DEVICE friend Array operator+(const Array &a, const Array &b) {
        Array result;
        for (size_t i = 0; i < Size; ++i)
            result[i] = a.v[i] + b.v[i];
        return result;
    }

    DEVICE Array& operator+=(const Array &a) {
        for (size_t i = 0; i < Size; ++i)
            v[i] += a.v[i];
        return *this;
    }

    DEVICE friend Array operator-(const Array &a, const Array &b) {
        Array result;
        for (size_t i = 0; i < Size; ++i)
            result[i] = a.v[i] - b.v[i];
        return result;
    }

    DEVICE Array& operator-=(const Array &a) {
        for (size_t i = 0; i < Size; ++i)
            v[i] -= a.v[i];
        return *this;
    }

    DEVICE friend Array operator*(const Array &a, const Array &b) {
        Array result;
        for (size_t i = 0; i < Size; ++i)
            result[i] = a.v[i] * b.v[i];
        return result;
    }

    DEVICE Array& operator*=(const Array &a) {
        for (size_t i = 0; i < Size; ++i)
            v[i] *= a.v[i];
        return *this;
    }

    DEVICE friend Array operator/(const Array &a, const Array &b) {
        Array result;
        for (size_t i = 0; i < Size; ++i)
            result[i] = a.v[i] / b.v[i];
        return result;
    }

    DEVICE Array& operator/=(const Array &a) {
        for (size_t i = 0; i < Size; ++i)
            v[i] /= a.v[i];
        return *this;
    }

    DEVICE bool operator==(const Array &a) const {
        for (size_t i = 0; i < Size; ++i) {
            if (v[i] != a.v[i])
                return false;
        }
        return true;
    }

    DEVICE bool operator!=(const Array &a) const {
        return !operator==(a);
    }

    DEVICE const Value &operator[](size_t i) const {
        assert(i < Size);
        return v[i];
    }

    DEVICE Value &operator[](size_t i) {
        assert(i < Size);
        return v[i];
    }

    template <size_t S = Size, std::enable_if_t<(S >= 1), int> = 0>
    DEVICE const Value &x() const { return v[0]; }
    template <size_t S = Size, std::enable_if_t<(S >= 1), int> = 0>
    DEVICE Value &x() { return v[0]; }

    template <size_t S = Size, std::enable_if_t<(S >= 2), int> = 0>
    DEVICE const Value &y() const { return v[1]; }
    template <size_t S = Size, std::enable_if_t<(S >= 2), int> = 0>
    DEVICE Value &y() { return v[1]; }

    template <size_t S = Size, std::enable_if_t<(S >= 3), int> = 0>
    DEVICE const Value &z() const { return v[2]; }
    template <size_t S = Size, std::enable_if_t<(S >= 3), int> = 0>
    DEVICE Value &z() { return v[2]; }

    template <size_t S = Size, std::enable_if_t<(S >= 4), int> = 0>
    DEVICE const Value &w() const { return v[3]; }
    template <size_t S = Size, std::enable_if_t<(S >= 4), int> = 0>
    DEVICE Value &w() { return v[3]; }

    /// Debug print
    DEVICE void print() const {
        printf("[");
        for (size_t i = 0; i < Size; i++)
            printf((i < Size - 1 ? "%f, " : "%f"), v[i]);
        printf("]\n");
    }
#endif

    alignas(16) Value v[StorageSize];
};

#ifdef __CUDACC__
template <typename Value, size_t Size>
DEVICE Value dot(const Array<Value, Size> &a1, const Array<Value, Size> &a2) {
    Value result = a1.v[0] * a2.v[0];
    for (size_t i = 1; i < Size; ++i)
        result = ::fmaf(a1.v[i], a2.v[i], result);
    return result;
}

template <typename Value, size_t Size>
DEVICE Value squared_norm(const Array<Value, Size> &a) {
    Value result = a.v[0] * a.v[0];
    for (size_t i = 1; i < Size; ++i)
        result = ::fmaf(a.v[i], a.v[i], result);
    return result;
}

template <typename Value, size_t Size>
DEVICE Value norm(const Array<Value, Size> &a) {
    return std::sqrt(squared_norm(a));
}

template <typename Value, size_t Size>
DEVICE Array<Value, Size> normalize(const Array<Value, Size> &a) {
    return a / norm(a);
}

template <typename Value>
DEVICE Array<Value, 3> cross(const Array<Value, 3> &a, const Array<Value, 3> &b) {
    return Array<Value, 3>(a.y() * b.z() - a.z() * b.y(),
                           a.z() * b.x() - a.x() * b.z(),
                           a.x() * b.y() - a.y() * b.x());
}

template <typename Value, size_t Size>
DEVICE Array<Value, Size> max(const Array<Value, Size> &a1, const Array<Value, Size> &a2) {
    Array<Value, Size> result;
    for (size_t i = 0; i < Size; ++i)
        result.v[i] = std::max(a1.v[i], a2.v[i]);
    return result;
}

template <typename Value, size_t Size>
DEVICE Array<Value, Size> min(const Array<Value, Size> &a1, const Array<Value, Size> &a2) {
    Array<Value, Size> result;
    for (size_t i = 0; i < Size; ++i)
        result.v[i] = std::min(a1.v[i], a2.v[i]);
    return result;
}

template <typename Value, size_t Size>
DEVICE Array<Value, Size> fmaf(const Value &a, const Array<Value, Size> &b, const Array<Value, Size> &c) {
    Array<Value, Size> result;
    for (size_t i = 0; i < Size; ++i)
        result.v[i] = __fmaf_rn(a, b[i], c[i]);
    return result;
}

template <typename Value, size_t Size>
DEVICE Array<Value, Size> frcp(const Array<Value, Size> &a) {
    Array<Value, Size> result;
    for (size_t i = 0; i < Size; ++i)
        result.v[i] = __frcp_rn(a[i]);
    return result;
}
#endif

// Import some common Enoki types
using Vector2f = Array<float, 2>;
using Vector3f = Array<float, 3>;
using Vector4f = Array<float, 4>;
using Vector2i = Array<int32_t, 2>;
using Vector3i = Array<int32_t, 3>;
using Vector4i = Array<int32_t, 4>;
using Vector2u = Array<uint32_t, 2>;
using Vector3u = Array<uint32_t, 3>;
using Vector4u = Array<uint32_t, 4>;

#ifdef __CUDACC__
__forceinline__ __device__ float3 make_float3(const Vector3f& v) {
    return ::make_float3(v.x(), v.y(), v.z());
}
__forceinline__ __device__ Vector3f make_vector3f(const float3& v) {
    return Vector3f(v.x, v.y, v.z);
}

__device__ void coordinate_system(Vector3f n, Vector3f &x, Vector3f &y) {
    /* Based on "Building an Orthonormal Basis, Revisited" by
       Tom Duff, James Burgess, Per Christensen,
       Christophe Hery, Andrew Kensler, Max Liani,
       and Ryusuke Villemin (JCGT Vol 6, No 1, 2017) */

    float s = copysignf(1.f, n.z()),
          a = -1.f / (s + n.z()),
          b = n.x() * n.y() * a;

    x = Vector3f(n.x() * n.x() * a * s + 1.f, b * s, -n.x() * s);
    y = Vector3f(b, s + n.y() * n.y() * a, -n.y());
}
#endif
} // end namespace optix
