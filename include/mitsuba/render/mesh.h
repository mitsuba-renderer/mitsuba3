#pragma once

#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/core/struct.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/distr_1d.h>
#include <tbb/spin_mutex.h>
#include <unordered_map>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Mesh : public Shape<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES()
    MTS_IMPORT_BASE(Shape, m_mesh, set_children, m_emitter)

    using InputFloat = float;
    using InputPoint3f  = Point<InputFloat, 3>;
    using InputVector2f = Vector<InputFloat, 2>;
    using InputVector3f = Vector<InputFloat, 3>;
    using InputNormal3f = Normal<InputFloat, 3>;

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;

    using FaceHolder   = std::unique_ptr<uint8_t[]>;
    using VertexHolder = std::unique_ptr<uint8_t[]>;

    /// Create a new mesh with the given vertex and face data structures
    Mesh(const std::string &name,
         Struct *vertex_struct, ScalarSize vertex_count,
         Struct *face_struct,   ScalarSize face_count);

    /// Create a new mesh from a blender mesh
    Mesh(const std::string &name,
        uintptr_t loop_tri_count, uintptr_t loop_tri_ptr,
        uintptr_t loop_ptr, uintptr_t vertex_count, uintptr_t vertex_ptr,
        uintptr_t poly_ptr, uintptr_t uv_ptr, uintptr_t col_ptr,
        short mat_nr, const ScalarMatrix4f &to_world);
    // =========================================================================
    //! @{ \name Accessors (vertices, faces, normals, etc)
    // =========================================================================

    /// Return the total number of vertices
    ScalarSize vertex_count() const { return m_vertex_count; }
    /// Return the total number of faces
    ScalarSize face_count() const { return m_face_count; }

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
    template <typename Index, typename VertexPtr = replace_scalar_t<Index, uint8_t *>>
    MTS_INLINE VertexPtr vertex(const Index &index) {
        return VertexPtr(m_vertices.get()) + m_vertex_size * index;
    }

    /// Return a pointer (or packet of pointers) to a specific vertex (const version)
    template <typename Index, typename VertexPtr = replace_scalar_t<Index, const uint8_t *>>
    MTS_INLINE VertexPtr vertex(const Index &index) const {
        return VertexPtr(m_vertices.get()) + m_vertex_size * index;
    }

    /// Return a pointer (or packet of pointers) to a specific face
    template <typename Index, typename FacePtr = replace_scalar_t<Index, uint8_t *>>
    MTS_INLINE FacePtr face(const Index &index) {
        return FacePtr(m_faces.get()) + m_face_size * index;
    }

    /// Return a pointer (or packet of pointers) to a specific face (const version)
    template <typename Index, typename FacePtr = replace_scalar_t<Index, const uint8_t *>>
    MTS_INLINE FacePtr face(const Index &index) const {
        return FacePtr(m_faces.get()) + m_face_size * index;
    }

    /// Returns the face indices associated with triangle \c index
    template <typename Index>
    MTS_INLINE auto face_indices(Index index, mask_t<Index> active = true) const {
        using Index3 = Array<Index, 3>;
        using Result = uint32_array_t<Index3>;
        ENOKI_MARK_USED(active);

        if constexpr (!is_array_v<Index>) {
            return load<Result>(face(index));
        } else if constexpr (!is_cuda_array_v<Index>) {
            index *= m_face_size / ScalarSize(sizeof(ScalarIndex));
            return gather<Result, sizeof(ScalarIndex)>(
                m_faces.get(), Index3(index, index + 1u, index + 2u), active);
        }
#if defined(MTS_ENABLE_OPTIX)
        else {
            return gather<Result, sizeof(ScalarIndex)>(m_optix->faces, index, active);
        }
#endif
    }

    /// Returns the world-space position of the vertex with index \c index
    template <typename Index>
    MTS_INLINE auto vertex_position(Index index, mask_t<Index> active = true) const {
        using Index3 = Array<Index, 3>;
        using Result = Point<replace_scalar_t<Index, InputFloat>, 3>;
        ENOKI_MARK_USED(active);

        if constexpr (!is_array_v<Index>) {
            return load<Result>(vertex(index));
        } else if constexpr (!is_cuda_array_v<Index>) {
            index *= m_vertex_size / ScalarSize(sizeof(InputFloat));
            return gather<Result, sizeof(InputFloat)>(
                m_vertices.get(), Index3(index, index + 1u, index + 2u), active);
        }
#if defined(MTS_ENABLE_OPTIX)
        else {
            return gather<Result, sizeof(InputFloat)>(m_optix->vertex_positions, index, active);
        }
#endif
    }

    /// Returns the normal direction of the vertex with index \c index
    template <typename Index>
    MTS_INLINE auto vertex_normal(Index index, mask_t<Index> active = true) const {
        using Index3 = Array<Index, 3>;
        using Result = Normal<replace_scalar_t<Index, InputFloat>, 3>;
        ENOKI_MARK_USED(active);

        if constexpr (!is_array_v<Index>) {
            return load_unaligned<Result>(vertex(index) + m_normal_offset);
        } else if constexpr (!is_cuda_array_v<Index>) {
            index *= m_vertex_size / ScalarSize(sizeof(InputFloat));
            return gather<Result, sizeof(InputFloat)>(
                m_vertices.get() + m_normal_offset, Index3(index, index + 1u, index + 2u), active);
        }
#if defined(MTS_ENABLE_OPTIX)
        else {
            return gather<Result, sizeof(InputFloat)>(m_optix->vertex_normals, index, active);
        }
#endif
    }

    /// Returns the UV texture coordinates of the vertex with index \c index
    template <typename Index>
    MTS_INLINE auto vertex_texcoord(Index index, mask_t<Index> active = true) const {
        using Result = Point<replace_scalar_t<Index, InputFloat>, 2>;
        ENOKI_MARK_USED(active);

        if constexpr (!is_array_v<Index>) {
            return load_unaligned<Result>(vertex(index) + m_texcoord_offset);
        } else if constexpr (!is_cuda_array_v<Index>) {
            index *= m_vertex_size / ScalarSize(sizeof(InputFloat));
            return gather<Result, sizeof(InputFloat)>(
                m_vertices.get() + m_texcoord_offset, Array<Index, 2>(index, index + 1u), active);
        }
#if defined(MTS_ENABLE_OPTIX)
        else {
            return gather<Result, sizeof(InputFloat)>(m_optix->vertex_texcoords, index, active);
        }
#endif
    }

    /// Returns the surface area of the face with index \c index
    template <typename Index>
    auto face_area(Index index, mask_t<Index> active = true) const {
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

    /// Does this mesh have per-vertex texture colors?
    bool has_vertex_colors() const { return m_color_offset != 0; }

    /// @}
    // =========================================================================

    /// Export mesh as a binary PLY file
    void write_ply(Stream *stream) const;

    /// Compute smooth vertex normals and replace the current normal values
    void recompute_vertex_normals();

    /// Recompute the bounding box (e.g. after modifying the vertex positions)
    void recompute_bbox();

    // =============================================================
    //! @{ \name Shape interface implementation
    // =============================================================

    virtual ScalarBoundingBox3f bbox() const override;

    virtual ScalarBoundingBox3f bbox(ScalarIndex index) const override;

    virtual ScalarBoundingBox3f bbox(ScalarIndex index,
                                     const ScalarBoundingBox3f &clip) const override;

    virtual ScalarSize primitive_count() const override;

    virtual ScalarFloat surface_area() const override;

    virtual PositionSample3f sample_position(Float time, const Point2f &sample,
                                             Mask active = true) const override;

    virtual Float pdf_position(const PositionSample3f &ps, Mask active = true) const override;

    virtual void fill_surface_interaction(const Ray3f &ray,
                                          const Float *cache,
                                          SurfaceInteraction3f &si,
                                          Mask active = true) const override;

    virtual std::pair<Vector3f, Vector3f>
    normal_derivative(const SurfaceInteraction3f &si,
                      bool shading_frame = true, Mask active = true) const override;

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
    MTS_INLINE std::tuple<Mask, Float, Float, Float>
    ray_intersect_triangle(const ScalarIndex &index, const Ray3f &ray,
                           identity_t<Mask> active = true) const {
        auto fi = face_indices(index);

        Point3f p0 = vertex_position(fi[0]),
                p1 = vertex_position(fi[1]),
                p2 = vertex_position(fi[2]);

        Vector3f e1 = p1 - p0, e2 = p2 - p0;

        Vector3f pvec = cross(ray.d, e2);
        Float inv_det = rcp(dot(e1, pvec));

        Vector3f tvec = ray.o - p0;
        Float u = dot(tvec, pvec) * inv_det;
        active &= u >= 0.f && u <= 1.f;

        Vector3f qvec = cross(tvec, e1);
        Float v = dot(ray.d, qvec) * inv_det;
        active &= v >= 0.f && u + v <= 1.f;

        Float t = dot(e2, qvec) * inv_det;
        active &= t >= ray.mint && t <= ray.maxt;

        return { active, u, v, t };
    }

#if defined(MTS_ENABLE_EMBREE)
    /// Return the Embree version of this shape
    virtual RTCGeometry embree_geometry(RTCDevice device) const override;
#endif

#if defined(MTS_ENABLE_OPTIX)
    void traverse(TraversalCallback *callback) override;
    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override;
    virtual void optix_geometry() override;
    virtual void optix_build_input(OptixBuildInput&) const override;
    virtual void optix_hit_group_data(HitGroupData&) const override;
#endif

    /// @}
    // =========================================================================


    /// Return a human-readable string representation of the shape contents.
    virtual std::string to_string() const override;

protected:
    Mesh(const Properties &);
    inline Mesh() { m_mesh = true; }
    virtual ~Mesh();

    /**
     * \brief Build internal tables for sampling uniformly wrt. area.
     *
     * Computes the surface area and sets up \ref m_area_distribution.
     * Thread-safe, since it uses a mutex.
     */
    void area_distr_build();

    // Ensures that the sampling table are ready.
    ENOKI_INLINE void area_distr_ensure() const {
        if (unlikely(m_area_distr.empty()))
            const_cast<Mesh *>(this)->area_distr_build();
    }

    MTS_DECLARE_CLASS()
protected:
    VertexHolder m_vertices;
    FaceHolder m_faces;
    ScalarSize m_vertex_size = 0;
    ScalarSize m_face_size = 0;

    /// Byte offset of the normal data within the vertex buffer
    ScalarIndex m_normal_offset = 0;
    /// Byte offset of the texture coordinate data within the vertex buffer
    ScalarIndex m_texcoord_offset = 0;
    /// Byte offset of the color data within the vertex buffer
    ScalarIndex m_color_offset = 0;

    std::string m_name;
    ScalarBoundingBox3f m_bbox;
    ScalarTransform4f m_to_world;

    ScalarSize m_vertex_count = 0;
    ScalarSize m_face_count = 0;

    ref<Struct> m_vertex_struct;
    ref<Struct> m_face_struct;

#if defined(MTS_ENABLE_OPTIX)
    static const uint32_t triangle_input_flags[1];
    struct OptixData {
        /* GPU versions of the above */
        Point3u  faces;
        Point3f  vertex_positions;
        Normal3f vertex_normals;
        Point2f  vertex_texcoords;

        void* faces_buf = nullptr;
        void* vertex_positions_buf = nullptr;
        void* vertex_normals_buf = nullptr;
        void* vertex_texcoords_buf = nullptr;
    };

    std::unique_ptr<OptixData> m_optix;
#endif

    /// Flag that can be set by the user to disable loading/computation of vertex normals
    bool m_disable_vertex_normals = false;

    /* Surface area distribution -- generated on demand when \ref
       prepare_area_distr() is first called. */
    DiscreteDistribution<Float> m_area_distr;
    tbb::spin_mutex m_mutex;
};

MTS_EXTERN_CLASS_RENDER(Mesh)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for dynamic vectorization
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
ENOKI_CALL_SUPPORT_TEMPLATE_BEGIN(mitsuba::Mesh)
    ENOKI_CALL_SUPPORT_METHOD(fill_surface_interaction)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(faces, m_faces, uint8_t*)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(vertices, m_vertices, uint8_t*)
ENOKI_CALL_SUPPORT_TEMPLATE_END(mitsuba::Mesh)

//! @}
// -----------------------------------------------------------------------
