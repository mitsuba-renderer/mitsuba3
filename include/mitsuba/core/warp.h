#pragma once
#if !defined(__MITSUBA_CORE_WARP_H_)
#define __MITSUBA_CORE_WARP_H_

// #include <mitsuba/core/frame.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

namespace {
  Float kDomainEpsilon = 1e-5;
}  // end anonymous namespace

/**
 * \brief Implements common warping techniques that map from the unit
 * square to other domains, such as spheres, hemispheres, etc.
 *
 * The main application of this class is to generate uniformly
 * distributed or weighted point sets in certain common target domains.
 */
NAMESPACE_BEGIN(warp)
// =============================================================
//! @{ \name Warping techniques related to spheres and subsets
// =============================================================

/// Returns 1.0 if the point is in the domain of the unit ball, 0.0 otherwise
extern MTS_EXPORT_CORE Float unitSphereIndicator(const Vector3f &v) {
  return std::abs(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] - 1) < kDomainEpsilon ? 1.0 : 0.0;
}

/// Uniformly sample a vector on the unit sphere with respect to solid angles
extern MTS_EXPORT_CORE Vector3f squareToUniformSphere(const Point2f &sample);

/// Density of \ref squareToUniformSphere() with respect to solid angles
extern MTS_EXPORT_CORE inline Float squareToUniformSpherePdf() { return math::InvFourPi; }

/// Returns 1.0 if the point is in the domain of the upper half unit ball, 0.0 otherwise.
extern MTS_EXPORT_CORE Float unitHemisphereIndicator(const Vector3f &v) {
  return (std::abs(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] - 1) < kDomainEpsilon
          && (v[2] >= 0)) ? 1.0 : 0.0;
}

/// Uniformly sample a vector on the unit hemisphere with respect to solid angles
extern MTS_EXPORT_CORE Vector3f squareToUniformHemisphere(const Point2f &sample);

/// Density of \ref squareToUniformHemisphere() with respect to solid angles
extern MTS_EXPORT_CORE inline Float squareToUniformHemispherePdf() { return math::InvTwoPi; }

/// Sample a cosine-weighted vector on the unit hemisphere with respect to solid angles
extern MTS_EXPORT_CORE Vector3f squareToCosineHemisphere(const Point2f &sample);

/// Density of \ref squareToCosineHemisphere() with respect to solid angles
extern MTS_EXPORT_CORE inline Float squareToCosineHemispherePdf(const Vector3f &) {
  // TODO
  Log(EError, "Not implemented yet.");
  //return math::InvPi * Frame::cosTheta(d);
}

/// Returns 1.0 if the vector is in the domain of the cone, 0.0 otherwise.
extern MTS_EXPORT_CORE Float unitConeIndicator(const Vector3f &) {
  // TODO
  Log(EError, "Not implemented yet.");
}


/**
 * \brief Uniformly sample a vector that lies within a given
 * cone of angles around the Z axis
 *
 * \param cosCutoff Cosine of the cutoff angle
 * \param sample A uniformly distributed sample on \f$[0,1]^2\f$
 */
extern MTS_EXPORT_CORE Vector3f squareToUniformCone(Float cosCutoff, const Point2f &sample);

/**
 * \brief Density of \ref squareToUniformCone per unit area.
 *
 * \param cosCutoff Cosine of the cutoff angle
 */
extern MTS_EXPORT_CORE inline Float squareToUniformConePdf(Float cosCutoff) {
  return math::InvTwoPi / (1 - cosCutoff);
}

//! @}
// =============================================================

// =============================================================
//! @{ \name Warping techniques that operate in the plane
// =============================================================

/// Returns 1.0 if the point is in the domain of the unit disk, 0.0 otherwise.
extern MTS_EXPORT_CORE Float unitDiskIndicator(const Point2f &p) {
  return (p[0] * p[0] + p[1] * p[1] <= 1) ? 1.0 : 0.0;
}

/// Uniformly sample a vector on a 2D disk
extern MTS_EXPORT_CORE Point2f squareToUniformDisk(const Point2f &sample);

/// Density of \ref squareToUniformDisk per unit area
extern MTS_EXPORT_CORE inline Float squareToUniformDiskPdf() { return math::InvPi; }

/// Low-distortion concentric square to disk mapping by Peter Shirley (PDF: 1/PI)
extern MTS_EXPORT_CORE Point2f squareToUniformDiskConcentric(const Point2f &sample);

/// Returns 1.0 if the vector is in the domain of the unit square, 0.0 otherwise.
extern MTS_EXPORT_CORE Float unitSquareIndicator(const Point2f &) {
  // TODO
  Log(EError, "Not implemented yet.");
}

/// Inverse of the mapping \ref squareToUniformDiskConcentric
extern MTS_EXPORT_CORE Point2f uniformDiskToSquareConcentric(const Point2f &p);

/// Density of \ref squareToUniformDisk per unit area
extern MTS_EXPORT_CORE inline Float squareToUniformDiskConcentricPdf() { return math::InvPi; }

/// Returns 1.0 if the vector is in the domain of the triangle, 0.0 otherwise.
extern MTS_EXPORT_CORE Float triangleIndicator(const Point2f &) {
  // TODO
  Log(EError, "Not implemented yet.");
}

/// Convert an uniformly distributed square sample into barycentric coordinates
extern MTS_EXPORT_CORE Point2f squareToUniformTriangle(const Point2f &sample);

/// Density of \ref squareToUniformTriangle per unit area.
extern MTS_EXPORT_CORE Float squareToUniformTrianglePdf(const Point2f &p) {
  return (p[0] >= 0 && p[1] >= 0 && p[0] <= 1 && p[1] <= 1)
      && (p[0] + p[1] <= 1) ? 1.0 : 0.0;
}

/** \brief Sample a point on a 2D standard normal distribution
 * Internally uses the Box-Muller transformation */
extern MTS_EXPORT_CORE Point2f squareToStdNormal(const Point2f &sample);

/// Density of \ref squareToStdNormal per unit area
extern MTS_EXPORT_CORE Float squareToStdNormalPdf(const Point2f &pos);

/// Warp a uniformly distributed square sample to a 2D tent distribution
extern MTS_EXPORT_CORE Point2f squareToTent(const Point2f &sample);

/// Density of \ref squareToTent per unit area.
extern MTS_EXPORT_CORE Float squareToTentPdf(const Point2f &) {
  // TODO
  Log(EError, "Not implemented yet.");
}

/** \brief Warp a uniformly distributed sample on [0, 1] to a nonuniform
 * tent distribution with nodes <tt>{a, b, c}</tt>
 */
extern MTS_EXPORT_CORE Float intervalToNonuniformTent(Float a, Float b, Float c, Float sample);

//! @}
// =============================================================
NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)

#endif /* __MITSUBA_CORE_WARP_H_ */
