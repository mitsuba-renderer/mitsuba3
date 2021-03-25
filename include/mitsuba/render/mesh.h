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
#include <enoki/dynamic.h>

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

    using FloatStorage = DynamicBuffer<ek::replace_scalar_t<Float, InputFloat>>;

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
    MTS_INLINE auto face_indices(Index index,
                                 ek::mask_t<Index> active = true) const {
        using Result = ek::Array<ek::uint32_array_t<Index>, 3>;
        return ek::gather<Result>(m_faces, index, active);
    }

    /// Returns the world-space position of the vertex with index \c index
    template <typename Index>
    MTS_INLINE auto vertex_position(Index index,
                                    ek::mask_t<Index> active = true) const {
        using Result = Point<ek::replace_scalar_t<Index, InputFloat>, 3>;
        return ek::gather<Result>(m_vertex_positions, index, active);
    }

    /// Returns the normal direction of the vertex with index \c index
    template <typename Index>
    MTS_INLINE auto vertex_normal(Index index,
                                  ek::mask_t<Index> active = true) const {
        using Result = Normal<ek::replace_scalar_t<Index, InputFloat>, 3>;
        return ek::gather<Result>(m_vertex_normals, index, active);
    }

    /// Returns the UV texture coordinates of the vertex with index \c index
    template <typename Index>
    MTS_INLINE auto vertex_texcoord(Index index,
                                    ek::mask_t<Index> active = true) const {
        using Result = Point<ek::replace_scalar_t<Index, InputFloat>, 2>;
        return ek::gather<Result>(m_vertex_texcoords, index, active);
    }

    /// Does this mesh have per-vertex normals?
    bool has_vertex_normals() const { return ek::width(m_vertex_normals) != 0; }

    /// Does this mesh have per-vertex texture coordinates?
    bool has_vertex_texcoords() const { return ek::width(m_vertex_texcoords) != 0; }

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
                                                             uint32_t hit_flags,
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
                                                       uint32_t hit_flags = +HitComputeFlags::All,
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
    template <typename T, typename Ray3>
    std::pair<T, Point<T, 2>>
    ray_intersect_triangle_impl(const ek::uint32_array_t<T> &index,
                                const Ray3 &ray,
                                ek::mask_t<T> active = true) const {
        using Point3T  = Point<T, 3>;
        using Vector3T = Vector<T, 3>;

        auto fi = face_indices(index, active);

        Point3T p0 = vertex_position(fi[0], active),
                p1 = vertex_position(fi[1], active),
                p2 = vertex_position(fi[2], active);

        Vector3T e1 = p1 - p0, e2 = p2 - p0;

        Vector3T pvec = ek::cross(ray.d, e2);
        T inv_det = ek::rcp(ek::dot(e1, pvec));

        Vector3T tvec = ray.o - p0;
        T u = ek::dot(tvec, pvec) * inv_det;
        active &= u >= 0.f && u <= 1.f;

        Vector3T qvec = ek::cross(tvec, e1);
        T v = ek::dot(ray.d, qvec) * inv_det;
        active &= v >= 0.f && u + v <= 1.f;

        T t = ek::dot(e2, qvec) * inv_det;
        active &= t >= ray.mint && t <= ray.maxt;

        return { ek::select(active, t, ek::Infinity<T>), { u, v } };
    }

    MTS_INLINE PreliminaryIntersection3f
    ray_intersect_triangle(const UInt32 &index, const Ray3f &ray,
                           Mask active = true) const {
        PreliminaryIntersection3f pi = ek::zero<PreliminaryIntersection3f>();
        std::tie(pi.t, pi.prim_uv) = ray_intersect_triangle_impl<Float>(index, ray, active);
        pi.prim_index = index;
        pi.shape = this;
        return pi;
    }

    using ScalarRay3f = Ray<ScalarPoint3f, Spectrum>;
    MTS_INLINE std::pair<ScalarFloat, ScalarPoint2f>
    ray_intersect_triangle_scalar(const ScalarUInt32 &index, const ScalarRay3f &ray) const {
        return ray_intersect_triangle_impl<ScalarFloat>(index, ray, true);
    }

#define MTS_DECLARE_RAY_INTERSECT_TRI_PACKET(N)                            \
    using FloatP##N   = ek::Packet<ek::scalar_t<Float>, N>;                \
    using MaskP##N    = ek::mask_t<FloatP##N>;                             \
    using UInt32P##N  = ek::uint32_array_t<FloatP##N>;                     \
    using Point2fP##N = Point<FloatP##N, 2>;                               \
    using Point3fP##N = Point<FloatP##N, 3>;                               \
    using Ray3fP##N   = Ray<Point3fP##N, Spectrum>;                        \
    virtual std::pair<FloatP##N, Point2fP##N>                              \
    ray_intersect_triangle_packet(const UInt32P##N &index,                 \
                                  const Ray3fP##N &ray,                    \
                                  MaskP##N active = true) const {          \
        return ray_intersect_triangle_impl<FloatP##N>(index, ray, active); \
    }

    MTS_DECLARE_RAY_INTERSECT_TRI_PACKET(4)
    MTS_DECLARE_RAY_INTERSECT_TRI_PACKET(8)
    MTS_DECLARE_RAY_INTERSECT_TRI_PACKET(16)

#if defined(MTS_ENABLE_EMBREE)
    /// Return the Embree version of this shape
    virtual RTCGeometry embree_geometry(RTCDevice device) override;
#endif

#if defined(MTS_ENABLE_CUDA)
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

    virtual void set_grad_suspended(bool state) override;

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
        mutable FloatStorage buf;

        MeshAttribute migrate(AllocType at) const {
            return MeshAttribute { size, type, ek::migrate(buf, at) };
        }
    };

    template <uint32_t Size, bool Raw>
    auto interpolate_attribute(MeshAttributeType type,
                               const FloatStorage &buf,
                               const SurfaceInteraction3f &si,
                               Mask active) const {
        using StorageType =
            std::conditional_t<Size == 1,
                               ek::replace_scalar_t<Float, InputFloat>,
                               ek::replace_scalar_t<Color3f, InputFloat>>;
        using ReturnType = std::conditional_t<Size == 1, Float, Color3f>;

        if (type == MeshAttributeType::Vertex) {
            auto fi = face_indices(si.prim_index, active);
            Point3f b = barycentric_coordinates(si, active);

            StorageType v0 = ek::gather<StorageType>(buf, fi[0], active),
                        v1 = ek::gather<StorageType>(buf, fi[1], active),
                        v2 = ek::gather<StorageType>(buf, fi[2], active);

            // Barycentric interpolation
            if constexpr (is_spectral_v<Spectrum> && Size == 3 && !Raw) {
                // Evaluate spectral upsampling model from stored coefficients
                UnpolarizedSpectrum c0, c1, c2;

                // NOTE this code assumes that mesh attribute data represents
                // srgb2spec model coefficients and not RGB color values in spectral mode.
                c0 = srgb_model_eval<UnpolarizedSpectrum>(v0, si.wavelengths);
                c1 = srgb_model_eval<UnpolarizedSpectrum>(v1, si.wavelengths);
                c2 = srgb_model_eval<UnpolarizedSpectrum>(v2, si.wavelengths);

                return ek::fmadd(c0, b[0], ek::fmadd(c1, b[1], c2 * b[2]));
            } else {
                return (ReturnType) ek::fmadd(v0, b[0], ek::fmadd(v1, b[1], v2 * b[2]));
            }
        } else {
            StorageType v = ek::gather<StorageType>(buf, si.prim_index, active);
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

    std::unordered_map<std::string, MeshAttribute> m_mesh_attributes;

#if defined(MTS_ENABLE_CUDA)
    mutable void* m_vertex_buffer_ptr = nullptr;
#endif

    /// Flag that can be set by the user to disable loading/computation of vertex normals
    bool m_disable_vertex_normals = false;

    /* Surface area distribution -- generated on demand when \ref
       prepare_area_pmf() is first called. */
    DiscreteDistribution<Float> m_area_pmf;
    std::mutex m_mutex;

    /// Optional: used in eval_parameterization()
    ref<Scene<Float, Spectrum>> m_parameterization;
};

MTS_EXTERN_CLASS_RENDER(Mesh)
NAMESPACE_END(mitsuba)
