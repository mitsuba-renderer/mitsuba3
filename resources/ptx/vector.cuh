#pragma once

#include <type_traits>
#include <cmath>
#include <cstdint>

#define DEVICE __forceinline__ __device__ 

template <typename Value_, size_t Size_> struct Array {
    using Value = Value_;
    static constexpr size_t Size = Size_;

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

    Value v[Size];
};

template <typename Value, size_t Size>
DEVICE Value dot(const Array<Value, Size> &a1, const Array<Value, Size> &a2) {
    Value result = a1.v[0] * a2.v[0];
    for (size_t i = 1; i < Size; ++i)
        result += a1.v[i] * a2.v[i];
    return result;
}

template <typename Value, size_t Size>
DEVICE Value squared_norm(const Array<Value, Size> &a) {
    Value result = a.v[0] * a.v[0];
    for (size_t i = 1; i < Size; ++i)
        result += a.v[i] * a.v[i];
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
    return Array<Value, 3>(
        a.y()*b.z() - a.z()*b.y(),
        a.z()*b.x() - a.x()*b.z(),
        a.x()*b.y() - a.y()*b.x()
    );
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

// Import some common Enoki types
using Vector2f     = Array<float, 2>;
using Vector3f     = Array<float, 3>;
using Vector4f     = Array<float, 4>;
using Vector2i     = Array<int32_t, 2>;
using Vector3i     = Array<int32_t, 3>;
using Vector4i     = Array<int32_t, 4>;
using Vector2ui    = Array<uint32_t, 2>;
using Vector3ui    = Array<uint32_t, 3>;
using Vector4ui    = Array<uint32_t, 4>;
