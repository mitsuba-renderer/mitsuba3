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
 * cos_theta(), \ref sin_phi(), etc.).
 */
template <typename Vector3> struct Frame {
    using Scalar  = value_t<Vector3>;
    using Vector2 = mitsuba::Vector<Scalar, 2>;
    using Normal3 = mitsuba::Normal<Scalar, 3>;
    Vector3 s, t;
    Normal3 n;

    /// Construct a new coordinate frame from a single vector
    Frame(const Vector3 &v) : n(v) {
        std::tie(s, t) = coordinate_system(v);
    }

    /// Convert from world coordinates to local coordinates
    Vector3 to_local(const Vector3 &v) const {
        return Vector3(dot(v, s), dot(v, t), dot(v, n));
    }

    /// Convert from local coordinates to world coordinates
    Vector3 to_world(const Vector3 &v) const {
        return s * v.x() + t * v.y() + n * v.z();
    }

    /** \brief Assuming that the given direction is in the local coordinate
     * system, return the cosine of the angle between the normal and v */
    static Scalar cos_theta(const Vector3 &v) { return v.z(); }

    /** \brief Assuming that the given direction is in the local coordinate
     * system, return the squared cosine of the angle between the normal and v */
    static Scalar cos_theta_2(const Vector3 &v) { return v.z() * v.z(); }

    /** \brief Assuming that the given direction is in the local coordinate
     * system, return the sine of the angle between the normal and v */
    static Scalar sin_theta(const Vector3 &v) { return safe_sqrt(sin_theta_2(v)); }

    /** \brief Assuming that the given direction is in the local coordinate
     * system, return the squared sine of the angle between the normal and v */
    static Scalar sin_theta_2(const Vector3 &v) { return Scalar(1) - v.z() * v.z(); }

    /** \brief Assuming that the given direction is in the local coordinate
     * system, return the tangent of the angle between the normal and v */
    static Scalar tan_theta(const Vector3 &v) {
        Scalar temp = Scalar(1) - v.z() * v.z();
        return select(temp <= Scalar(0), Scalar(0),
                      sqrt(temp) / v.z());
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared tangent of the angle between the normal and v */
    static Scalar tan_theta_2(const Vector3 &v) {
        Scalar temp = 1 - v.z() * v.z();
        return select(temp <= Scalar(0), Scalar(0),
                      temp / (v.z() * v.z()));
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the sine of the phi parameter in spherical coordinates */
    static Scalar sin_phi(const Vector3 &v) {
        Scalar sin_theta = Frame::sin_theta(v);
        return select(eq(sin_theta, Scalar(0)),
                      Scalar(1),
                      clamp(v.y() / sin_theta, Scalar(-1), Scalar(1)));
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared sine of the phi parameter in  spherical
    * coordinates */
    static Scalar sin_phi_2(const Vector3 &v) {
        Scalar sin_theta_2 = Frame::sin_theta_2(v);
        return select(eq(sin_theta_2, Scalar(0)),
                      Scalar(1),
                      min((v.y() * v.y()) / sin_theta_2, Scalar(1)));
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the cosine of the phi parameter in spherical coordinates */
    static Float cos_phi(const Vector3 &v) {
        Scalar sin_theta = Frame::sin_theta(v);
        return select(eq(sin_theta, Scalar(0)),
                      Scalar(1),
                      clamp(v.x() / sin_theta, Scalar(-1), Scalar(1)));
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared cosine of the phi parameter in  spherical
    * coordinates */
    static Float cos_phi_2(const Vector3 &v) {
        Scalar sin_theta_2 = Frame::sin_theta_2(v);
        return select(eq(sin_theta_2, Scalar(0)),
                      Scalar(1),
                      min((v.x() * v.x()) / sin_theta_2, Scalar(1)));
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the u and v coordinates of the vector 'v' */
    static Vector2 uv(const Vector3 &v) {
        return Vector2(v.x(), v.y());
    }

    /// Equality test
    bool operator==(const Frame &frame) const {
        return frame.s == s && frame.t == t && frame.n == n;
    }

    /// Inequality test
    bool operator!=(const Frame &frame) const {
        return frame.s != s || frame.t != t || frame.n != n;
    }

    ENOKI_STRUCT(Frame, s, t, n)
    ENOKI_ALIGNED_OPERATOR_NEW()
};

/// Return a string representation of a frame
template <typename Vector3>
std::ostream &operator<<(std::ostream &os, const Frame<Vector3> &f) {
    os << "Frame[" << std::endl
       << "  s = " << string::indent(f.s, 6) << "," << std::endl
       << "  t = " << string::indent(f.t, 6) << "," << std::endl
       << "  n = " << string::indent(f.n, 6) << std::endl
       << "]";
    return os;
}

NAMESPACE_END(mitsuba)

ENOKI_STRUCT_DYNAMIC(mitsuba::Frame, s, t, n)
