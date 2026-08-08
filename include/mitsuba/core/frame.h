#pragma once

#include <mitsuba/core/math.h>
#include <mitsuba/core/vector.h>
#include <drjit/array_traverse.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Stores a three-dimensional orthonormal coordinate frame
 *
 * This class is used to convert between different cartesian coordinate systems
 * and to efficiently evaluate trigonometric functions in a spherical
 * coordinate system whose pole is aligned with the ``n`` axis (e.g. `cos_theta()`, `sin_phi()`, etc.).
 */
template <typename Float_> struct Frame {
    using Float    = Float_;
    MI_IMPORT_CORE_TYPES()

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
        return Vector3f(dr::dot(v, s), dr::dot(v, t), dr::dot(v, n));
    }

    /// Convert from local coordinates to world coordinates
    Vector3f to_world(const Vector3f &v) const {
        return dr::fmadd(n, v.z(), dr::fmadd(t, v.y(), s * v.x()));
    }

    /**
     * Give a unit direction, this function returns the cosine of the
     * elevation angle in a reference spherical coordinate system (see the `Frame3f` description)
     */
    static Float cos_theta(const Vector3f &v) { return v.z(); }

    /**
     * Give a unit direction, this function returns the square cosine
     * of the elevation angle in a reference spherical coordinate system (see
     * the `Frame3f` description)
     */
    static Float cos_theta_2(const Vector3f &v) { return dr::square(v.z()); }

    /**
     * Give a unit direction, this function returns the sine
     * of the elevation angle in a reference spherical coordinate system (see
     * the `Frame3f` description)
     */
    static Float sin_theta(const Vector3f &v) { return dr::safe_sqrt(sin_theta_2(v)); }

    /**
     * Give a unit direction, this function returns the square sine
     * of the elevation angle in a reference spherical coordinate system (see
     * the `Frame3f` description)
     */
    static Float sin_theta_2(const Vector3f &v) { return dr::fmadd(v.x(), v.x(), dr::square(v.y())); }

    /**
     * Give a unit direction, this function returns the tangent
     * of the elevation angle in a reference spherical coordinate system (see
     * the `Frame3f` description)
     */
    static Float tan_theta(const Vector3f &v) {
        Float temp = dr::fnmadd(v.z(), v.z(), 1.f);
        return dr::safe_sqrt(temp) / v.z();
    }

    /**
     * Give a unit direction, this function returns the square tangent
     * of the elevation angle in a reference spherical coordinate system (see
     * the `Frame3f` description)
     */
    static Float tan_theta_2(const Vector3f &v) {
        Float temp = dr::fnmadd(v.z(), v.z(), 1.f);
        return dr::maximum(temp, 0.f) / dr::square(v.z());
    }

    /**
     * Give a unit direction, this function returns the sine of the
     * azimuth in a reference spherical coordinate system (see the `Frame3f`
     * description)
     */
    static Float sin_phi(const Vector3f &v) {
        Float sin_theta_2 = Frame::sin_theta_2(v),
              inv_sin_theta = dr::rsqrt(Frame::sin_theta_2(v));
        return dr::select(dr::abs(sin_theta_2) <= 4.f * dr::Epsilon<Float>, 0.f,
                          dr::clip(v.y() * inv_sin_theta, -1.f, 1.f));
    }

    /**
     * Give a unit direction, this function returns the cosine of the
     * azimuth in a reference spherical coordinate system (see the `Frame3f`
     * description)
     */
    static Float cos_phi(const Vector3f &v) {
        Float sin_theta_2 = Frame::sin_theta_2(v),
              inv_sin_theta = dr::rsqrt(Frame::sin_theta_2(v));
        return dr::select(dr::abs(sin_theta_2) <= 4.f * dr::Epsilon<Float>, 1.f,
                          dr::clip(v.x() * inv_sin_theta, -1.f, 1.f));
    }

    /**
     * Give a unit direction, this function returns the sine and cosine
     * of the azimuth in a reference spherical coordinate system (see the `Frame3f` description)
     */
    static std::pair<Float, Float> sincos_phi(const Vector3f &v) {
        Float sin_theta_2 = Frame::sin_theta_2(v),
              inv_sin_theta = dr::rsqrt(Frame::sin_theta_2(v));

        Vector2f result = dr::head<2>(v) * inv_sin_theta;

        result = dr::select(dr::abs(sin_theta_2) <= 4.f * dr::Epsilon<Float>,
                            Vector2f(1.f, 0.f),
                            dr::clip(result, -1.f, 1.f));

        return { result.y(), result.x() };
    }

    /**
     * Give a unit direction, this function returns the squared sine of
     * the azimuth in a reference spherical coordinate system (see the `Frame3f` description)
     */
    static Float sin_phi_2(const Vector3f &v) {
        Float sin_theta_2 = Frame::sin_theta_2(v);
        return dr::select(dr::abs(sin_theta_2) <= 4.f * dr::Epsilon<Float>, 0.f,
                          dr::clip(dr::square(v.y()) / sin_theta_2, -1.f, 1.f));
    }

    /**
     * Give a unit direction, this function returns the squared cosine of
     * the azimuth in a reference spherical coordinate system (see the `Frame3f` description)
     */
    static Float cos_phi_2(const Vector3f &v) {
        Float sin_theta_2 = Frame::sin_theta_2(v);
        return dr::select(dr::abs(sin_theta_2) <= 4.f * dr::Epsilon<Float>, 1.f,
                          dr::clip(dr::square(v.x()) / sin_theta_2, -1.f, 1.f));
    }

    /**
     * Give a unit direction, this function returns the squared sine
     * and cosine of the azimuth in a reference spherical coordinate system
     * (see the `Frame3f` description)
     */
    static std::pair<Float, Float> sincos_phi_2(const Vector3f &v) {
        Float sin_theta_2 = Frame::sin_theta_2(v),
              inv_sin_theta_2 = dr::rcp(sin_theta_2);

        Vector2f result = dr::square(dr::head<2>(v)) * inv_sin_theta_2;

        result = dr::select(dr::abs(sin_theta_2) <= 4.f * dr::Epsilon<Float>,
                            Vector2f(1.f, 0.f), dr::clip(result, -1.f, 1.f));

        return { result.y(), result.x() };
    }

    /// Equality test
    Mask operator==(const Frame &frame) const {
        return dr::all((frame.s == s) && (frame.t == t) && (frame.n == n));
    }

    /// Inequality test
    Mask operator!=(const Frame &frame) const {
        return dr::any((frame.s != s) || (frame.t != t) || (frame.n != n));
    }

    DRJIT_STRUCT(Frame, s, t, n)
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
 * Given a smoothly varying shading normal and a tangent of a shape
 * parameterization, compute a smoothly varying orthonormal frame.
 *
 * Args:
 *     n: A shading normal at a surface position
 *
 *     dp_du: Position derivative of the underlying parameterization with respect to
 *         the 'u' coordinate
 */
template <typename Normal3f, typename Vector3f,
          typename Float = dr::value_t<Normal3f>,
          typename Frame = mitsuba::Frame<Float>>
Frame compute_shading_frame(const Normal3f &n, const Vector3f &dp_du) {
    Vector3f s = dr::normalize(dr::fnmadd(n, dr::dot(n, dp_du), dp_du));
    return Frame(s, dr::cross(n, s), n);
}

NAMESPACE_END(mitsuba)
