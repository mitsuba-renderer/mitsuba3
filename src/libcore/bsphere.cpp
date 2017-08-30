#include <mitsuba/core/bsphere.h>

NAMESPACE_BEGIN(mitsuba)

template struct MTS_EXPORT_CORE BoundingSphere<Point3f>;
template MTS_EXPORT_CORE std::tuple<bool, float, float>  BoundingSphere<Point3f>::ray_intersect(const Ray3f &) const;
template MTS_EXPORT_CORE std::tuple<mask_t<FloatP>, FloatP, FloatP> BoundingSphere<Point3f>::ray_intersect(const Ray3fP &) const;

NAMESPACE_END(mitsuba)
