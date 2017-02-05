#pragma once

#include <mitsuba/render/shape.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/struct.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Mesh : public Shape {
public:
    using FaceHolder   = std::unique_ptr<uint8_t, enoki::aligned_deleter>;
    using VertexHolder = std::unique_ptr<uint8_t, enoki::aligned_deleter>;

    struct Vertex { Float x; Float y; Float z; };

    /// Return the total number of vertices
    Size vertex_count() const { return m_vertex_count; }

    /// Return a \c Struct instance describing the contents of the vertex buffer
    const Struct *vertex_struct() const { return m_vertex_struct.get(); }

    /// Return a pointer to the raw vertex buffer
    const uint8_t *vertices() const { return m_vertices.get(); }

    /// Return a pointer to the raw vertex buffer
    uint8_t *vertices() { return m_vertices.get(); }

    /// Return a pointer to the raw vertex buffer (at a specified vertex index)
    const uint8_t *vertex(Index index) const { return m_vertices.get() + m_vertex_size * index; }

    /// Return a pointer to the raw vertex buffer (at a specified vertex index)
    uint8_t *vertex(Index index) { return m_vertices.get() + m_vertex_size * index; }

    /// Return the total number of faces
    Size face_count() const { return m_face_count; }

    /// Return a \c Struct instance describing the contents of the face buffer
    const Struct *face_struct() const { return m_face_struct.get(); }

    /// Return a pointer to the raw face buffer
    const uint8_t *faces() const { return m_faces.get(); }

    /// Return a pointer to the raw face buffer
    uint8_t *faces() { return m_faces.get(); }

    /// Return a pointer to the raw face buffer (at a specified face index)
    const uint8_t *face(Index index) const { return m_faces.get() + m_face_size * index; }

    /// Return a pointer to the raw face buffer (at a specified face index)
    uint8_t *face(Index index) { return m_faces.get() + m_face_size * index; }

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
    virtual Size primitive_count() const override;

    /// @}
    // =========================================================================

    /// Return a human-readable string representation of the shape contents.
    virtual std::string to_string() const override;

    MTS_DECLARE_CLASS()

protected:
    virtual ~Mesh();

    VertexHolder m_vertices;
    Size m_vertex_size = 0;
    FaceHolder m_faces;
    Size m_face_size = 0;

    std::string m_name;
    BoundingBox3f m_bbox;

    Size m_vertex_count = 0;
    Size m_face_count = 0;

    ref<Struct> m_face_struct;
    ref<Struct> m_vertex_struct;
};

NAMESPACE_END(mitsuba)
