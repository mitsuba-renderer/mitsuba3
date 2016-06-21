#pragma once

#include <mitsuba/core/math.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Stores a three-dimensional orthonormal coordinate frame
 *
 * This class is mostly used to quickly convert between different
 * cartesian coordinate systems and to efficiently compute certain
 * quantities (e.g. \ref cosTheta(), \ref tanTheta, ..).
 *
 * \ingroup libcore
 * \ingroup libpython
 */
struct Frame {
    Vector3f s, t;
    Normal3f n;

    /// Default constructor -- performs no initialization!
    inline Frame() { }

    /// Given a normal and tangent vectors, construct a new coordinate frame
    inline Frame(const Vector3f &s, const Vector3f &t, const Normal3f &n)
        : s(s), t(t), n(n) {
    }

    /// Construct a frame from the given orthonormal vectors
    inline Frame(const Vector3f &x, const Vector3f &y, const Vector3f &z)
        : s(x), t(y), n(static_cast<Vector3f::Base>(z)) {
    }

    /// Construct a new coordinate frame from a single vector
    inline Frame(const Vector3f &v) : n(static_cast<Vector3f::Base>(v)) {
        // TODO: double-check these .first and .second have not been swapped
        const auto rest = coordinateSystem(v);
        s = rest.first;
        t = rest.second;
    }

    /// Unserialize from a binary data stream
    // TODO: this is not the way to deserialize anymore
    inline Frame(Stream *) {
        Log(EError, "Not implemented: Frame deserialization");
        // s = Vector3f(stream);
        // t = Vector3f(stream);
        // n = Normal3f(stream);
    }

    /// Serialize to a binary data stream
    // TODO: this is not the way to serialize
    inline void serialize(Stream *) const {
        Log(EError, "Not implemented: Frame serialization");
        // s.serialize(stream);
        // t.serialize(stream);
        // n.serialize(stream);
    }

    /// Convert from world coordinates to local coordinates
    inline Vector3f toLocal(const Vector3f &v) const {
        return Vector3f(
            dot(v, s),
            dot(v, t),
            dot(v, n)
        );
    }

    /// Convert from local coordinates to world coordinates
    inline Vector3f toWorld(const Vector3f &v) const {
        return s * v.x() + t * v.y() + n * v.z();
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared cosine of the angle between the normal and v */
    inline static Float cosTheta2(const Vector3f &v) {
        return v.z() * v.z();
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the cosine of the angle between the normal and v */
    inline static Float cosTheta(const Vector3f &v) {
        return v.z();
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the u and v coordinates of the vector 'v' */
    inline static Vector2f uv(const Vector3f &v) {
        return Vector2f(v.x(), v.y());
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared sine of the angle between the normal and v */
    inline static Float sinTheta2(const Vector3f &v) {
        return 1.0f - v.z() * v.z();
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the sine of the angle between the normal and v */
    inline static Float sinTheta(const Vector3f &v) {
        Float temp = sinTheta2(v);
        if (temp <= 0.0f)
            return 0.0f;
        return std::sqrt(temp);
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the tangent of the angle between the normal and v */
    inline static Float tanTheta(const Vector3f &v) {
        Float temp = 1 - v.z()*v.z();
        if (temp <= 0.0f)
            return 0.0f;
        return std::sqrt(temp) / v.z();
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared tangent of the angle between the normal and v */
    inline static Float tanTheta2(const Vector3f &v) {
        Float temp = 1 - v.z()*v.z();
        if (temp <= 0.0f)
            return 0.0f;
        return temp / (v.z() * v.z());
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the sine of the phi parameter in spherical coordinates */
    inline static Float sinPhi(const Vector3f &v) {
        Float sinTheta = Frame::sinTheta(v);
        if (sinTheta == 0.0f)
            return 1.0f;
        return math::clamp(v.y() / sinTheta, (Float) -1.0f, (Float) 1.0f);
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the cosine of the phi parameter in spherical coordinates */
    inline static Float cosPhi(const Vector3f &v) {
        Float sinTheta = Frame::sinTheta(v);
        if (sinTheta == 0.0f)
            return 1.0f;
        return math::clamp(v.x() / sinTheta, (Float) -1.0f, (Float) 1.0f);
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared sine of the phi parameter in  spherical
    * coordinates */
    inline static Float sinPhi2(const Vector3f &v) {
        return math::clamp(v.y() * v.y() / sinTheta2(v), (Float) 0.0f, (Float) 1.0f);
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared cosine of the phi parameter in  spherical
    * coordinates */
    inline static Float cosPhi2(const Vector3f &v) {
        return math::clamp(v.x() * v.x() / sinTheta2(v), (Float) 0.0f, (Float) 1.0f);
    }

    /// Equality test
    inline bool operator==(const Frame &frame) const {
        return frame.s == s && frame.t == t && frame.n == n;
    }

    /// Inequality test
    inline bool operator!=(const Frame &frame) const {
        return !operator==(frame);
    }

    /// Return a string representation of this frame
    inline std::string toString() const {
        std::ostringstream oss;
        oss << "Frame[" << std::endl
            << "  s = " << s << "," << std::endl
            << "  t = " << t << "," << std::endl
            << "  n = " << n << std::endl
            << "]";
        return oss.str();
    }
};

NAMESPACE_END(mitsuba)
