#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Implements common warping techniques that map from the unit
 * square [0, 1]^2 to other domains such as spheres, hemispheres, etc.
 *
 * The main application of this class is to generate uniformly
 * distributed or weighted point sets in certain common target domains.
 */
NAMESPACE_BEGIN(warp)

// =============================================================
//! @{ \name Warping techniques related to spheres and subsets
// =============================================================

/// Uniformly sample a vector on the unit sphere with respect to solid angles
template <typename Vector3f, typename Point2f>
Vector3f square_to_uniform_sphere(Point2f sample) {
    auto z = 1.f - 2.f * sample.y();
    auto r = safe_sqrt(1.f - z*z);
    auto sc = sincos(2.f * math::Pi * sample.x());
    return Vector3f(r * sc.second, r * sc.first, z);
}

extern template MTS_EXPORT_CORE Vector3f  square_to_uniform_sphere(Point2f  sample);
extern template MTS_EXPORT_CORE Vector3fP square_to_uniform_sphere(Point2fP sample);

/// Density of \ref square_to_uniform_sphere() with respect to solid angles
template <bool TestDomain = false, typename Vector3f, typename Float = value_t<Vector3f>>
Float square_to_uniform_sphere_pdf(Vector3f v) {
    if (TestDomain) {
        return select(abs(squared_norm(v) - 1.f) > math::Epsilon,
                      Float(0.0f), Float(math::InvFourPi));
    } else {
        return math::InvFourPi;
    }
}

extern template MTS_EXPORT_CORE Float  square_to_uniform_sphere_pdf<true>(Vector3f  sample);
extern template MTS_EXPORT_CORE FloatP square_to_uniform_sphere_pdf<true>(Vector3fP sample);
extern template MTS_EXPORT_CORE Float  square_to_uniform_sphere_pdf<false>(Vector3f  sample);
extern template MTS_EXPORT_CORE FloatP square_to_uniform_sphere_pdf<false>(Vector3fP sample);

#if 0
// ** TO BE DONE **

/// Uniformly sample a vector on the unit hemisphere with respect to solid angles
extern MTS_EXPORT_CORE Vector3f square_to_uniform_hemisphere(const Point2f &sample);

/// Density of \ref square_to_uniform_hemisphere() with respect to solid angles
template <bool TestDomain = false>
Float square_to_uniform_hemisphere_pdf(const Vector3f &v) {
    if (TestDomain && (std::abs(squared_norm(v) - 1.f) > math::Epsilon ||
                       Frame3f::cos_theta(v) < 0))
        return 0.f;
    return math::InvTwoPi;
}

/// Sample a cosine-weighted vector on the unit hemisphere with respect to solid angles
extern MTS_EXPORT_CORE Vector3f square_to_cosine_hemisphere(const Point2f &sample);

/// Density of \ref square_to_cosine_hemisphere() with respect to solid angles
template <bool TestDomain = false>
Float square_to_cosine_hemisphere_pdf(const Vector3f &v) {
    if (TestDomain && (std::abs(squared_norm(v) - 1.f) > math::Epsilon ||
                       Frame3f::cos_theta(v) < 0))
        return 0.f;
    return math::InvPi * Frame3f::cos_theta(v);
}

/**
 * \brief Uniformly sample a vector that lies within a given
 * cone of angles around the Z axis
 *
 * \param cosCutoff Cosine of the cutoff angle
 * \param sample A uniformly distributed sample on \f$[0,1]^2\f$
 */
extern MTS_EXPORT_CORE Vector3f square_to_uniform_cone(const Point2f &sample, Float cosCutoff);

/**
 * \brief Density of \ref square_to_uniform_cone per unit area.
 *
 * \param cosCutoff Cosine of the cutoff angle
 */
template <bool TestDomain = false>
inline Float square_to_uniform_cone_pdf(const Vector3f &v, Float cosCutoff) {
    if (TestDomain && (std::abs(squared_norm(v) - 1.f) > math::Epsilon ||
                       Frame3f::cos_theta(v) < cosCutoff))
        return 0.0;
    return math::InvTwoPi / (1 - cosCutoff);
}

/// Warp a uniformly distributed square sample to a Beckmann distribution *
/// cosine for the given 'alpha' parameter
extern MTS_EXPORT_CORE Vector3f square_to_beckmann(const Point2f &sample,
                                                 Float alpha);

/// Probability density of \ref square_to_beckmann()
extern MTS_EXPORT_CORE Float square_to_beckmann_pdf(const Vector3f &m,
                                                 Float alpha);

/// Warp a uniformly distributed square sample to a von Mises Fisher distribution
extern MTS_EXPORT_CORE Vector3f square_to_von_mises_fisher(const Point2f &sample,
                                                       Float kappa);

/// Probability density of \ref square_to_von_mises_fisher()
extern MTS_EXPORT_CORE Float square_to_von_mises_fisher_pdf(const Vector3f &v,
                                                       Float kappa);

/// Warp a uniformly distributed square sample to a rough fiber distribution
extern MTS_EXPORT_CORE Vector3f square_to_rough_fiber(const Point3f &sample,
                                                   const Vector3f &wi,
                                                   const Vector3f &tangent,
                                                   Float kappa);

/// Probability density of \ref square_to_rough_fiber()
extern MTS_EXPORT_CORE Float square_to_rough_fiber_pdf(const Vector3f &v,
                                                   const Vector3f &wi,
                                                   const Vector3f &tangent,
                                                   Float kappa);

//! @}
// =============================================================

// =============================================================
//! @{ \name Warping techniques that operate in the plane
// =============================================================

/// Uniformly sample a vector on a 2D disk
extern MTS_EXPORT_CORE Point2f square_to_uniform_disk(const Point2f &sample);

/// Density of \ref square_to_uniform_disk per unit area
template <bool TestDomain = false>
Float square_to_uniform_disk_pdf(const Point2f &p) {
    if (TestDomain && squared_norm(p) > 1)
        return 0.f;
    return math::InvPi;
}

/// Low-distortion concentric square to disk mapping by Peter Shirley (PDF: 1/PI)
extern MTS_EXPORT_CORE Point2f square_to_uniform_disk_concentric(const Point2f &sample);

/// Density of \ref square_to_uniform_disk per unit area
template <bool TestDomain = false>
Float square_to_uniform_disk_concentric_pdf(const Point2f &p) {
    if (TestDomain && squared_norm(p) > 1)
        return 0.f;
    return math::InvPi;
}

/// Inverse of the mapping \ref square_to_uniform_disk_concentric
extern MTS_EXPORT_CORE Point2f disk_to_uniform_square_concentric(const Point2f &p);

/// Convert an uniformly distributed square sample into barycentric coordinates
extern MTS_EXPORT_CORE Point2f square_to_uniform_triangle(const Point2f &sample);

/// Density of \ref square_to_uniform_triangle per unit area.
template <bool TestDomain = false>
Float square_to_uniform_triangle_pdf(const Point2f &p) {
    if (TestDomain && (p.x() < 0 || p.y() < 0 || p.x() + p.y() > 1))
        return 0.f;
    return 2.f;
}

/** \brief Sample a point on a 2D standard normal distribution
 * Internally uses the Box-Muller transformation */
extern MTS_EXPORT_CORE Point2f square_to_std_normal(const Point2f &sample);

/// Density of \ref square_to_std_normal per unit area
extern MTS_EXPORT_CORE Float square_to_std_normal_pdf(const Point2f &p);

/// Warp a uniformly distributed square sample to a 2D tent distribution
extern MTS_EXPORT_CORE Point2f square_to_tent(const Point2f &sample);

/// Density of \ref square_to_tent per unit area.
extern MTS_EXPORT_CORE Float square_to_tent_pdf(const Point2f &p);

/** \brief Warp a uniformly distributed sample on [0, 1] to a nonuniform
 * tent distribution with nodes <tt>{a, b, c}</tt>
 */
extern MTS_EXPORT_CORE Float interval_to_nonuniform_tent(Float sample, Float a, Float b, Float c);

//! @}
// =============================================================
#endif

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
