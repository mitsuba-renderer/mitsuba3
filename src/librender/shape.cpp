#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

Shape::~Shape() { }

BoundingBox3f Shape::bbox(Index) const {
    return bbox();
}

BoundingBox3f Shape::bbox(Index index, const BoundingBox3f &clip) const {
    BoundingBox3f result = bbox(index);
    result.clip(clip);
    return result;
}

Shape::Size Shape::primitive_count() const {
    return 1;
}

void Shape::adjust_time(SurfaceInteraction3f &si, const Float &time) const {
    si.time = time;
}

void Shape::adjust_time(SurfaceInteraction3fP &si, const FloatP &time,
                        const mask_t<FloatP> &active) const {
    si.time[active] = time;
}

MTS_IMPLEMENT_CLASS(Shape, Object)
NAMESPACE_END(mitsuba)
