#pragma once

#include <mitsuba/render/shape.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/struct.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Mesh : public Shape {
public:
    using Allocator    = simd::AlignedAllocator<>;
    using FaceHolder   = std::unique_ptr<uint8_t, Allocator>;
    using VertexHolder = std::unique_ptr<uint8_t, Allocator>;

    struct Vertex { Float x; Float y; Float z; };

    /// Return the total number of vertices
    Size vertexCount() const { return m_vertexCount; }

    /// Return a \c Struct instance describing the contents of the vertex buffer
    const Struct *vertexStruct() const { return m_vertexStruct.get(); }

    /// Return a pointer to the raw vertex buffer
    const uint8_t *vertices() const { return m_vertices.get(); }

    /// Return a pointer to the raw vertex buffer
    uint8_t *vertices() { return m_vertices.get(); }

    /// Return a pointer to the raw vertex buffer (at a specified vertex index)
    const uint8_t *vertex(Index index) const { return m_vertices.get() + m_vertexSize * index; }

    /// Return a pointer to the raw vertex buffer (at a specified vertex index)
    uint8_t *vertex(Index index) { return m_vertices.get() + m_vertexSize * index; }

    /// Return the total number of faces
    Size faceCount() const { return m_faceCount; }

    /// Return a \c Struct instance describing the contents of the face buffer
    const Struct *faceStruct() const { return m_faceStruct.get(); }

    /// Return a pointer to the raw face buffer
    const uint8_t *faces() const { return m_faces.get(); }

    /// Return a pointer to the raw face buffer
    uint8_t *faces() { return m_faces.get(); }

    /// Return a pointer to the raw face buffer (at a specified face index)
    const uint8_t *face(Index index) const { return m_faces.get() + m_faceSize * index; }

    /// Return a pointer to the raw face buffer (at a specified face index)
    uint8_t *face(Index index) { return m_faces.get() + m_faceSize * index; }

    // =========================================================================
    //! @{ \name Shape interface implementation
    // =========================================================================

    /// Export mesh using the file format implemented by the subclass
    virtual void write(Stream *stream) const = 0;

    /**
     * \brief Return an axis aligned box that bounds the set of triangles
     * (including any transformations that may have been applied to them)
     */
    virtual BoundingBox3f bbox() const override;

    /**
     * \brief Return an axis aligned box that bounds a single triangle
     * (including any transformations that may have been applied to it)
     */
    virtual BoundingBox3f bbox(Index index) const override;

    /**
     * \brief Return an axis aligned box that bounds a single triangle after it
     * has been clipped to another bounding box.
	 *
     * This is extremely important to construct decent kd-trees. The default
     * implementation just takes the bounding box returned by \ref bbox(Index
     * index) and clips it to \a clip.
     */
    virtual BoundingBox3f bbox(Index index,
                               const BoundingBox3f &clip) const override;

    /**
     * \brief Returns the number of sub-primitives (i.e. triangles) that make up
     * this shape
     */
    virtual Size primitiveCount() const override;

    /// @}
    // =========================================================================

    /// Return a human-readable string representation of the shape contents.
    virtual std::string toString() const override;

    MTS_DECLARE_CLASS()

protected:
    virtual ~Mesh();

    VertexHolder m_vertices;
    Size m_vertexSize = 0;
    FaceHolder m_faces;
    Size m_faceSize = 0;

    std::string m_name;
    BoundingBox3f m_bbox;

    Size m_vertexCount = 0;
    Size m_faceCount = 0;

    ref<Struct> m_faceStruct;
    ref<Struct> m_vertexStruct;
};

NAMESPACE_END(mitsuba)
