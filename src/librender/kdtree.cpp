#include <mitsuba/render/kdtree.h>

NAMESPACE_BEGIN(mitsuba)

void ShapeKDTree::addShape(Shape *shape) {
    m_shapes.push_back(shape);
    m_bbox.expand(shape->bbox());
}

MTS_IMPLEMENT_CLASS(ShapeKDTree, Object)
NAMESPACE_END(mitsuba)
