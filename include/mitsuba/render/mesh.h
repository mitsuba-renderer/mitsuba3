#pragma once

#include <mitsuba/core/ddistr.h>
#include <mitsuba/core/struct.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <tbb/tbb.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Mesh : public Shape {
public:
    using FaceHolder   = std::unique_ptr<uint8_t, enoki::aligned_deleter>;
    using VertexHolder = std::unique_ptr<uint8_t, enoki::aligned_deleter>;

    /// Create a new mesh with the given vertex and face data structures
    Mesh(const std::string &name,
         Struct *vertex_struct, Size vertex_count,
         Struct *face_struct, Size face_count);

    // =========================================================================
    //! @{ \name Accessors (vertices, faces, normals, etc)
    // =========================================================================

    /// Return the total number of vertices
    Size vertex_count() const { return m_vertex_count; }
    /// Return the total number of faces
    Size face_count() const { return m_face_count; }

    /// Return a \c Struct instance describing the contents of the vertex buffer
    const Struct *vertex_struct() const { return m_vertex_struct.get(); }
    /// Return a \c Struct instance describing the contents of the face buffer
    const Struct *face_struct() const { return m_face_struct.get(); }

    /// Return a pointer to the raw vertex buffer
    uint8_t *vertices() { return m_vertices.get(); }
    /// Const variant of \ref vertices.
    const uint8_t *vertices() const { return m_vertices.get(); }
    /// Const variant of \ref faces.
    uint8_t *faces() { return (uint8_t *) m_faces.get(); }
    /// Return a pointer to the raw face buffer
    const uint8_t *faces() const { return m_faces.get(); }

    /// Return a pointer (or packet of pointers) to a specific vertex
    template <typename Index, typename Pointer = like_t<Index, uint8_t *>>
    Pointer vertex(Index index) {
        return Pointer(m_vertices.get()) + m_vertex_size * index;
    }
    /// Return a pointer (or packet of pointers) to a specific vertex (const version)
    template <typename Index, typename Pointer = like_t<Index, const uint8_t *>>
    Pointer vertex(Index index) const {
        return Pointer(m_vertices.get()) + m_vertex_size * index;
    }
    /// Return a pointer (or packet of pointers) to a specific face
    template <typename Index, typename Pointer = like_t<Index, uint8_t *>>
    Pointer face(Index index) {
        return Pointer(m_faces.get()) + m_face_size * index;
    }
    /// Return a pointer (or packet of pointers) to a specific face (const version)
    template <typename Index, typename Pointer = like_t<Index, const uint8_t *>>
    Pointer face(Index index) const {
        return Pointer(m_faces.get()) + m_face_size * index;
    }

    /// Returns the face indices associated with triangle \c index
    Vector3i face_indices(Index index) const {
        return load<Vector3i>(face(index));
    }

    /// Vectorized version of \ref face_indices()
    Vector3iP face_indices(IndexP index, MaskP active = true) const {
        index *= m_face_size / (uint32_t) sizeof(Index);
        return gather<Vector3iP, sizeof(Index)>(m_faces.get(), index, active);
    }

    /// Compatibility wrapper, which strips the mask argument and invokes \ref face_indices()
    Vector3i face_indices(Index index, bool /* unused */) const {
        return face_indices(index);
    }

    /// Returns the world-space position of the vertex with index \c index
    Point3f vertex_position(Index index) const {
        return load<Point3f>(vertex(index));
    }

    /// Vectorized version of \ref vertex_position()
    Point3fP vertex_position(IndexP index, MaskP active = true) const {
        index *= m_vertex_size / (uint32_t) sizeof(Float);
        return gather<Point3fP, sizeof(Float)>(m_vertices.get(), index, active);
    }

    /// Compatibility wrapper, which strips the mask argument and invokes \ref vertex_position()
    Point3f vertex_position(Index index, bool /* unused */) const {
        return vertex_position(index);
    }

    /// Returns the normal direction of the vertex with index \c index
    Normal3f vertex_normal(Index index) const {
        return load<Normal3f>(vertex(index) + m_normal_offset);
    }

    /// Vectorized version of \ref vertex_normal()
    Normal3fP vertex_normal(IndexP index, MaskP active = true) const {
        index *= m_vertex_size / (uint32_t) sizeof(Float);
        return gather<Point3fP, sizeof(Float)>(m_vertices.get() + m_normal_offset, index, active);
    }

    /// Compatibility wrapper, which strips the mask argument and invokes \ref vertex_normal()
    Normal3f vertex_normal(Index index, bool /* unused */) const {
        return vertex_normal(index);
    }

    /// Returns the UV texture coordinates of the vertex with index \c index
    Point2f vertex_texcoord(Index index) const {
        return load<Point2f>(vertex(index) + m_texcoord_offset);
    }

    /// Vectorized version of \ref vertex_texcoord()
    Point2fP vertex_texcoord(IndexP index, MaskP active = true) const {
        index *= m_vertex_size / (uint32_t) sizeof(Float);
        return gather<Point2fP, sizeof(Float)>(m_vertices.get() + m_texcoord_offset, index, active);
    }

    /// Compatibility wrapper, which strips the mask argument and invokes \ref vertex_texcoord()
    Point2f vertex_texcoord(Index index, bool /* unused */) const {
        return vertex_texcoord(index);
    }

    /// Returns the surface area of the face with index \c index
    template <typename Index,
              typename Value = like_t<Index, Float>,
              typename Mask = mask_t<Value>>
    Value face_area(Index index, mask_t<Mask> active = true) const {
        auto fi = face_indices(index, active);

        auto p0 = vertex_position(fi[0], active),
             p1 = vertex_position(fi[1], active),
             p2 = vertex_position(fi[2], active);

        return .5f * norm(cross(p1 - p0, p2 - p0));
    }

    /// Does this mesh have per-vertex normals?
    bool has_vertex_normals() const { return m_normal_offset != 0; }

    /// Does this mesh have per-vertex texture coordinates?
    bool has_vertex_texcoords() const { return m_texcoord_offset != 0; }

    /// @}
    // =========================================================================

    /// Export mesh using the file format implemented by the subclass
    virtual void write(Stream *stream) const;

    /// Compute smooth vertex normals and replace the current normal values
    void recompute_vertex_normals();

    /// Recompute the bounding box (e.g. after modifying the vertex positions)
    void recompute_bbox();

    // =============================================================
    //! @{ \name Shape interface implementation
    // =============================================================

    virtual BoundingBox3f bbox() const override;

    virtual BoundingBox3f bbox(Index index) const override;

    virtual BoundingBox3f bbox(Index index, const BoundingBox3f &clip) const override;

    virtual Size primitive_count() const override;

    virtual Float surface_area() const override;

    using Shape::sample_position;

    virtual PositionSample3f sample_position(Float time,
                                             const Point2f &sample) const override;

    virtual PositionSample3fP sample_position(FloatP time,
                                              const Point2fP &sample,
                                              MaskP active = true) const override;

    using Shape::pdf_position;

    virtual Float pdf_position(const PositionSample3f &ps) const override;

    virtual FloatP pdf_position(const PositionSample3fP &ps,
                                MaskP active = true) const override;

    using Shape::fill_surface_interaction;

    virtual void fill_surface_interaction(const Ray3f &ray,
                                          const Float *cache,
                                          SurfaceInteraction3f &si) const override;

    virtual void fill_surface_interaction(const Ray3fP &ray,
                                          const FloatP *cache,
                                          SurfaceInteraction3fP &si,
                                          MaskP active = true) const override;

    using Shape::normal_derivative;

    virtual std::pair<Vector3f, Vector3f>
    normal_derivative(const SurfaceInteraction3f &si,
                      bool shading_frame = true) const override;

    virtual std::pair<Vector3fP, Vector3fP>
    normal_derivative(const SurfaceInteraction3fP &si,
                      bool shading_frame = true,
                      MaskP active = true) const override;

    /** \brief Ray-triangle intersection test
     *
     * Uses the algorithm by Moeller and Trumbore discussed at
     * <tt>http://www.acm.org/jgt/papers/MollerTrumbore97/code.html</tt>.
     *
     * \param index
     *    Index of the triangle to be intersected.
     * \param ray
     *    The ray segment to be used for the intersection query.
     * \return
     *    Returns an ordered tuple <tt>(mask, u, v, t)</tt>, where \c mask
     *    indicates whether an intersection was found, \c t contains the
     *    distance from the ray origin to the intersection point, and \c u and
     *    \c v contains the first two components of the intersection in
     *    barycentric coordinates
     */
    template <typename Ray,
              typename Value = typename Ray::Value,
              typename Mask  = mask_t<Value>>
    MTS_INLINE std::tuple<Mask, Value, Value, Value>
    ray_intersect_triangle(Index index, const Ray &ray, Mask active = true) const {
        const Index * idx = (const Index *) face(index);

        Point3f v0 = vertex_position(idx[0]),
                v1 = vertex_position(idx[1]),
                v2 = vertex_position(idx[2]);

        Vector3f edge1 = v1 - v0,
                 edge2 = v2 - v0;

        using Vector = typename Ray::Vector;

        Vector pvec = cross(ray.d, edge2);
        Value inv_det = rcp(dot(edge1, pvec));

        Vector tvec = ray.o - v0;
        Value u = dot(tvec, pvec) * inv_det;
        active = active && (u >= 0.f) && (u <= 1.f);

        Vector qvec = cross(tvec, edge1);
        Value v = dot(ray.d, qvec) * inv_det;
        active = active && (v >= 0.f) && (u + v <= 1.f);

        Value t = dot(edge2, qvec) * inv_det;
        active = active && (t >= ray.mint) && (t <= ray.maxt);

        return std::make_tuple(active, u, v, t);
    }

    /// @}
    // =========================================================================


    /// Return a human-readable string representation of the shape contents.
    virtual std::string to_string() const override;

    MTS_DECLARE_CLASS()

protected:
    Mesh(const Properties &);
    inline Mesh() { m_mesh = true; }
    virtual ~Mesh();

    /**
     * \brief Prepare internal tables for sampling uniformly wrt. area.
     *
     * Computes the surface area and sets up \ref m_area_distribution.
     * Thread-safe, since it uses a mutex.
     */
    void prepare_sampling_table();

    /**
     * Ensures that the sampling table is ready. This function is not really
     * const, but only makes (thread-safe) writes to \ref m_area_distribution,
     * \ref m_surface_area, and \ref m_inv_surface_area.
     */
    ENOKI_INLINE void ensure_table_ready() const {
        if (unlikely(m_surface_area < 0))
            const_cast<Mesh *>(this)->prepare_sampling_table();
    }

    template <typename Value,
              typename Point2 = Point<Value, 2>,
              typename Point3 = Point<Value, 3>,
              typename PositionSample = PositionSample<Point3>,
              typename Mask = mask_t<Value>>
    PositionSample sample_position_impl(Value time, Point2 sample,
                                        Mask value) const;

    template <typename PositionSample,
              typename Value = typename PositionSample::Value,
              typename Mask  = mask_t<Value>>
    Value pdf_position_impl(const PositionSample &ps, Mask active) const;

    template <typename Ray,
              typename Value = typename Ray::Value,
              typename Point3 = typename Ray::Point,
              typename SurfaceInteraction = mitsuba::SurfaceInteraction<Point3>,
              typename Mask = mask_t<Value>>
    void fill_surface_interaction_impl(const Ray &ray,
                                       const Value *cache,
                                       SurfaceInteraction &si,
                                       Mask active) const;

    template <typename SurfaceInteraction,
              typename Value = typename SurfaceInteraction::Value,
              typename Vector3 = typename SurfaceInteraction::Vector3,
              typename Mask = mask_t<Value>>
    std::pair<Vector3, Vector3>
    normal_derivative_impl(const SurfaceInteraction &si,
                           bool shading_frame,
                           Mask active) const;
protected:
    VertexHolder m_vertices;
    Size m_vertex_size = 0;
    FaceHolder m_faces;
    Size m_face_size = 0;

    /// Byte offset of the normal data within the vertex buffer
    Index m_normal_offset = 0;
    /// Byte offset of the texture coordinate data within the vertex buffer
    Index m_texcoord_offset = 0;

    std::string m_name;
    BoundingBox3f m_bbox;
    Transform4f m_to_world;

    Size m_vertex_count = 0;
    Size m_face_count = 0;

    ref<Struct> m_vertex_struct;
    ref<Struct> m_face_struct;

    /// Flag that can be set by the user to disable loading of vertex normals
    bool m_disable_vertex_normals = false;

    /**
     * Surface area distribution -- generated on demand when \ref
     * prepare_sampling_table() is first calls. The value of \ref
     * m_surface_area is negative until it has been computed.
     */
    DiscreteDistribution m_area_distribution;
    Float m_surface_area = -1.f;
    Float m_inv_surface_area = -1.f;
    tbb::spin_mutex m_mutex;
};

using MeshPtr = like_t<FloatP, const Mesh *>;

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for packets of Mesh pointers
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
ENOKI_CALL_SUPPORT_BEGIN(mitsuba::MeshPtr)
ENOKI_CALL_SUPPORT_SCALAR(has_vertex_normals)
ENOKI_CALL_SUPPORT_SCALAR(vertex_struct)
ENOKI_CALL_SUPPORT(vertex)
ENOKI_CALL_SUPPORT(vertex_position)
ENOKI_CALL_SUPPORT(vertex_normal)
ENOKI_CALL_SUPPORT(face)
ENOKI_CALL_SUPPORT(fill_surface_interaction)
ENOKI_CALL_SUPPORT_END(mitsuba::MeshPtr)

//! @}
// -----------------------------------------------------------------------
