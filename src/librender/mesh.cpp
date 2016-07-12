#include <mitsuba/render/mesh.h>

NAMESPACE_BEGIN(mitsuba)

Mesh::~Mesh() { }

BoundingBox3f Mesh::bbox() const {
    return m_bbox;
}

BoundingBox3f Mesh::bbox(Index index) const {
    using simd::min;
    using simd::max;

    auto idx = (const Index *) face(index);
    Assert(idx[0] < vertexCount());
    Assert(idx[1] < vertexCount());
    Assert(idx[2] < vertexCount());

    Vector3f v0, v1, v2;
    v0.loadUnaligned((float *) vertex(idx[0]));
    v1.loadUnaligned((float *) vertex(idx[1]));
    v2.loadUnaligned((float *) vertex(idx[2]));

    return BoundingBox3f(
        min(min(v0, v1), v2),
        max(max(v0, v1), v2)
    );
}

Shape::Size Mesh::primitiveCount() const {
    return faceCount();
}

MTS_IMPLEMENT_CLASS(Mesh, Shape)
NAMESPACE_END(mitsuba)
