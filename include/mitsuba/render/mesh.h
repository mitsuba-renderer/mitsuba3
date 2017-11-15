#pragma once

#include <mitsuba/core/ddistr.h>
#include <mitsuba/core/struct.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Mesh : public Shape {
public:
    using FaceHolder   = std::unique_ptr<uint8_t, enoki::aligned_deleter>;
    using VertexHolder = std::unique_ptr<uint8_t, enoki::aligned_deleter>;

    /// Create a new mesh with the given vertex and face data structures
    Mesh(const std::string &name,
         Struct *vertex_struct, Size vertex_count,
         Struct *face_struct, Size face_count);

    /// Recompute the bounding box (must be called following changes to
    /// vertex positions).
    void recompute_bbox();

    /// Generate smooth vertex normals (overriding existing normals).
    void recompute_vertex_normals();

    // =========================================================================
    //! @{ \name Accessors (vertices, faces, normals, etc)
    // =========================================================================
    /// Return the total number of vertices
    Size vertex_count() const { return m_vertex_count; }

    /// Return a \c Struct instance describing the contents of the vertex buffer
    const Struct *vertex_struct() const { return m_vertex_struct.get(); }

    /// Return a pointer to the raw vertex buffer
    uint8_t *vertices() { return m_vertices.get(); }
    /// Const variant of \ref vertices.
    const uint8_t *vertices() const { return m_vertices.get(); }

    /// Return a packet of pointers to the raw face buffer (at faces specified
    /// by a packet of indices).
    template <typename Index, typename BufferPtr = like_t<Index, uint8_t *>>
    BufferPtr vertex(const Index &index) {
        return BufferPtr(m_vertices.get()) + m_vertex_size * index;
    }
    /// Const variant of \ref vertex.
    template <typename Index, typename BufferPtr = like_t<Index, const uint8_t *>>
    BufferPtr vertex(const Index &index) const {
        return BufferPtr(m_vertices.get()) + m_vertex_size * index;
    }

    /// Load the normal for a vertex. Assumes that \ref has_vertex_normals().
    template <typename Index>
    auto vertex_position(const Index &index,
                         const mask_t<Index> &active = true) const {
        using Value = like_t<Index, Float>;
        Point<Value, 3> res;
        auto base_offset = (Float *)m_vertices.get();
        auto indices = m_vertex_size * index;
        res.x() = gather<Value, 1>(base_offset    , indices, active);
        res.y() = gather<Value, 1>(base_offset + 1, indices, active);
        res.z() = gather<Value, 1>(base_offset + 2, indices, active);
        return res;
    }

    /// Load the normal for a vertex. Assumes that \ref has_vertex_normals().
    /// Recall that normals are stored in half precision.
    template <typename Index>
    auto vertex_normal(const Index &index,
                       const mask_t<Index> &active = true) const {
        using Value = like_t<Index, enoki::half>;
        Normal<Value, 3> res;
        auto offset = (enoki::half *)(m_vertices.get() + m_normals_offset);
        auto indices = m_vertex_size * index;
        res.x() = gather<Value, 1>(offset    , indices, active);
        res.y() = gather<Value, 1>(offset + 1, indices, active);
        res.z() = gather<Value, 1>(offset + 2, indices, active);
        return res;
    }

    /// Return the total number of faces
    Size face_count() const { return m_face_count; }

    /// Return a \c Struct instance describing the contents of the face buffer
    const Struct *face_struct() const { return m_face_struct.get(); }

    /// Return a pointer to the raw face buffer
    const uint8_t *faces() const { return m_faces.get(); }
    /// Const variant of \ref faces.
    uint8_t *faces() { return (uint8_t *) m_faces.get(); }

    /// Return a packet of pointers to the raw face buffer (at faces specified
    /// by a packet of indices).
    template <typename Index, typename BufferPtr = like_t<Index, uint8_t *>>
    BufferPtr face(const Index &index) {
        return BufferPtr(m_faces.get()) + m_face_size * index;
    }
    /// Const variant of \ref face.
    template <typename Index, typename BufferPtr = like_t<Index, const uint8_t *>>
    BufferPtr face(const Index &index) const {
        return BufferPtr(m_faces.get()) + m_face_size * index;
    }

    /// Does this mesh have per-vertex normals?
    bool has_vertex_normals() const { return m_vertex_normals; }
    /// @}
    // =========================================================================

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
    virtual Shape::Size primitive_count() const override {
        return face_count();
    }

    /**
     * \brief Return the shape's surface area.
     *
     * Assumes that the object is not undergoing some kind of
     * time-dependent scaling.
     */
    virtual Float surface_area() const override {
        ensure_table_ready();
        return m_surface_area;
    }

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
     *    Index of the triangle to be intersected.
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
    auto ray_intersect(Index index, const Ray3 &ray) const {
        using Vector3 = expr_t<typename Ray3::Vector>;
        using Value = value_t<Vector3>;

        auto idx = (const Index *) face(index);
        Assert(idx[0] < m_vertex_count &&
               idx[1] < m_vertex_count &&
               idx[2] < m_vertex_count);

        Point3f v0 = vertex_position(idx[0]);
        Point3f v1 = vertex_position(idx[1]);
        Point3f v2 = vertex_position(idx[2]);

        Vector3f edge1 = v1 - v0, edge2 = v2 - v0;
        Vector3 pvec = cross(ray.d, edge2);
        Value inv_det = rcp(dot(edge1, pvec));

        Vector3 tvec = ray.o - v0;
        Value u = dot(tvec, pvec) * inv_det;
        auto mask = (u >= 0.f) & (u <= 1.f);
        Vector3 qvec = cross(tvec, edge1);
        Value v = dot(ray.d, qvec) * inv_det;
        mask &= (v >= 0.f) & (u + v <= 1.f);

        Value t = dot(edge2, qvec) * inv_det;
        return std::make_tuple(mask, u, v, t);
    }

    // =========================================================================
    //! @{ \name Sampling routines
    // =========================================================================
    // TODO: docstrings
    virtual void sample_position(PositionSample3f &p_rec,
                                 const Point2f &sample) const override;
    virtual void sample_position(PositionSample3fP &p_rec,
                                 const Point2fP &sample,
                                 const mask_t<FloatP> &active = true) const override;

    virtual Float pdf_position(const PositionSample3f &/*p_rec*/) const override {
        ensure_table_ready();
        return m_inv_surface_area;
    }
    virtual FloatP pdf_position(
            const PositionSample3fP &/*p_rec*/,
            const mask_t<FloatP> &/*active*/ = true) const override {
        ensure_table_ready();
        return FloatP(m_inv_surface_area);
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

    /**
     * \brief Prepare internal tables for sampling uniformly wrt. area.
     *
     * Computes the surface area and sets up \ref m_area_distribution.
     * Thread-safe, since it uses a mutex.
     */
    void prepare_sampling_table();

    /// Ensures that the sampling table is ready. This function is not really
    /// const, but only makes thread-safe writes to \ref m_area_distribution,
    /// \ref m_surface_area, and \ref m_inv_surface_area.
    ENOKI_INLINE void ensure_table_ready() const {
        if (unlikely(m_surface_area < 0))
            const_cast<Mesh *>(this)->prepare_sampling_table();
    }

    template <typename SurfaceInteraction, typename Mask>
    auto normal_derivative_impl(const SurfaceInteraction &si,
                                bool shading_frame, const Mask &active) const;

    template <typename PositionSample,
              typename Point2 = typename PositionSample::Point2,
              typename Mask = mask_t<typename PositionSample::Value>>
    void sample_position_impl(PositionSample &p_rec, const Point2 &sample_,
                              const Mask &active) const;

protected:
    VertexHolder m_vertices;
    Size m_vertex_size = 0;
    FaceHolder m_faces;
    Size m_face_size = 0;
    /// Offset to the normal values in the raw vertex buffer, in bytes.
    /// Only valid if \ref m_vertex_normals is true.
    size_t m_normals_offset = 0;

    std::string m_name;
    BoundingBox3f m_bbox;
    bool m_vertex_normals;
    Transform4f m_to_world;

    Size m_vertex_count = 0;
    Size m_face_count = 0;

    ref<Struct> m_vertex_struct;
    ref<Struct> m_face_struct;


    /// Surface and distribution -- generated lazily by calling
    /// \ref prepare_sampling_table. Value of \ref m_surface_area is negative
    /// until it has been computed.
    DiscreteDistribution m_area_distribution;
    Float m_surface_area, m_inv_surface_area;
    ref<std::mutex> m_mutex;
};

// TODO: move to a more appropriate location.
using MeshPtr = Packet<const Mesh *, PacketSize>;

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
