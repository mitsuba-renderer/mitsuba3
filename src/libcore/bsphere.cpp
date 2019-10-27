#include <mitsuba/core/bsphere.h>

NAMESPACE_BEGIN(mitsuba)

#if 0  // TODO: remove this?
template struct MTS_EXPORT_CORE BoundingSphere<Point3f>;
template MTS_EXPORT_CORE std::tuple<bool, float, float>  BoundingSphere<Point3f>::ray_intersect(const Ray3f &) const;
template MTS_EXPORT_CORE std::tuple<mask_t<FloatP>, FloatP, FloatP> BoundingSphere<Point3f>::ray_intersect(const Ray3fP &) const;
#endif

NAMESPACE_END(mitsuba)
