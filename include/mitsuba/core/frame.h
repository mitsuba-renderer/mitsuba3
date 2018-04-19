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
template <typename Vector3_> struct Frame {
    using Vector3 = Vector3_;
    using Value  = value_t<Vector3>;
    using Scalar = scalar_t<Value>;
    using Vector2 = mitsuba::Vector<Value, 2>;
    using Normal3 = mitsuba::Normal<Value, 3>;
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

    /** \brief Give a unit direction, this function returns the cosine of the
     * elevation angle in a reference spherical coordinate system (see the \ref
     * Frame description)
     */
    static Value cos_theta(const Vector3 &v) { return v.z(); }

    /** \brief Give a unit direction, this function returns the square cosine
     * of the elevation angle in a reference spherical coordinate system (see
     * the \ref Frame description)
     */
    static Value cos_theta_2(const Vector3 &v) { return sqr(v.z()); }

    /** \brief Give a unit direction, this function returns the sine
     * of the elevation angle in a reference spherical coordinate system (see
     * the \ref Frame description)
     */
    static Value sin_theta(const Vector3 &v) { return safe_sqrt(sin_theta_2(v)); }

    /** \brief Give a unit direction, this function returns the square sine
     * of the elevation angle in a reference spherical coordinate system (see
     * the \ref Frame description)
     */
    static Value sin_theta_2(const Vector3 &v) { return fnmadd(v.z(), v.z(), Scalar(1)); }

    /** \brief Give a unit direction, this function returns the tangent
     * of the elevation angle in a reference spherical coordinate system (see
     * the \ref Frame description)
     */
    static Value tan_theta(const Vector3 &v) {
        Value temp = fnmadd(v.z(), v.z(), Scalar(1));
        return safe_sqrt(temp) / v.z();
    }

    /** \brief Give a unit direction, this function returns the square tangent
     * of the elevation angle in a reference spherical coordinate system (see
     * the \ref Frame description)
     */
    static Value tan_theta_2(const Vector3 &v) {
        Value temp = fnmadd(v.z(), v.z(), Scalar(1));
        return max(temp, Scalar(0)) / sqr(v.z());
    }

    /** \brief Give a unit direction, this function returns the sine of the
     * azimuth in a reference spherical coordinate system (see the \ref Frame
     * description)
     */
    static Value sin_phi(const Vector3 &v) {
        Value sin_theta_2 = Frame::sin_theta_2(v),
              inv_sin_theta = rsqrt(Frame::sin_theta_2(v));
        return select(abs(sin_theta_2) <= 4 * math::MachineEpsilon, Scalar(0),
                      clamp(v.y() * inv_sin_theta, Scalar(-1), Scalar(1)));
    }

    /** \brief Give a unit direction, this function returns the cosine of the
     * azimuth in a reference spherical coordinate system (see the \ref Frame
     * description)
     */
    static Value cos_phi(const Vector3 &v) {
        Value sin_theta_2 = Frame::sin_theta_2(v),
              inv_sin_theta = rsqrt(Frame::sin_theta_2(v));
        return select(abs(sin_theta_2) <= 4 * math::MachineEpsilon, Scalar(1),
                      clamp(v.x() * inv_sin_theta, Scalar(-1), Scalar(1)));
    }

    /** \brief Give a unit direction, this function returns the sine and cosine
     * of the azimuth in a reference spherical coordinate system (see the \ref
     * Frame description)
     */
    static std::pair<Value, Value> sincos_phi(const Vector3 &v) {
        Value sin_theta_2 = Frame::sin_theta_2(v),
              inv_sin_theta = rsqrt(Frame::sin_theta_2(v));

        Vector2 result = head<2>(v) * inv_sin_theta;

        result = select(abs(sin_theta_2) <= 4 * math::MachineEpsilon,
                        Vector2(Scalar(1), Scalar(0)),
                        clamp(result, Scalar(-1), Scalar(1)));

        return { result.y(), result.x() };
    }

    /** \brief Give a unit direction, this function returns the squared sine of
     * the azimuth in a reference spherical coordinate system (see the \ref
     * Frame description)
     */
    static Value sin_phi_2(const Vector3 &v) {
        Value sin_theta_2 = Frame::sin_theta_2(v);
        return select(abs(sin_theta_2) <= 4 * math::MachineEpsilon, Scalar(0),
                      clamp(sqr(v.y()) / sin_theta_2, Scalar(-1), Scalar(1)));
    }

    /** \brief Give a unit direction, this function returns the squared cosine of
     * the azimuth in a reference spherical coordinate system (see the \ref
     * Frame description)
     */
    static Value cos_phi_2(const Vector3 &v) {
        Value sin_theta_2 = Frame::sin_theta_2(v);
        return select(abs(sin_theta_2) <= 4 * math::MachineEpsilon, Scalar(1),
                      clamp(sqr(v.x()) / sin_theta_2, Scalar(-1), Scalar(1)));
    }

    /** \brief Give a unit direction, this function returns the squared sine
     * and cosine of the azimuth in a reference spherical coordinate system
     * (see the \ref Frame description)
     */
    static std::pair<Value, Value> sincos_phi_2(const Vector3 &v) {
        Value sin_theta_2 = Frame::sin_theta_2(v),
              inv_sin_theta_2 = rcp(sin_theta_2);

        Vector2 result = sqr(head<2>(v)) * inv_sin_theta_2;

        result = select(abs(sin_theta_2) <= 4 * math::MachineEpsilon,
                        Vector2(Scalar(1), Scalar(0)),
                        clamp(result, Scalar(-1), Scalar(1)));

        return { result.y(), result.x() };
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

/**
 * \brief Given a smoothly varying shading normal and a tangent of a shape
 * parameterization, compute a smoothly varying orthonormal frame.
 *
 * \param n
 *    A shading normal at a surface position
 * \param dp_du
 *    Position derivative of the underlying parameterization with respect to
 *    the 'u' coordinate
 * \param frame
 *    Used to return the computed frame
 */
template <typename Vector3>
auto compute_shading_frame(const typename Vector3::Normal &n,
                           const Vector3 &dp_du) {
    auto s = normalize(dp_du - n * dot(n, dp_du));
    return Frame<Vector3>(s, cross(n, s), n);
}

NAMESPACE_END(mitsuba)

ENOKI_STRUCT_DYNAMIC(mitsuba::Frame, s, t, n)
