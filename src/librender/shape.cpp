#include <mitsuba/render/shape.h>
#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

Shape::~Shape() { }

BoundingBox3f Shape::bbox(Index) const {
    return bbox();
}

Shape::Size Shape::primitiveCount() const {
    return 1;
}

MTS_IMPLEMENT_CLASS(Shape, Object)
NAMESPACE_END(mitsuba)
