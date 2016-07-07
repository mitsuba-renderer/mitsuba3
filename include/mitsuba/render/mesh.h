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

    /// Return an axis aligned box that bounds the (transformed) shape geometry
    BoundingBox3f bbox() const override;

    /// Return a pointer to the raw vertex buffer
    const VertexType *vertices() const { return m_vertices.get(); }

    /// Return a \c Struct instance describing the contents of the vertex buffer
    const Struct *vertexStruct() const { return m_vertexStruct.get(); }

    /// Return a pointer to the raw vertex buffer
    const IndexType *faces() const { return m_faces.get(); }

    /// Return a \c Struct instance describing the contents of the face buffer
    const Struct *faceStruct() const { return m_faceStruct.get(); }

    /// Export mesh using the file format implemented by the subclass
    virtual void write(Stream *stream) const = 0;

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
