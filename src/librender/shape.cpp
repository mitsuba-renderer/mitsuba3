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

MTS_IMPLEMENT_CLASS(Shape, Object)
NAMESPACE_END(mitsuba)
