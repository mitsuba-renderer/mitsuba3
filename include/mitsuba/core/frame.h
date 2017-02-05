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
template <typename Vector> struct TFrame {
    using Scalar = typename Vector::Scalar;
    using Vector2 = TVector<Scalar, 2>;
    using Normal = TNormal<Scalar>;
    Vector s, t;
    Normal n;

    /// Default constructor -- performs no initialization!
    TFrame() { }

    /// Given a normal and tangent vectors, construct a new coordinate frame
    TFrame(const Vector &s, const Vector &t, const Normal &n)
        : s(s), t(t), n(n) { }

    /// Construct a frame from the given orthonormal vectors
    TFrame(const Vector &x, const Vector &y, const Vector &z)
        : s(x), t(y), n(z) { }

    /// Construct a new coordinate frame from a single vector
    TFrame(const Vector &v) : n(v) {
        std::tie(s, t) = coordinate_system(v);
    }

    /// Convert from world coordinates to local coordinates
    Vector to_local(const Vector &v) const {
        return Vector(dot(v, s), dot(v, t), dot(v, n));
    }

    /// Convert from local coordinates to world coordinates
    Vector to_world(const Vector &v) const {
        return s * v.x() + t * v.y() + n * v.z();
    }

    /** \brief Assuming that the given direction is in the local coordinate
     * system, return the cosine of the angle between the normal and v */
    static Scalar cos_theta(const Vector &v) { return v.z(); }

    /** \brief Assuming that the given direction is in the local coordinate
     * system, return the squared cosine of the angle between the normal and v */
    static Scalar cos_theta_2(const Vector &v) { return v.z() * v.z(); }

    /** \brief Assuming that the given direction is in the local coordinate
     * system, return the sine of the angle between the normal and v */
    static Scalar sin_theta(const Vector &v) { return safe_sqrt(sin_theta_2(v)); }

    /** \brief Assuming that the given direction is in the local coordinate
     * system, return the squared sine of the angle between the normal and v */
    static Scalar sin_theta_2(const Vector &v) { return Scalar(1) - v.z() * v.z(); }

    /** \brief Assuming that the given direction is in the local coordinate
     * system, return the tangent of the angle between the normal and v */
    static Scalar tan_theta(const Vector &v) {
        Scalar temp = Scalar(1) - v.z() * v.z();
        return select(temp <= Scalar(0), Scalar(0),
                      sqrt(temp) / v.z());
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared tangent of the angle between the normal and v */
    static Scalar tan_theta_2(const Vector &v) {
        Scalar temp = 1 - v.z() * v.z();
        return select(temp <= Scalar(0), Scalar(0),
                      temp / (v.z() * v.z()));
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the sine of the phi parameter in spherical coordinates */
    static Scalar sin_phi(const Vector &v) {
        Scalar sin_theta = TFrame::sin_theta(v);
        return select(eq(sin_theta, Scalar(0)),
                      Scalar(1),
                      clamp(v.y() / sin_theta, Scalar(-1), Scalar(1)));
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared sine of the phi parameter in  spherical
    * coordinates */
    static Scalar sin_phi_2(const Vector &v) {
        Scalar sin_theta_2 = TFrame::sin_theta_2(v);
        return select(eq(sin_theta_2, Scalar(0)),
                      Scalar(1),
                      min((v.y() * v.y()) / sin_theta_2, Scalar(1)));
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the cosine of the phi parameter in spherical coordinates */
    static Float cos_phi(const Vector &v) {
        Scalar sin_theta = TFrame::sin_theta(v);
        return select(eq(sin_theta, Scalar(0)),
                      Scalar(1),
                      clamp(v.x() / sin_theta, Scalar(-1), Scalar(1)));
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the squared cosine of the phi parameter in  spherical
    * coordinates */
    static Float cos_phi_2(const Vector &v) {
        Scalar sin_theta_2 = TFrame::sin_theta_2(v);
        return select(eq(sin_theta_2, Scalar(0)),
                      Scalar(1),
                      min((v.x() * v.x()) / sin_theta_2, Scalar(1)));
    }

    /** \brief Assuming that the given direction is in the local coordinate
    * system, return the u and v coordinates of the vector 'v' */
    static Vector2 uv(const Vector &v) {
        return Vector2(v.x(), v.y());
    }

    /// Equality test
    bool operator==(const TFrame &frame) const {
        return frame.s == s && frame.t == t && frame.n == n;
    }

    /// Inequality test
    bool operator!=(const TFrame &frame) const {
        return frame.s != s || frame.t != t || frame.n != n;
    }

    /// Return a string representation of this frame
    friend std::ostream &operator<<(std::ostream &os, const TFrame &f) {
        os << "Frame["
           << "  s = " << f.s << "," << std::endl
           << "  t = " << f.t << "," << std::endl
           << "  n = " << f.n
           << "]";
        return os;
    }
};

NAMESPACE_END(mitsuba)
