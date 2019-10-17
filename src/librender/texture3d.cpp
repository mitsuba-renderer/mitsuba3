#include <mitsuba/render/interaction.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/texture3d.h>

NAMESPACE_BEGIN(mitsuba)

MTS_IMPLEMENT_CLASS_TEMPLATE(Texture3D, Object)
MTS_IMPLEMENT_CLASS_TEMPLATE(Grid3DBase, Texture3D)

NAMESPACE_END(mitsuba)
