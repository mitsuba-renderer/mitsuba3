#include <mitsuba/render/bsdf.h>

NAMESPACE_BEGIN(mitsuba)

template struct MTS_EXPORT_RENDER BSDFSample<Point3f>;
template struct MTS_EXPORT_RENDER BSDFSample<Point3fP>;

BSDF::~BSDF() { }

MTS_IMPLEMENT_CLASS(BSDF, Object)
NAMESPACE_END(mitsuba)
