#pragma once

#include <mitsuba/render/shape.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/struct.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Mesh : public Shape {
public:
    using IndexType    = uint32_t;
    using SizeType     = uint32_t;
    using VertexType   = uint8_t;
    using Allocator    = simd::AlignedAllocator<>;
    using FaceHolder   = std::unique_ptr<IndexType[], Allocator>;
    using VertexHolder = std::unique_ptr<VertexType[], Allocator>;

    MTS_DECLARE_CLASS()

protected:
    virtual ~Mesh();

    BoundingBox3f m_bbox;
    std::string m_name;

    SizeType m_faceCount = 0;
    ref<Struct> m_faceStruct;
    FaceHolder m_faces;

    SizeType m_vertexCount = 0;
    ref<Struct> m_vertexStruct;
    VertexHolder m_vertices;
};

NAMESPACE_END(mitsuba)
