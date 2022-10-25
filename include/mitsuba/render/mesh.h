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
                   m_interior_medium, m_exterior_medium, m_is_instance)

    // Mesh is always stored in single precision
    using InputFloat = float;
    using InputPoint3f  = Point<InputFloat, 3>;
    using InputVector2f = Vector<InputFloat, 2>;
    using InputVector3f = Vector<InputFloat, 3>;
    using InputNormal3f = Normal<InputFloat, 3>;

    using FloatStorage = DynamicBuffer<dr::replace_scalar_t<Float, InputFloat>>;

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;

    /// Create a new mesh with the given vertex and face data structures
    Mesh(const std::string &name, ScalarSize vertex_count,
         ScalarSize face_count, const Properties &props = Properties(),
         bool has_vertex_normals = false, bool has_vertex_texcoords = false);

    /// Must be called at the end of the constructor of Mesh plugins
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

    /// Returns the face indices associated with triangle \c index
    template <typename Index>
    MI_INLINE auto face_indices(Index index,
                                dr::mask_t<Index> active = true) const {
        using Result = dr::Array<dr::uint32_array_t<Index>, 3>;
        return dr::gather<Result>(m_faces, index, active);
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

    /// Export mesh as a binary PLY file
    void write_ply(const std::string &filename) const;

    /// Merge two meshes into one
    ref<Mesh> merge(const Mesh *other) const;

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

    virtual Float surface_area() const override;

    virtual PositionSample3f sample_position(Float time, const Point2f &sample,
                                             Mask active = true) const override;

    virtual Float pdf_position(const PositionSample3f &ps, Mask active = true) const override;

    virtual Point3f
    barycentric_coordinates(const SurfaceInteraction3f &si,
                            Mask active = true) const;

    virtual SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                             const PreliminaryIntersection3f &pi,
                                                             uint32_t ray_flags,
                                                             uint32_t recursion_depth = 0,
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
                                                       uint32_t ray_flags = +RayFlags::All,
                                                       Mask active = true) const override;

    void set_scene(Scene<Float, Spectrum> *scene) { m_scene = scene; }

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
    virtual RTCGeometry embree_geometry(RTCDevice device) override;
#endif

#if defined(MI_ENABLE_CUDA)
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
