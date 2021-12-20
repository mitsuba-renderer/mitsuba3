#pragma once

#include <mitsuba/core/math.h>
#include <mitsuba/core/vector.h>
#include <enoki/struct.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Stores a three-dimensional orthonormal coordinate frame
 *
 * This class is used to convert between different cartesian coordinate systems
 * and to efficiently evaluate trigonometric functions in a spherical
 * coordinate system whose pole is aligned with the \c n axis (e.g. \ref
 * cos_theta(), \ref sin_phi(), etc.).
 */
template <typename Float_> struct Frame {
    using Float    = Float_;
    MTS_IMPORT_CORE_TYPES()

    Vector3f s, t;
    Normal3f n;

    /// Construct a new coordinate frame from a single vector
    Frame(const Vector3f &v) : n(v) {
        std::tie(s, t) = coordinate_system(v);
    }

    /// Construct a new coordinate frame from three vectors
    Frame(const Vector3f &s, const Vector3f &t, const Normal3f &n)
        : s(s), t(t), n(n) {}

    /// Convert from world coordinates to local coordinates
    Vector3f to_local(const Vector3f &v) const {
        return Vector3f(ek::dot(v, s), ek::dot(v, t), ek::dot(v, n));
    }

    /// Convert from local coordinates to world coordinates
    Vector3f to_world(const Vector3f &v) const {
        return ek::fmadd(s, v.x(), ek::fmadd(t, v.y(), n * v.z()));
    }

    /** \brief Give a unit direction, this function returns the cosine of the
     * elevation angle in a reference spherical coordinate system (see the \ref
     * Frame description)
     */
    static Float cos_theta(const Vector3f &v) { return v.z(); }

    /** \brief Give a unit direction, this function returns the square cosine
     * of the elevation angle in a reference spherical coordinate system (see
     * the \ref Frame description)
     */
    static Float cos_theta_2(const Vector3f &v) { return ek::sqr(v.z()); }

    /** \brief Give a unit direction, this function returns the sine
     * of the elevation angle in a reference spherical coordinate system (see
     * the \ref Frame description)
     */
    static Float sin_theta(const Vector3f &v) { return ek::safe_sqrt(sin_theta_2(v)); }

    /** \brief Give a unit direction, this function returns the square sine
     * of the elevation angle in a reference spherical coordinate system (see
     * the \ref Frame description)
     */
    static Float sin_theta_2(const Vector3f &v) { return ek::fmadd(v.x(), v.x(), ek::sqr(v.y())); }

    /** \brief Give a unit direction, this function returns the tangent
     * of the elevation angle in a reference spherical coordinate system (see
     * the \ref Frame description)
     */
    static Float tan_theta(const Vector3f &v) {
        Float temp = ek::fnmadd(v.z(), v.z(), 1.f);
        return ek::safe_sqrt(temp) / v.z();
    }

    /** \brief Give a unit direction, this function returns the square tangent
     * of the elevation angle in a reference spherical coordinate system (see
     * the \ref Frame description)
     */
    static Float tan_theta_2(const Vector3f &v) {
        Float temp = ek::fnmadd(v.z(), v.z(), 1.f);
        return ek::max(temp, 0.f) / ek::sqr(v.z());
    }

    /** \brief Give a unit direction, this function returns the sine of the
     * azimuth in a reference spherical coordinate system (see the \ref Frame
     * description)
     */
    static Float sin_phi(const Vector3f &v) {
        Float sin_theta_2 = Frame::sin_theta_2(v),
              inv_sin_theta = ek::rsqrt(Frame::sin_theta_2(v));
        return ek::select(ek::abs(sin_theta_2) <= 4.f * ek::Epsilon<Float>, 0.f,
                          ek::clamp(v.y() * inv_sin_theta, -1.f, 1.f));
    }

    /** \brief Give a unit direction, this function returns the cosine of the
     * azimuth in a reference spherical coordinate system (see the \ref Frame
     * description)
     */
    static Float cos_phi(const Vector3f &v) {
        Float sin_theta_2 = Frame::sin_theta_2(v),
              inv_sin_theta = ek::rsqrt(Frame::sin_theta_2(v));
        return ek::select(ek::abs(sin_theta_2) <= 4.f * ek::Epsilon<Float>, 1.f,
                          ek::clamp(v.x() * inv_sin_theta, -1.f, 1.f));
    }

    /** \brief Give a unit direction, this function returns the sine and cosine
     * of the azimuth in a reference spherical coordinate system (see the \ref
     * Frame description)
     */
    static std::pair<Float, Float> sincos_phi(const Vector3f &v) {
        Float sin_theta_2 = Frame::sin_theta_2(v),
              inv_sin_theta = ek::rsqrt(Frame::sin_theta_2(v));

        Vector2f result = ek::head<2>(v) * inv_sin_theta;

        result = ek::select(ek::abs(sin_theta_2) <= 4.f * ek::Epsilon<Float>,
                            Vector2f(1.f, 0.f),
                            ek::clamp(result, -1.f, 1.f));

        return { result.y(), result.x() };
    }

    /** \brief Give a unit direction, this function returns the squared sine of
     * the azimuth in a reference spherical coordinate system (see the \ref
     * Frame description)
     */
    static Float sin_phi_2(const Vector3f &v) {
        Float sin_theta_2 = Frame::sin_theta_2(v);
        return ek::select(ek::abs(sin_theta_2) <= 4.f * ek::Epsilon<Float>, 0.f,
                          ek::clamp(ek::sqr(v.y()) / sin_theta_2, -1.f, 1.f));
    }

    /** \brief Give a unit direction, this function returns the squared cosine of
     * the azimuth in a reference spherical coordinate system (see the \ref
     * Frame description)
     */
    static Float cos_phi_2(const Vector3f &v) {
        Float sin_theta_2 = Frame::sin_theta_2(v);
        return ek::select(ek::abs(sin_theta_2) <= 4.f * ek::Epsilon<Float>, 1.f,
                          ek::clamp(ek::sqr(v.x()) / sin_theta_2, -1.f, 1.f));
    }

    /** \brief Give a unit direction, this function returns the squared sine
     * and cosine of the azimuth in a reference spherical coordinate system
     * (see the \ref Frame description)
     */
    static std::pair<Float, Float> sincos_phi_2(const Vector3f &v) {
        Float sin_theta_2 = Frame::sin_theta_2(v),
              inv_sin_theta_2 = ek::rcp(sin_theta_2);

        Vector2f result = ek::sqr(ek::head<2>(v)) * inv_sin_theta_2;

        result = ek::select(ek::abs(sin_theta_2) <= 4.f * ek::Epsilon<Float>,
                            Vector2f(1.f, 0.f), ek::clamp(result, -1.f, 1.f));

        return { result.y(), result.x() };
    }

    /// Equality test
    Mask operator==(const Frame &frame) const {
        return ek::all(ek::eq(frame.s, s) && ek::eq(frame.t, t) && ek::eq(frame.n, n));
    }

    /// Inequality test
    Mask operator!=(const Frame &frame) const {
        return ek::any(ek::neq(frame.s, s) || ek::neq(frame.t, t) || ek::neq(frame.n, n));
    }

    ENOKI_STRUCT(Frame, s, t, n)
};

/// Return a string representation of a frame
template <typename Float>
std::ostream &operator<<(std::ostream &os, const Frame<Float> &f) {
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
 *
 * \param dp_du
 *    Position derivative of the underlying parameterization with respect to
 *    the 'u' coordinate
 *
 * \param frame
 *    Used to return the computed frame
 */
template <typename Normal3f, typename Vector3f,
          typename Float = ek::value_t<Normal3f>,
          typename Frame = mitsuba::Frame<Float>>
Frame compute_shading_frame(const Normal3f &n, const Vector3f &dp_du) {
    Vector3f s = ek::normalize(ek::fnmadd(n, ek::dot(n, dp_du), dp_du));
    return Frame(s, ek::cross(n, s), n);
}

NAMESPACE_END(mitsuba)
