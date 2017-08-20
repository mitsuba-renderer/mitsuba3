#pragma once

#include <mitsuba/core/fwd.h>

NAMESPACE_BEGIN(mitsuba)

class BSDF;
class Shape;
class ShapeKDTree;
class Medium;
class Subsurface;
class Endpoint;
class Emitter;
class Sensor;

template <typename Vector3> struct DirectionSample;
template <typename Point3>  struct DirectSample;
template <typename Point3>  struct PositionSample;
template <typename Point3>  struct SurfaceInteraction;
template <typename Point3>  struct MediumInteraction;

// -----------------------------------------------------------------------------
// Common type aliases (non-vectorized, packet, dynamic).

using PositionSample3f       = PositionSample<Point3f>;
using PositionSample3fP      = PositionSample<Point3fP>;
using PositionSample3fX      = PositionSample<Point3fX>;

using DirectionSample3f      = DirectionSample<Vector3f>;
using DirectionSample3fP     = DirectionSample<Vector3fP>;
using DirectionSample3fX     = DirectionSample<Vector3fX>;

using DirectSample3f         = DirectSample<Point3f>;
using DirectSample3fP        = DirectSample<Point3fP>;
using DirectSample3fX        = DirectSample<Point3fX>;

using SurfaceInteraction3f   = SurfaceInteraction<Point3f>;
using SurfaceInteraction3fP  = SurfaceInteraction<Point3fP>;
using SurfaceInteraction3fX  = SurfaceInteraction<Point3fX>;

using MediumInteraction3f    = MediumInteraction<Point3f>;
using MediumInteraction3fP   = MediumInteraction<Point3fP>;
using MediumInteraction3fX   = MediumInteraction<Point3fX>;

using ShapeP  = Packet<const Shape *,  PacketSize>;
using MediumP = Packet<const Medium *, PacketSize>;
using BSDFP   = Packet<const BSDF *,   PacketSize>;

// -----------------------------------------------------------------------------

NAMESPACE_END(mitsuba)
