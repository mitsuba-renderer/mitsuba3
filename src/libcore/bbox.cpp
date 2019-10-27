#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

#if 0  // TODO: remove this?
template struct MTS_EXPORT_CORE BoundingBox<Point3f>;
template MTS_EXPORT_CORE Float BoundingBox<Point3f>::surface_area() const;
template MTS_EXPORT_CORE std::tuple<bool, float, float> BoundingBox<Point3f>::ray_intersect(const Ray3f &) const;
template MTS_EXPORT_CORE std::tuple<mask_t<FloatP>, FloatP, FloatP> BoundingBox<Point3f>::ray_intersect(const Ray3fP &) const;
#endif

NAMESPACE_END(mitsuba)
