#pragma once

#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/core/struct.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/distr_1d.h>
#include <mitsuba/core/properties.h>
#include <tbb/spin_mutex.h>
#include <unordered_map>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Mesh : public Shape<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES()
    MTS_IMPORT_BASE(Shape, m_to_world, set_children)

    // Mesh is always stored in single precision
    using InputFloat = float;
    using InputPoint3f  = Point<InputFloat, 3>;
    using InputVector2f = Vector<InputFloat, 2>;
    using InputVector3f = Vector<InputFloat, 3>;
    using InputNormal3f = Normal<InputFloat, 3>;

    using FloatStorage = DynamicBuffer<replace_scalar_t<Float, InputFloat>>;

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;

    /// Create a new mesh with the given vertex and face data structures
    Mesh(const std::string &name, ScalarSize vertex_count,
         ScalarSize face_count, const Properties &props = Properties(),
         bool has_vertex_normals = false, bool has_vertex_texcoords = false);

    // =========================================================================
    //! @{ \name Accessors (vertices, faces, normals, etc)
    // =========================================================================

    /// Return the total number of vertices
    ScalarSize vertex_count() const { return m_vertex_count; }
    /// Return the total number of faces
    ScalarSize face_count() const { return m_face_count; }

    /// Return vertex positions buffer
    FloatStorage& vertex_positions_buffer() { return m_vertex_positions_buf; }
    /// Const variant of \ref vertex_positions_buffer.
    const FloatStorage& vertex_positions_buffer() const { return m_vertex_positions_buf; }

    /// Return vertex normals buffer
    FloatStorage& vertex_normals_buffer() { return m_vertex_normals_buf; }
    /// Const variant of \ref vertex_normals_buffer.
    const FloatStorage& vertex_normals_buffer() const { return m_vertex_normals_buf; }

    /// Return vertex texcoords buffer
    FloatStorage& vertex_texcoords_buffer() { return m_vertex_texcoords_buf; }
    /// Const variant of \ref vertex_texcoords_buffer.
    const FloatStorage& vertex_texcoords_buffer() const { return m_vertex_texcoords_buf; }

    /// Return face indices buffer
    DynamicBuffer<UInt32>& faces_buffer() { return m_faces_buf; }
    /// Const variant of \ref faces_buffer.
    const DynamicBuffer<UInt32>& faces_buffer() const { return m_faces_buf; }

    /// Return the mesh attribute associated with \c name
    FloatStorage& attribute_buffer(const std::string& name) {
        auto attribute = m_mesh_attributes.find(name);
        if (attribute == m_mesh_attributes.end())
            Throw("attribute_buffer(): attribute %s doesn't exist.", name.c_str());
        return attribute->second.buf;
    }

    /// Add an attribute buffer with the given \c name and \c dim
    void add_attribute(const std::string& name, size_t dim, const FloatStorage& buf);

    /// Returns the face indices associated with triangle \c index
    template <typename Index>
    MTS_INLINE auto face_indices(Index index, mask_t<Index> active = true) const {
        using Result = Array<replace_scalar_t<Index, uint32_t>, 3>;
        return gather<Result>(m_faces_buf, index, active);
    }

    /// Returns the world-space position of the vertex with index \c index
    template <typename Index>
    MTS_INLINE auto vertex_position(Index index, mask_t<Index> active = true) const {
        using Result = Point<replace_scalar_t<Index, InputFloat>, 3>;
        return gather<Result>(m_vertex_positions_buf, index, active);
    }

    /// Returns the normal direction of the vertex with index \c index
    template <typename Index>
    MTS_INLINE auto vertex_normal(Index index, mask_t<Index> active = true) const {
        using Result = Normal<replace_scalar_t<Index, InputFloat>, 3>;
        return gather<Result>(m_vertex_normals_buf, index, active);
    }

    /// Returns the UV texture coordinates of the vertex with index \c index
    template <typename Index>
    MTS_INLINE auto vertex_texcoord(Index index, mask_t<Index> active = true) const {
        using Result = Point<replace_scalar_t<Index, InputFloat>, 2>;
        return gather<Result>(m_vertex_texcoords_buf, index, active);
    }

    /// Returns the surface area of the face with index \c index
    template <typename Index>
    auto face_area(Index index, mask_t<Index> active = true) const {
        auto fi = face_indices(index, active);

        auto p0 = vertex_position(fi[0], active),
             p1 = vertex_position(fi[1], active),
             p2 = vertex_position(fi[2], active);

        return 0.5f * norm(cross(p1 - p0, p2 - p0));
    }

    /// Does this mesh have per-vertex normals?
    bool has_vertex_normals() const { return slices(m_vertex_normals_buf) != 0; }

    /// Does this mesh have per-vertex texture coordinates?
    bool has_vertex_texcoords() const { return slices(m_vertex_texcoords_buf) != 0; }

    /// @}
    // =========================================================================

    /// Export mesh as a binary PLY file
    void write_ply(const std::string &filename) const;

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

    virtual Point3f
    barycentric_coordinates(const SurfaceInteraction3f &si,
                            Mask active = true) const;

    virtual SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                             PreliminaryIntersection3f pi,
                                                             HitComputeFlags flags,
                                                             Mask active = true) const override;

    virtual UnpolarizedSpectrum eval_attribute(const std::string &name,
                                               const SurfaceInteraction3f &si,
                                               Mask active = true) const override;
    virtual Float eval_attribute_1(const std::string& name,
                                   const SurfaceInteraction3f &si,
                                   Mask active = true) const override;
    virtual Color3f eval_attribute_3(const std::string& name,
                                     const SurfaceInteraction3f &si,
                                     Mask active = true) const override;

    virtual SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                                       Mask active = true) const override;

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
    MTS_INLINE PreliminaryIntersection3f
    ray_intersect_triangle(const UInt32 &index, const Ray3f &ray,
                           Mask active = true) const {
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

        PreliminaryIntersection3f pi = zero<PreliminaryIntersection3f>();
        pi.t = select(active, t, math::Infinity<Float>);
        pi.prim_uv = Point2f(u, v);
        pi.prim_index = index;
        pi.shape = this;

        return pi;
    }

#if defined(MTS_ENABLE_EMBREE)
    /// Return the Embree version of this shape
    virtual RTCGeometry embree_geometry(RTCDevice device) override;
#endif

#if defined(MTS_ENABLE_OPTIX)
    using Base::m_optix_data_ptr;
    virtual void optix_prepare_geometry() override;
    virtual void optix_build_input(OptixBuildInput&) const override;
#endif

    /// @}
    // =========================================================================

    void traverse(TraversalCallback *callback) override;
    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override;
    bool parameters_grad_enabled() const override;

    /// Return a human-readable string representation of the shape contents.
    virtual std::string to_string() const override;

    size_t vertex_data_bytes() const;
    size_t face_data_bytes() const;

protected:
    Mesh(const Properties &);
    inline Mesh() {}
    virtual ~Mesh();

    /**
     * \brief Build internal tables for sampling uniformly wrt. area.
     *
     * Computes the surface area and sets up \c m_area_pmf
     * Thread-safe, since it uses a mutex.
     */
    void build_pmf();

    /**
     * \brief Initialize the \c m_parameterization field for mapping UV
     * coordinates to positions
     *
     * Internally, the function creates a nested scene to leverage optimized
     * ray tracing functionality in \ref eval_parameterization()
     */
    void build_parameterization();

    // Ensures that the sampling table are ready.
    ENOKI_INLINE void ensure_pmf_built() const {
        if (unlikely(m_area_pmf.empty()))
            const_cast<Mesh *>(this)->build_pmf();
    }

    MTS_DECLARE_CLASS()

protected:
    enum MeshAttributeType {
        Vertex, Face
    };

    struct MeshAttribute {
        size_t size;
        MeshAttributeType type;
        FloatStorage buf;
    };

    template <uint32_t Size, bool Raw>
    auto interpolate_attribute(MeshAttributeType type,
                               const FloatStorage &buf,
                               const SurfaceInteraction3f &si,
                               Mask active) const {
        using StorageType =
            std::conditional_t<Size == 1,
                               replace_scalar_t<Float, InputFloat>,
                               replace_scalar_t<Color3f, InputFloat>>;
        using ReturnType = std::conditional_t<Size == 1, Float, Color3f>;

        if (type == MeshAttributeType::Vertex) {
            auto fi = face_indices(si.prim_index, active);
            Point3f b = barycentric_coordinates(si, active);

            StorageType v0 = gather<StorageType>(buf, fi[0], active),
                        v1 = gather<StorageType>(buf, fi[1], active),
                        v2 = gather<StorageType>(buf, fi[2], active);

            // Barycentric interpolation
            if constexpr (is_spectral_v<Spectrum> && Size == 3 && !Raw) {
                // Evaluate spectral upsampling model from stored coefficients
                UnpolarizedSpectrum c0, c1, c2;

                // NOTE this code assumes that mesh attribute data represents
                // srgb2spec model coefficients and not RGB color values in spectral mode.
                c0 = srgb_model_eval<UnpolarizedSpectrum>(v0, si.wavelengths);
                c1 = srgb_model_eval<UnpolarizedSpectrum>(v1, si.wavelengths);
                c2 = srgb_model_eval<UnpolarizedSpectrum>(v2, si.wavelengths);

                return fmadd(c0, b[0], fmadd(c1, b[1], c2 * b[2]));
            } else {
                return (ReturnType) fmadd(v0, b[0], fmadd(v1, b[1], v2 * b[2]));
            }
        } else {
            StorageType v = gather<StorageType>(buf, si.prim_index, active);
            if constexpr (is_spectral_v<Spectrum> && Size == 3 && !Raw) {
                return srgb_model_eval<UnpolarizedSpectrum>(v, si.wavelengths);
            } else {
                return (ReturnType) v;
            }
        }
    }

protected:
    std::string m_name;
    ScalarBoundingBox3f m_bbox;

    ScalarSize m_vertex_count = 0;
    ScalarSize m_face_count = 0;

    FloatStorage m_vertex_positions_buf;
    FloatStorage m_vertex_normals_buf;
    FloatStorage m_vertex_texcoords_buf;

    DynamicBuffer<UInt32> m_faces_buf;

    std::unordered_map<std::string, MeshAttribute> m_mesh_attributes;

#if defined(MTS_ENABLE_OPTIX)
    void* m_vertex_buffer_ptr;
#endif

    /// Flag that can be set by the user to disable loading/computation of vertex normals
    bool m_disable_vertex_normals = false;

    /* Surface area distribution -- generated on demand when \ref
       prepare_area_pmf() is first called. */
    DiscreteDistribution<Float> m_area_pmf;
    tbb::spin_mutex m_mutex;

    /// Optional: used in eval_parameterization()
    ref<Scene<Float, Spectrum>> m_parameterization;
};

MTS_EXTERN_CLASS_RENDER(Mesh)
NAMESPACE_END(mitsuba)
