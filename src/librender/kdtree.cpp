#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/mesh.h>

NAMESPACE_BEGIN(mitsuba)

ShapeKDTree::ShapeKDTree() {
    m_primitiveMap.push_back(0);
}

void ShapeKDTree::addShape(Shape *shape) {
    Assert(!ready());
	m_primitiveMap.push_back(m_primitiveMap.back() + shape->primitiveCount());
	m_shapes.push_back(shape);
	m_bbox.expand(shape->bbox());
}

MTS_IMPLEMENT_CLASS(ShapeKDTree, TShapeKDTree)
NAMESPACE_END(mitsuba)
