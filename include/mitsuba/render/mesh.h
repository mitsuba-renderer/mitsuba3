#pragma once

#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/core/struct.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/distr_1d.h>
#include <mitsuba/core/properties.h>
#include <unordered_map>
#include <mutex>
#include <drjit/dynamic.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Mesh : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_TYPES()
    MI_IMPORT_BASE(Shape, m_to_world, mark_dirty, m_emitter, m_sensor, m_bsdf,
                   m_interior_medium, m_exterior_medium, m_is_instance,
                   m_discontinuity_types, m_shape_type, m_initialized)

    // Mesh is always stored in single precision
    using InputFloat = float;
    using InputPoint3f  = Point<InputFloat, 3>;
    using InputVector2f = Vector<InputFloat, 2>;
    using InputVector3f = Vector<InputFloat, 3>;
    using InputNormal3f = Normal<InputFloat, 3>;

    using FloatStorage = DynamicBuffer<dr::replace_scalar_t<Float, InputFloat>>;

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using typename Base::Index;

    /** \brief Creates a zero-initialized mesh with the given vertex and face
     * counts
     *
     * The vertex and face buffers can be filled using the ``mi.traverse``
     * mechanism. When initializing these buffers through another method, an
     * explicit call to \ref initialize must be made once all buffers are
     * filled.
     */
    Mesh(const std::string &name, ScalarSize vertex_count,
         ScalarSize face_count, const Properties &props = Properties(),
         bool has_vertex_normals = false, bool has_vertex_texcoords = false);

    /// Destructor
    ~Mesh();

    /** \brief Must be called once at the end of the construction of a Mesh
     *
     * This method computes internal data structures and notifies the parent
     * sensor or emitter (if there is one) that this instance is their internal
     * shape.
     */
    void initialize() override;

    // =========================================================================
    //! @{ \name Accessors (vertices, faces, normals, etc)
    // =========================================================================

    /// Return the total number of vertices
    ScalarSize vertex_count() const { return m_vertex_count; }
    /// Return the total number of faces
    ScalarSize face_count() const { return m_face_count; }

    /// Return vertex positions buffer
    FloatStorage& vertex_positions_buffer() { return m_vertex_positions; }
    /// Const variant of \ref vertex_positions_buffer.
    const FloatStorage& vertex_positions_buffer() const { return m_vertex_positions; }

    /// Return vertex normals buffer
    FloatStorage& vertex_normals_buffer() { return m_vertex_normals; }
    /// Const variant of \ref vertex_normals_buffer.
    const FloatStorage& vertex_normals_buffer() const { return m_vertex_normals; }

    /// Return vertex texcoords buffer
    FloatStorage& vertex_texcoords_buffer() { return m_vertex_texcoords; }
    /// Const variant of \ref vertex_texcoords_buffer.
    const FloatStorage& vertex_texcoords_buffer() const { return m_vertex_texcoords; }

    /// Return face indices buffer
    DynamicBuffer<UInt32>& faces_buffer() { return m_faces; }
    /// Const variant of \ref faces_buffer.
    const DynamicBuffer<UInt32>& faces_buffer() const { return m_faces; }

    /// Return the mesh attribute associated with \c name
    FloatStorage& attribute_buffer(const std::string& name) {
        auto attribute = m_mesh_attributes.find(name);
        if (attribute == m_mesh_attributes.end())
            Throw("attribute_buffer(): attribute %s doesn't exist.", name.c_str());
        return attribute->second.buf;
    }

    /// Add an attribute buffer with the given \c name and \c dim
    void add_attribute(const std::string &name, size_t dim,
                       const std::vector<InputFloat> &buf);

    /// Returns the vertex indices associated with triangle \c index
    template <typename Index>
    MI_INLINE auto face_indices(Index index,
                                dr::mask_t<Index> active = true) const {
        using Result = dr::Array<dr::uint32_array_t<Index>, 3>;
        return dr::gather<Result>(m_faces, index, active);
    }

    /**
     * Returns the vertex indices associated with edge \c edge_index (0..2)
     * of triangle \c tri_index.
     */
    template <typename Index>
    MI_INLINE auto edge_indices(Index tri_index, Index edge_index,
                                dr::mask_t<Index> active = true) const {
        using UInt32 = dr::uint32_array_t<Index>;
        return dr::Array<UInt32, 2>(
            dr::gather<UInt32>(m_faces, 3 * tri_index + edge_index, active),
            dr::gather<UInt32>(m_faces, 3 * tri_index + (edge_index + 1) % 3, active)
        );
    }

    /// Returns the world-space position of the vertex with index \c index
    template <typename Index>
    MI_INLINE auto vertex_position(Index index,
                                   dr::mask_t<Index> active = true) const {
        using Result = Point<dr::replace_scalar_t<Index, InputFloat>, 3>;
        return dr::gather<Result>(m_vertex_positions, index, active);
    }

    /// Returns the normal direction of the vertex with index \c index
    template <typename Index>
    MI_INLINE auto vertex_normal(Index index,
                                 dr::mask_t<Index> active = true) const {
        using Result = Normal<dr::replace_scalar_t<Index, InputFloat>, 3>;
        return dr::gather<Result>(m_vertex_normals, index, active);
    }

    /// Returns the UV texture coordinates of the vertex with index \c index
    template <typename Index>
    MI_INLINE auto vertex_texcoord(Index index,
                                   dr::mask_t<Index> active = true) const {
        using Result = Point<dr::replace_scalar_t<Index, InputFloat>, 2>;
        return dr::gather<Result>(m_vertex_texcoords, index, active);
    }

    /// Returns the normal direction of the face with index \c index
    template <typename Index>
    MI_INLINE auto face_normal(Index index,
                               dr::mask_t<Index> active = true) const {
        Vector3u vertex_indices = face_indices(index, active);
        Vector3f v[3] = { vertex_position(vertex_indices[0], active),
                          vertex_position(vertex_indices[1], active),
                          vertex_position(vertex_indices[2], active) };

        return dr::normalize(dr::cross(v[1] - v[0], v[2] - v[0]));
    }

    /// Returns the opposite edge index associated with directed edge \c index
    template <typename Index>
    MI_INLINE auto opposite_dedge(Index index,
                                 dr::mask_t<Index> active = true) const {
        using Result = dr::uint32_array_t<Index>;
        return dr::gather<Result>(m_E2E, index, active);
    }

    /// Does this mesh have per-vertex normals?
    bool has_vertex_normals() const { return dr::width(m_vertex_normals) != 0; }

    /// Does this mesh have per-vertex texture coordinates?
    bool has_vertex_texcoords() const { return dr::width(m_vertex_texcoords) != 0; }

    /// Does this mesh have additional mesh attributes?
    bool has_mesh_attributes() const { return m_mesh_attributes.size() > 0; }

    /// Does this mesh use face normals?
    bool has_face_normals() const { return m_face_normals; }

    /// @}
    // =========================================================================

    /**
     * Write the mesh to a binary PLY file
     *
     * \param filename
     *    Target file path on disk
     */
    void write_ply(const std::string &filename) const;

    /**
     * Write the mesh encoded in binary PLY format to a stream
     *
     * \param stream
     *    Target stream that will receive the encoded output
     */
    void write_ply(Stream *stream) const;

    /// Merge two meshes into one
    ref<Mesh> merge(const Mesh *other) const;

    /// Compute smooth vertex normals and replace the current normal values
    void recompute_vertex_normals();

    /// Recompute the bounding box (e.g. after modifying the vertex positions)
    void recompute_bbox();

    /**
     * /brief Build directed edge data structure to efficiently access adjacent
     * edges.
     *
     * This is an implementation of the technique described in:
     * <tt>https://www.graphics.rwth-aachen.de/media/papers/directed.pdf</tt>.
     */
    void build_directed_edges();

    // =============================================================
    //! @{ \name Shape interface implementation
    // =============================================================

    ScalarBoundingBox3f bbox() const override;

    ScalarBoundingBox3f bbox(ScalarIndex index) const override;

    ScalarBoundingBox3f bbox(ScalarIndex index,
                             const ScalarBoundingBox3f &clip) const override;

    ScalarSize primitive_count() const override;

    Float surface_area() const override;

    PositionSample3f sample_position(Float time,
                                     const Point2f &sample,
                                     Mask active = true) const override;

    Float pdf_position(const PositionSample3f &ps, Mask active = true) const override;

    Point3f barycentric_coordinates(const SurfaceInteraction3f &si,
                                    Mask active = true) const;

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     uint32_t recursion_depth = 0,
                                                     Mask active = true) const override;

    Mask has_attribute(const std::string &name, Mask active = true) const override;

    UnpolarizedSpectrum eval_attribute(const std::string &name,
                                       const SurfaceInteraction3f &si,
                                       Mask active = true) const override;

    Float eval_attribute_1(const std::string &name,
                           const SurfaceInteraction3f &si,
                           Mask active = true) const override;

    Color3f eval_attribute_3(const std::string &name,
                             const SurfaceInteraction3f &si,
                             Mask active = true) const override;

    SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                               uint32_t ray_flags = +RayFlags::All,
                                               Mask active = true) const override;

    void set_scene(Scene<Float, Spectrum> *scene) { m_scene = scene; }

    SilhouetteSample3f sample_silhouette(const Point3f &sample,
                                         uint32_t flags,
                                         Mask active) const override;

    Point3f invert_silhouette_sample(const SilhouetteSample3f &ss,
                                     Mask active) const override;

    Point3f differential_motion(const SurfaceInteraction3f &si,
                                Mask active = true) const override;

    SilhouetteSample3f primitive_silhouette_projection(const Point3f &viewpoint,
                                                       const SurfaceInteraction3f &si,
                                                       uint32_t flags,
                                                       Float sample,
                                                       Mask active = true) const override;

    std::tuple<DynamicBuffer<UInt32>, DynamicBuffer<Float>>
    precompute_silhouette(const ScalarPoint3f &viewpoint) const override;

    SilhouetteSample3f sample_precomputed_silhouette(const Point3f &viewpoint,
                                                     Index sample1,
                                                     Float sample2,
                                                     Mask active = true) const override;

    //! @}
    // =============================================================


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
    template <typename T, typename Ray3>
    std::pair<T, Point<T, 2>>
    ray_intersect_triangle_impl(const dr::uint32_array_t<T> &index,
                                const Ray3 &ray,
                                dr::mask_t<T> active = true) const {
        using Point3T = Point<T, 3>;
        using Faces = dr::Array<dr::uint32_array_t<T>, 3>;

        Faces fi;
        Point3T p0, p1, p2;
#if defined(MI_ENABLE_LLVM) && !defined(MI_ENABLE_EMBREE)
        // Ensure we don't rely on drjit-core when called from an LLVM kernel
        if constexpr (!dr::is_array_v<T> && dr::is_llvm_v<Float>) {
            fi = dr::gather<Faces>(m_faces_ptr, index, active);
            p0 = dr::gather<InputPoint3f>(m_vertex_positions_ptr, fi[0], active),
            p1 = dr::gather<InputPoint3f>(m_vertex_positions_ptr, fi[1], active),
            p2 = dr::gather<InputPoint3f>(m_vertex_positions_ptr, fi[2], active);
        } else
#endif
        {
            fi = face_indices(index, active);
            p0 = vertex_position(fi[0], active),
            p1 = vertex_position(fi[1], active),
            p2 = vertex_position(fi[2], active);
        }

        auto [t, uv, hit] = moeller_trumbore(ray, p0, p1, p2, active);
        return { dr::select(hit, t, dr::Infinity<T>), uv };
    }

    MI_INLINE PreliminaryIntersection3f
    ray_intersect_triangle(const UInt32 &index, const Ray3f &ray,
                           Mask active = true) const {
        PreliminaryIntersection3f pi = dr::zeros<PreliminaryIntersection3f>();
        std::tie(pi.t, pi.prim_uv) = ray_intersect_triangle_impl<Float>(index, ray, active);
        pi.prim_index = index;
        pi.shape = this;
        return pi;
    }

    using ScalarRay3f = Ray<ScalarPoint3f, Spectrum>;
    MI_INLINE std::pair<ScalarFloat, ScalarPoint2f>
    ray_intersect_triangle_scalar(const ScalarUInt32 &index, const ScalarRay3f &ray) const {
        return ray_intersect_triangle_impl<ScalarFloat>(index, ray, true);
    }

#define MI_DECLARE_RAY_INTERSECT_TRI_PACKET(N)                            \
    using FloatP##N   = dr::Packet<dr::scalar_t<Float>, N>;                \
    using MaskP##N    = dr::mask_t<FloatP##N>;                             \
    using UInt32P##N  = dr::uint32_array_t<FloatP##N>;                     \
    using Point2fP##N = Point<FloatP##N, 2>;                               \
    using Point3fP##N = Point<FloatP##N, 3>;                               \
    using Ray3fP##N   = Ray<Point3fP##N, Spectrum>;                        \
    virtual std::pair<FloatP##N, Point2fP##N>                              \
    ray_intersect_triangle_packet(const UInt32P##N &index,                 \
                                  const Ray3fP##N &ray,                    \
                                  MaskP##N active = true) const {          \
        return ray_intersect_triangle_impl<FloatP##N>(index, ray, active); \
    }

    MI_DECLARE_RAY_INTERSECT_TRI_PACKET(4)
    MI_DECLARE_RAY_INTERSECT_TRI_PACKET(8)
    MI_DECLARE_RAY_INTERSECT_TRI_PACKET(16)

#if defined(MI_ENABLE_EMBREE)
    /// Return the Embree version of this shape
    RTCGeometry embree_geometry(RTCDevice device) override;
#endif

#if defined(MI_ENABLE_CUDA)
    using Base::m_optix_data_ptr;
    void optix_prepare_geometry() override;
    void optix_build_input(OptixBuildInput&) const override;
#endif

    /// @}
    // =========================================================================

    void traverse(TraversalCallback *callback) override;
    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override;
    bool parameters_grad_enabled() const override;

    /// Return a human-readable string representation of the shape contents.
    std::string to_string() const override;

    size_t vertex_data_bytes() const;
    size_t face_data_bytes() const;

protected:
    Mesh(const Properties &);
    inline Mesh() {}

    /**
     * \brief Build internal tables for sampling uniformly wrt. area.
     *
     * Computes the surface area and sets up \c m_area_pmf
     * Thread-safe, since it uses a mutex.
     */
    void build_pmf();

    /**
     * /brief Precompute the set of edges that could contribute to the indirect
     * discontinuous integral.
     *
     * This method filters out any concave edges or flat surfaces.
     *
     * Internally, this method relies on the directed edge data structure. A
     * call to \ref build_directed_edges before a call to this method is
     * therefore necessary.
     */
    void build_indirect_silhouette_distribution();

    /**
     * \brief Initialize the \c m_parameterization field for mapping UV
     * coordinates to positions
     *
     * Internally, the function creates a nested scene to leverage optimized
     * ray tracing functionality in \ref eval_parameterization()
     */
    void build_parameterization();

    // Ensures that the sampling table are ready.
    DRJIT_INLINE void ensure_pmf_built() const {
        if (unlikely(m_area_pmf.empty()))
            const_cast<Mesh *>(this)->build_pmf();
    }

    /** \brief Moeller and Trumbore algorithm for computing ray-triangle
     * intersection
     *
     * Discussed at
     * <tt>http://www.acm.org/jgt/papers/MollerTrumbore97/code.html</tt>.
     *
     * \param ray
     *    The ray segment to be used for the intersection query.
     * \param p0
     *    First vertex position of the triangle
     * \param p1
     *    Second vertex position of the triangle
     * \param p2
     *    Third vertex position of the triangle
     * \return
     *    Returns an ordered tuple <tt>(mask, u, v, t)</tt>, where \c mask
     *    indicates whether an intersection was found, \c t contains the
     *    distance from the ray origin to the intersection point, and \c u and
     *    \c v contains the first two components of the intersection in
     *    barycentric coordinates
     */
    template <typename T, typename Ray3>
    std::tuple<T, Point<T, 2>, dr::mask_t<T>>
    moeller_trumbore(const Ray3 &ray, const Point<T, 3> &p0,
                     const Point<T, 3> &p1, const Point<T, 3> &p2,
                     dr::mask_t<T> active = true) const {
        using Vector3T = Vector<T, 3>;
        Vector3T e1 = p1 - p0, e2 = p2 - p0;

        Vector3T pvec = dr::cross(ray.d, e2);
        T inv_det = dr::rcp(dr::dot(e1, pvec));

        Vector3T tvec = ray.o - p0;
        T u = dr::dot(tvec, pvec) * inv_det;
        active &= u >= 0.f && u <= 1.f;

        Vector3T qvec = dr::cross(tvec, e1);
        T v = dr::dot(ray.d, qvec) * inv_det;
        active &= v >= 0.f && u + v <= 1.f;

        T t = dr::dot(e2, qvec) * inv_det;
        active &= t >= 0.f && t <= ray.maxt;

        return { t, { u, v }, active };
    }

    MI_DECLARE_CLASS()

protected:
    enum MeshAttributeType {
        Vertex, Face
    };

    struct MeshAttribute {
        size_t size;
        MeshAttributeType type;
        mutable FloatStorage buf;

        MeshAttribute migrate(AllocType at) const {
            return MeshAttribute { size, type, dr::migrate(buf, at) };
        }
    };

    template <uint32_t Size, bool Raw>
    auto interpolate_attribute(MeshAttributeType type,
                               const FloatStorage &buf,
                               const SurfaceInteraction3f &si,
                               Mask active) const {
        using StorageType =
            std::conditional_t<Size == 1,
                               dr::replace_scalar_t<Float, InputFloat>,
                               dr::replace_scalar_t<Color3f, InputFloat>>;
        using ReturnType = std::conditional_t<Size == 1, Float, Color3f>;

        if (type == MeshAttributeType::Vertex) {
            auto fi = face_indices(si.prim_index, active);
            Point3f b = barycentric_coordinates(si, active);

            StorageType v0 = dr::gather<StorageType>(buf, fi[0], active),
                        v1 = dr::gather<StorageType>(buf, fi[1], active),
                        v2 = dr::gather<StorageType>(buf, fi[2], active);

            // Barycentric interpolation
            if constexpr (is_spectral_v<Spectrum> && Size == 3 && !Raw) {
                // Evaluate spectral upsampling model from stored coefficients
                UnpolarizedSpectrum c0, c1, c2;

                // NOTE this code assumes that mesh attribute data represents
                // srgb2spec model coefficients and not RGB color values in spectral mode.
                c0 = srgb_model_eval<UnpolarizedSpectrum>(v0, si.wavelengths);
                c1 = srgb_model_eval<UnpolarizedSpectrum>(v1, si.wavelengths);
                c2 = srgb_model_eval<UnpolarizedSpectrum>(v2, si.wavelengths);

                return dr::fmadd(c0, b[0], dr::fmadd(c1, b[1], c2 * b[2]));
            } else {
                return (ReturnType) dr::fmadd(v0, b[0], dr::fmadd(v1, b[1], v2 * b[2]));
            }
        } else {
            StorageType v = dr::gather<StorageType>(buf, si.prim_index, active);
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

    mutable FloatStorage m_vertex_positions;
    mutable FloatStorage m_vertex_normals;
    mutable FloatStorage m_vertex_texcoords;

    mutable DynamicBuffer<UInt32> m_faces;

    /// Directed edges data structures to support neighbor queries
    mutable DynamicBuffer<UInt32> m_E2E;
    bool m_E2E_outdated = true;


    constexpr static ScalarIndex m_invalid_dedge = (ScalarIndex) -1;

    /// Sampling density of silhouette (\ref build_indirect_silhouette_distribution)
    DiscreteDistribution<Float> m_sil_dedge_pmf;

#if defined(MI_ENABLE_LLVM) && !defined(MI_ENABLE_EMBREE)
    /* Data pointer to ensure triangle intersection routine doesn't rely on
       drjit-core when called from an LLVM kernel */
    float* m_vertex_positions_ptr;
    uint32_t* m_faces_ptr;
#endif

    std::unordered_map<std::string, MeshAttribute> m_mesh_attributes;

#if defined(MI_ENABLE_CUDA)
    mutable void* m_vertex_buffer_ptr = nullptr;
#endif

    /// Flag that can be set by the user to disable loading/computation of vertex normals
    bool m_face_normals = false;
    bool m_flip_normals = false;

    /* Surface area distribution -- generated on demand when \ref
       prepare_area_pmf() is first called. */
    DiscreteDistribution<Float> m_area_pmf;
    std::mutex m_mutex;

    /// Optional: used in eval_parameterization()
    ref<Scene<Float, Spectrum>> m_parameterization;

    /// Pointer to the scene that owns this mesh
    Scene<Float, Spectrum>* m_scene = nullptr;
};

MI_EXTERN_CLASS(Mesh)
NAMESPACE_END(mitsuba)


// -----------------------------------------------------------------------
//! @{ \name Dr.Jit support for vectorized function calls
// -----------------------------------------------------------------------

DRJIT_CALL_TEMPLATE_INHERITED_BEGIN(mitsuba::Mesh, mitsuba::Shape)
    DRJIT_CALL_METHOD(face_indices)
    DRJIT_CALL_METHOD(edge_indices)
    DRJIT_CALL_METHOD(vertex_position)
    DRJIT_CALL_METHOD(vertex_normal)
    DRJIT_CALL_METHOD(vertex_texcoord)
    DRJIT_CALL_METHOD(face_normal)
    DRJIT_CALL_METHOD(opposite_dedge)
    DRJIT_CALL_METHOD(ray_intersect_triangle)

    DRJIT_CALL_GETTER(vertex_count)
    DRJIT_CALL_GETTER(face_count)
    DRJIT_CALL_GETTER(has_vertex_normals)
    DRJIT_CALL_GETTER(has_vertex_texcoords)
    DRJIT_CALL_GETTER(has_mesh_attributes)
    DRJIT_CALL_GETTER(has_face_normals)
DRJIT_CALL_INHERITED_END(mitsuba::Mesh)

//! @}
// -----------------------------------------------------------------------
