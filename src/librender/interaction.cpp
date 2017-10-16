#include <mitsuba/render/interaction.h>

#include <mitsuba/render/scene.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

template struct MTS_EXPORT_RENDER SurfaceInteraction<Point3f>;
/* TODO this doesn't compile, simply ignore the forced template instantiations for now.
 * The `normal_derivative()` method triggers a compilation error in the enoki library.
 */
//template struct MTS_EXPORT_RENDER SurfaceInteraction<Point3fP>;

NAMESPACE_END(mitsuba)
