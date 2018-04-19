#include <mitsuba/render/interaction.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/shape.h>
#include <enoki/stl.h>

NAMESPACE_BEGIN(mitsuba)

template struct MTS_EXPORT_RENDER SurfaceInteraction<Point3f>;
template struct MTS_EXPORT_RENDER SurfaceInteraction<Point3fP>;

NAMESPACE_END(mitsuba)
