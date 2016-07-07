#include <mitsuba/render/mesh.h>

NAMESPACE_BEGIN(mitsuba)

Mesh::~Mesh() { }

BoundingBox3f Mesh::bbox() const {
    return m_bbox;
}

MTS_IMPLEMENT_CLASS(Mesh, Shape)
NAMESPACE_END(mitsuba)
