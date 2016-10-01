#pragma once

#include <mitsuba/core/math.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Stores a three-dimensional orthonormal coordinate frame
 *
 * This class is used to convert between different cartesian coordinate systems
 * and to efficiently evaluate trigonometric functions in a spherical
 * coordinate system whose pole is aligned with the \c n axis (e.g. \ref
 * cosTheta(), \ref sinPhi(), etc.).
 *
 * TODO: serialization support (serialization_traits template specialization)
 */
struct Frame {
    Vector3f s, t;
    Normal3f n;

    /// Default constructor -- performs no initialization!
    Frame() { }

    /// Given a normal and tangent vectors, construct a new coordinate frame
    Frame(const Vector3f &s, const Vector3f &t, const Normal3f &n)
        : s(s), t(t), n(n) {
    }

    /// Construct a frame from the given orthonormal vectors
    Frame(const Vector3f &x, const Vector3f &y, const Vector3f &z)
        : s(x), t(y), n(z) {
    }

    /// Construct a new coordinate frame from a single vector
    Frame(const Vector3f &v) : n(v) {
        std::tie(s, t) = coordinateSystem(v);
    }

    /// Convert from world coordinates to local coordinates
    Vector3f toLocal(const Vector3f &v) const {
        return Vector3f(
            simd::dot(v, s),
            simd::dot(v, t),
            simd::dot(v, Vector3f(n))
        );
    }

    /// Convert from local coordinates to world coordinates
    Vector3f toWorld(const Vector3f &v) const {
        return s * v.x() + t * v.y() + n * v.z();
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the cosine of the angle between the normal and v */
    static Float cosTheta(const Vector3f &v) {
        return v.z();
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared cosine of the angle between the normal and v */
    static Float cosTheta2(const Vector3f &v) {
        return v.z() * v.z();
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the sine of the angle between the normal and v */
    static Float sinTheta(const Vector3f &v) {
        Float temp = sinTheta2(v);
        if (temp <= 0.0f)
            return 0.0f;
        return std::sqrt(temp);
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared sine of the angle between the normal and v */
    static Float sinTheta2(const Vector3f &v) {
        return 1.0f - v.z() * v.z();
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the tangent of the angle between the normal and v */
    static Float tanTheta(const Vector3f &v) {
        Float temp = 1 - v.z()*v.z();
        if (temp <= 0.0f)
            return 0.0f;
        return std::sqrt(temp) / v.z();
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared tangent of the angle between the normal and v */
    static Float tanTheta2(const Vector3f &v) {
        Float temp = 1 - v.z()*v.z();
        if (temp <= 0.0f)
            return 0.0f;
        return temp / (v.z() * v.z());
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the sine of the phi parameter in spherical coordinates */
    static Float sinPhi(const Vector3f &v) {
        Float sinTheta = Frame::sinTheta(v);
        if (sinTheta == 0.0f)
            return 1.0f;
        return math::clamp(v.y() / sinTheta, (Float) -1.0f, (Float) 1.0f);
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared sine of the phi parameter in  spherical
    * coordinates */
    static Float sinPhi2(const Vector3f &v) {
        return math::clamp(v.y() * v.y() / sinTheta2(v), (Float) 0.0f, (Float) 1.0f);
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the cosine of the phi parameter in spherical coordinates */
    static Float cosPhi(const Vector3f &v) {
        Float sinTheta = Frame::sinTheta(v);
        if (sinTheta == 0.0f)
            return 1.0f;
        return math::clamp(v.x() / sinTheta, (Float) -1.0f, (Float) 1.0f);
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared cosine of the phi parameter in  spherical
    * coordinates */
    static Float cosPhi2(const Vector3f &v) {
        return math::clamp(v.x() * v.x() / sinTheta2(v), (Float) 0.0f, (Float) 1.0f);
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the u and v coordinates of the vector 'v' */
    static Vector2f uv(const Vector3f &v) {
        return Vector2f(v.x(), v.y());
    }

    /// Equality test
    bool operator==(const Frame &frame) const {
        return frame.s == s && frame.t == t && frame.n == n;
    }

    /// Inequality test
    bool operator!=(const Frame &frame) const {
        return !operator==(frame);
    }

    /// Return a string representation of this frame
    std::string toString() const {
        std::ostringstream oss;
        oss << "Frame["
            << "  s = " << s << ","
            << "  t = " << t << ","
            << "  n = " << n
            << "]";
        return oss.str();
    }
};

NAMESPACE_END(mitsuba)
