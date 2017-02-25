#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

template struct MTS_EXPORT_CORE BoundingBox<Point3f>;
template MTS_EXPORT_CORE Float BoundingBox<Point3f>::surface_area() const;
template MTS_EXPORT_CORE auto BoundingBox<Point3f>::ray_intersect(const Ray3f &) const;
template MTS_EXPORT_CORE auto BoundingBox<Point3f>::ray_intersect(const Ray3fP &) const;

NAMESPACE_END(mitsuba)
