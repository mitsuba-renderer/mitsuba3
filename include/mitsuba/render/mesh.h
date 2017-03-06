#pragma once

#include <mitsuba/render/shape.h>
#include <mitsuba/core/struct.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Mesh : public Shape {
public:
    using FaceHolder   = std::unique_ptr<uint8_t, enoki::aligned_deleter>;
    using VertexHolder = std::unique_ptr<uint8_t, enoki::aligned_deleter>;

    /// Create a new mesh with the given vertex and face data structures
    Mesh(const std::string &name,
         Struct *vertex_struct, Size vertex_count,
         Struct *face_struct, Size face_count);

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
    uint8_t *faces() { return (uint8_t *) m_faces.get(); }

    /// Return a pointer to the raw face buffer (at a specified face index)
    const uint8_t *face(Index index) const { return m_faces.get() + m_face_size * index; }

    /// Return a pointer to the raw face buffer (at a specified face index)
    uint8_t *face(Index index) { return m_faces.get() + m_face_size * index; }

    /// Return whether or not the mesh has per-vertex normals
    bool vertex_normals() const { return m_vertex_normals; }

    /// Recompute the bounding box (must be called following changes to vertex positions)
    void recompute_bbox();

    /// Generate smooth vertex normals (overriding existing normals)
    void recompute_vertex_normals();

    // =========================================================================
    //! @{ \name Shape interface implementation
    // =========================================================================

    /// Export mesh using the file format implemented by the subclass
    virtual void write(Stream *stream) const;

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
     * This is extremely important to construct decent kd-trees.
     */
    virtual BoundingBox3f bbox(Index index,
                               const BoundingBox3f &clip) const override;

    /// Returns the number of sub-primitives (i.e. triangles) that make up this shape
    virtual Size primitive_count() const override;

    /// Returns the derivative of the normal vector with respect to the UV parameterization
    virtual std::pair<Vector3f, Vector3f>
    normal_derivative(const SurfaceInteraction3f &si,
                      bool shading_frame = true) const override;

    virtual std::pair<Vector3fP, Vector3fP>
    normal_derivative(const SurfaceInteraction3fP &si, bool shading_frame = true,
                      const mask_t<FloatP> &active = true) const override;

    /** \brief Ray-triangle intersection test
     *
     * Uses the algorithm by Moeller and Trumbore discussed at
     * <tt>http://www.acm.org/jgt/papers/MollerTrumbore97/code.html</tt>.
     *
     * \param index
     *    Index of the triangle to be intersected
     * \param ray
     *    The ray segment to be used for the intersection query. The
     *    ray's minimum and maximum extent values are not considered.
     * \return
     *    Returns an ordered tuple <tt>(mask, u, v, t)</tt>, where \c mask
     *    indicates whether an intersection was found, \c t contains the
     *    distance from the ray origin to the intersection point, and \c u and
     *    \c v contains the first two components of the intersection in
     *    barycentric coordinates
     */
    template <typename Ray3>
    auto ray_intersect(size_t index, const Ray3 &ray) const {
        using Vector3 = typename Ray3::Vector;
        using Float = value_t<Vector3>;

        auto idx = (const Index *) face(index);
        Assert(idx[0] < m_vertex_count &&
               idx[1] < m_vertex_count &&
               idx[2] < m_vertex_count);

        Point3f v0 = load<Point3f>((float *) vertex(idx[0]));
        Point3f v1 = load<Point3f>((float *) vertex(idx[1]));
        Point3f v2 = load<Point3f>((float *) vertex(idx[2]));

        Vector3 edge1 = v1 - v0, edge2 = v2 - v0;
        Vector3 pvec = cross(ray.d, edge2);
        Float inv_det = rcp(dot(edge1, pvec));

        Vector3 tvec = ray.o - v0;
        Float u = dot(tvec, pvec) * inv_det;
        auto mask = (u >= 0.f) & (u <= 1.f);

        auto qvec = cross(tvec, edge1);
        Float v = dot(ray.d, qvec) * inv_det;
        mask &= (v >= 0.f) & (u + v <= 1.f);

        Float t = dot(edge2, qvec) * inv_det;
        return std::make_tuple(mask, u, v, t);
    }

    /// @}
    // =========================================================================

    /// Return a human-readable string representation of the shape contents.
    virtual std::string to_string() const override;

    MTS_DECLARE_CLASS()

protected:
    Mesh(const Properties &);
    inline Mesh() { }
    virtual ~Mesh();

    template <typename SurfaceInteraction, typename Mask>
    auto normal_derivative_impl(const SurfaceInteraction &si,
                                bool shading_frame, const Mask &active) const;

    VertexHolder m_vertices;
    Size m_vertex_size = 0;
    FaceHolder m_faces;
    Size m_face_size = 0;

    std::string m_name;
    BoundingBox3f m_bbox;
    bool m_vertex_normals;
    Transform4f m_to_world;

    Size m_vertex_count = 0;
    Size m_face_count = 0;

    ref<Struct> m_vertex_struct;
    ref<Struct> m_face_struct;
};

NAMESPACE_END(mitsuba)
