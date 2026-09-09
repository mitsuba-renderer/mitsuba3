#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/platform.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Frame parameters of a volume's local coordinates in world space.
 *
 * .. note::
 *     Currently only holds a ``to_world`` matrix but becomes useful in the
 *     presence of volume that use different coordinate systems.
 */
template<typename Float>
struct VolumeParametrization {

    using AffineTransform4f = Transform<Point<Float, 4>, true>;

    AffineTransform4f to_world;

    VolumeParametrization(): to_world(AffineTransform4f()) {}

    VolumeParametrization(AffineTransform4f to_world): to_world(to_world) {}
};

NAMESPACE_END(mitsuba)
