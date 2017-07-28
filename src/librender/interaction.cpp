#include <mitsuba/render/interaction.h>

#include <mitsuba/render/scene.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

template MTS_EXPORT_CORE void SurfaceInteraction3f ::compute_partials(const RayDifferential3f  &);
template MTS_EXPORT_CORE void SurfaceInteraction3fP::compute_partials(const RayDifferential3fP &);

NAMESPACE_END(mitsuba)
