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
extern MTS_EXPORT_CORE Vector3f squareToUniformSphere(const Point2f &sample);

/// Density of \ref squareToUniformSphere() with respect to solid angles
template <bool TestDomain = false>
Float squareToUniformSpherePdf(const Vector3f &v) {
    if (TestDomain && std::abs(simd::squaredNorm(v) - 1.f) > math::Epsilon)
        return 0.f;
    return math::InvFourPi;
}

/// Uniformly sample a vector on the unit hemisphere with respect to solid angles
extern MTS_EXPORT_CORE Vector3f squareToUniformHemisphere(const Point2f &sample);

/// Density of \ref squareToUniformHemisphere() with respect to solid angles
template <bool TestDomain = false>
Float squareToUniformHemispherePdf(const Vector3f &v) {
    if (TestDomain && (std::abs(simd::squaredNorm(v) - 1.f) > math::Epsilon ||
                       Frame::cosTheta(v) < 0))
        return 0.f;
    return math::InvTwoPi;
}

/// Sample a cosine-weighted vector on the unit hemisphere with respect to solid angles
extern MTS_EXPORT_CORE Vector3f squareToCosineHemisphere(const Point2f &sample);

/// Density of \ref squareToCosineHemisphere() with respect to solid angles
template <bool TestDomain = false>
Float squareToCosineHemispherePdf(const Vector3f &v) {
    if (TestDomain && (std::abs(simd::squaredNorm(v) - 1.f) > math::Epsilon ||
                       Frame::cosTheta(v) < 0))
        return 0.f;
    return math::InvPi * Frame::cosTheta(v);
}

/**
 * \brief Uniformly sample a vector that lies within a given
 * cone of angles around the Z axis
 *
 * \param cosCutoff Cosine of the cutoff angle
 * \param sample A uniformly distributed sample on \f$[0,1]^2\f$
 */
extern MTS_EXPORT_CORE Vector3f squareToUniformCone(const Point2f &sample, Float cosCutoff);

/**
 * \brief Density of \ref squareToUniformCone per unit area.
 *
 * \param cosCutoff Cosine of the cutoff angle
 */
template <bool TestDomain = false>
inline Float squareToUniformConePdf(const Vector3f &v, Float cosCutoff) {
    if (TestDomain && (std::abs(simd::squaredNorm(v) - 1.f) > math::Epsilon ||
                       Frame::cosTheta(v) < cosCutoff))
        return 0.0;
    return math::InvTwoPi / (1 - cosCutoff);
}

/// Warp a uniformly distributed square sample to a Beckmann distribution *
/// cosine for the given 'alpha' parameter
extern MTS_EXPORT_CORE Vector3f squareToBeckmann(const Point2f &sample,
                                                 Float alpha);

/// Probability density of \ref squareToBeckmann()
extern MTS_EXPORT_CORE Float squareToBeckmannPdf(const Vector3f &m,
                                                 Float alpha);

/// Warp a uniformly distributed square sample to a von Mises Fisher distribution
extern MTS_EXPORT_CORE Vector3f squareToVonMisesFisher(const Point2f &sample,
                                                       Float kappa);

/// Probability density of \ref squareToVonMisesFisher()
extern MTS_EXPORT_CORE Float squareToVonMisesFisherPdf(const Vector3f &v,
                                                       Float kappa);

/// Warp a uniformly distributed square sample to a rough fiber distribution
extern MTS_EXPORT_CORE Vector3f squareToRoughFiber(const Point3f &sample,
                                                   const Vector3f &wi,
                                                   const Vector3f &tangent,
                                                   Float kappa);

/// Probability density of \ref squareToRoughFiber()
extern MTS_EXPORT_CORE Float squareToRoughFiberPdf(const Vector3f &v,
                                                   const Vector3f &wi,
                                                   const Vector3f &tangent,
                                                   Float kappa);

//! @}
// =============================================================

// =============================================================
//! @{ \name Warping techniques that operate in the plane
// =============================================================

/// Uniformly sample a vector on a 2D disk
extern MTS_EXPORT_CORE Point2f squareToUniformDisk(const Point2f &sample);

/// Density of \ref squareToUniformDisk per unit area
template <bool TestDomain = false>
Float squareToUniformDiskPdf(const Point2f &p) {
    if (TestDomain && simd::squaredNorm(p) > 1)
        return 0.f;
    return math::InvPi;
}

/// Low-distortion concentric square to disk mapping by Peter Shirley (PDF: 1/PI)
extern MTS_EXPORT_CORE Point2f squareToUniformDiskConcentric(const Point2f &sample);

/// Density of \ref squareToUniformDisk per unit area
template <bool TestDomain = false>
Float squareToUniformDiskConcentricPdf(const Point2f &p) {
    if (TestDomain && simd::squaredNorm(p) > 1)
        return 0.f;
    return math::InvPi;
}

/// Inverse of the mapping \ref squareToUniformDiskConcentric
extern MTS_EXPORT_CORE Point2f diskToUniformSquareConcentric(const Point2f &p);

/// Convert an uniformly distributed square sample into barycentric coordinates
extern MTS_EXPORT_CORE Point2f squareToUniformTriangle(const Point2f &sample);

/// Density of \ref squareToUniformTriangle per unit area.
template <bool TestDomain = false>
Float squareToUniformTrianglePdf(const Point2f &p) {
    if (TestDomain && (p.x() < 0 || p.y() < 0 || p.x() + p.y() > 1))
        return 0.f;
    return 2.f;
}

/** \brief Sample a point on a 2D standard normal distribution
 * Internally uses the Box-Muller transformation */
extern MTS_EXPORT_CORE Point2f squareToStdNormal(const Point2f &sample);

/// Density of \ref squareToStdNormal per unit area
extern MTS_EXPORT_CORE Float squareToStdNormalPdf(const Point2f &p);

/// Warp a uniformly distributed square sample to a 2D tent distribution
extern MTS_EXPORT_CORE Point2f squareToTent(const Point2f &sample);

/// Density of \ref squareToTent per unit area.
extern MTS_EXPORT_CORE Float squareToTentPdf(const Point2f &p);

/** \brief Warp a uniformly distributed sample on [0, 1] to a nonuniform
 * tent distribution with nodes <tt>{a, b, c}</tt>
 */
extern MTS_EXPORT_CORE Float intervalToNonuniformTent(Float sample, Float a, Float b, Float c);

//! @}
// =============================================================

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
