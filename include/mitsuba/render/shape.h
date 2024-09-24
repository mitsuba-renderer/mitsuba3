#pragma once

#include <drjit/call.h>
#include <mitsuba/render/records.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/field.h>
#include <drjit/packet.h>
#include <unordered_map>

#if defined(MI_ENABLE_CUDA)
#  include <mitsuba/render/optix/common.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/// Enumeration of all shape types in Mitsuba
enum class ShapeType : uint32_t {
    /// Meshes (`ply`, `obj`, `serialized`)
    Mesh = 0u,
    /// B-Spline curves (`bsplinecurve`)
    BSplineCurve = 1u,
    /// Cylinders (`cylinder`)
    Cylinder = 2u,
    /// Disks (`disk`)
    Disk = 3u,
    /// Linear curves (`linearcurve`)
    LinearCurve = 4u,
    /// Rectangles (`rectangle`)
    Rectangle = 5u,
    /// SDF Grids (`sdfgrid`)
    SDFGrid = 6u,
    /// Spheres (`sphere`)
    Sphere = 7u,
    /// Instance (`instance`)
    Instance = 8u,
    /// Other shapes
    Other = 9u
};
MI_DECLARE_ENUM_OPERATORS(ShapeType)

/**
 * \brief This list of flags is used to control the behavior of discontinuity
 * related routines.
 */
enum class DiscontinuityFlags : uint32_t {
    // =============================================================
    //!                   Discontinuity types
    // =============================================================

    /// No flags set (default value)
    Empty = 0x0,

    /// Open boundary or jumping normal type of discontinuity
    PerimeterType = 0x1,

    /// Smooth normal type of discontinuity
    InteriorType = 0x2,

    // =============================================================
    //!              Encoding and projection flags
    // =============================================================

    /* \brief Use spherical lune to encode segment direction
     *
     * This flag is only relevant for certain shape types.
     */
    DirectionLune = 0x4,

    /* \brief Use spherical coordinates to encode segment direction
     *
     * This flag is only relevant for certain shape types.
     */
    DirectionSphere = 0x8,

    /* \brief Project to an edge using a heuristic probability
     *
     * This flag only applies to triangle meshes.
     *
     * By default a projection operation on a mesh triangle would uniformly pick
     * one of its three edges. This flag modifies that operation such that each
     * edge is weighted according to the angle it forms between the two adjadent
     * faces.
     */
    HeuristicWalk = 0x10,

    // =============================================================
    //!                 Compound types
    // =============================================================

    /// All types of discontinuities
    AllTypes = PerimeterType | InteriorType
};
MI_DECLARE_ENUM_OPERATORS(DiscontinuityFlags)

/// Forward declaration for `SilhouetteSample`
template <typename Float, typename Spectrum> class Shape;

/**
 * \brief Data structure holding the result of visibility silhouette sampling
 * operations on geometry.
 */
template <typename Float_, typename Spectrum_>
struct SilhouetteSample : public PositionSample<Float_, Spectrum_> {
    // =============================================================
    //! @{ \name Type declarations
    // =============================================================
    using Float    = Float_;
    using Spectrum = Spectrum_;

    MI_IMPORT_BASE(PositionSample, p, n, uv, time, pdf, delta)

    MI_IMPORT_RENDER_BASIC_TYPES()
    MI_IMPORT_OBJECT_TYPES()

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Type of discontinuity (\ref DiscontinuityFlags)
    UInt32 discontinuity_type;

    /// Direction of the boundary segment sample
    Vector3f d;

    /// Direction of the silhouette curve at the boundary point
    Vector3f silhouette_d;

    /// Primitive index, e.g. the triangle ID (if applicable)
    UInt32 prim_index;

    /// Index of the shape in the scene (if applicable)
    UInt32 scene_index;

    /// The set of \c DiscontinuityFlags that were used to generate this sample
    UInt32 flags;

    /**
     * \brief Projection index indicator
     *
     * For primitives like triangle meshes, a boundary segment is defined not
     * only by the triangle index but also the edge index of the selected
     * triangle. A value larger than 3 indicates a failed projection. For other
     * primitives, zero indicates a failed projection.
     *
     * For triangle meshes, index 0 stands for the directed edge p0->p1 (not the
     * opposite edge p1->p2), index 1 stands for the edge p1->p2, and index 2
     * for p2->p0.
     */
    UInt32 projection_index;

    /// Pointer to the associated shape
    ShapePtr shape = nullptr;

    /**
     * \brief Local-form boundary foreshortening term.
     *
     * It stores `sin_phi_B` for perimeter silhouettes or the normal curvature
     * for interior silhouettes.
     */
    Float foreshortening;

    /**
     * \brief Offset along the boundary segment direction (`d`) to avoid
     * self-intersections.
     */
    Float offset;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Methods
    // =============================================================

    /// Partially initialize a boundary segment from a position sample
    SilhouetteSample(const PositionSample<Float, Spectrum> &ps)
        : Base(ps), discontinuity_type((uint32_t) DiscontinuityFlags::Empty),
          d(0), silhouette_d(0), prim_index(0), scene_index(0), flags(0),
          projection_index(0), shape(nullptr), foreshortening(0), offset(0) {}

    /// Is the current boundary segment valid=
    Mask is_valid() const {
        return discontinuity_type != (uint32_t) DiscontinuityFlags::Empty;
    }

    /**
     * \brief Spawn a ray on the silhouette point in the direction of \ref d
     *
     * The ray origin is offset in the direction of the segment (\ref d) aswell
     * as in the in the direction of the silhouette normal (\ref n). Without this
     * offsetting, during a ray intersection, the ray could potentially find
     * an intersection point at its origin due to numerical instabilities in
     * the intersection routines.
     */
    Ray3f spawn_ray() const {
        Vector3f o_offset = (1 + dr::max(dr::abs(p))) *
                            (d * offset + n * math::ShapeEpsilon<Float>);
        return Ray3f(p + o_offset, d);
    }

    //! @}
    // =============================================================

    DRJIT_STRUCT(SilhouetteSample, p, n, uv, time, pdf, delta,
                 discontinuity_type, d, silhouette_d, prim_index, scene_index,
                 flags, projection_index, shape, foreshortening, offset)
};

/**
 * \brief Base class of all geometric shapes in Mitsuba
 *
 * This class provides core functionality for sampling positions on surfaces,
 * computing ray intersections, and bounding shapes within ray intersection
 * acceleration data structures.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Shape : public Object {
public:
    MI_IMPORT_TYPES(BSDF, Medium, Emitter, Sensor, MeshAttribute, Texture)

    // Use 32 bit indices to keep track of indices to conserve memory
    using ScalarIndex = uint32_t;
    using ScalarSize  = uint32_t;
    using Index = UInt32;
    using ScalarRay3f = Ray<ScalarPoint3f, Spectrum>;

    /// Destructor
    ~Shape();

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    /**
     * \brief Sample a point on the surface of this shape
     *
     * The sampling strategy is ideally uniform over the surface, though
     * implementations are allowed to deviate from a perfectly uniform
     * distribution as long as this is reflected in the returned probability
     * density.
     *
     * \param time
     *     The scene time associated with the position sample
     *
     * \param sample
     *     A uniformly distributed 2D point on the domain <tt>[0,1]^2</tt>
     *
     * \return
     *     A \ref PositionSample instance describing the generated sample
     */
    virtual PositionSample3f sample_position(Float time, const Point2f &sample,
                                             Mask active = true) const;

    /**
     * \brief Query the probability density of \ref sample_position() for
     * a particular point on the surface.
     *
     * \param ps
     *     A position record describing the sample in question
     *
     * \return
     *     The probability density per unit area
     */
    virtual Float pdf_position(const PositionSample3f &ps, Mask active = true) const;

    /**
     * \brief Sample a direction towards this shape with respect to solid
     * angles measured at a reference position within the scene
     *
     * An ideal implementation of this interface would achieve a uniform solid
     * angle density within the surface region that is visible from the
     * reference position <tt>it.p</tt> (though such an ideal implementation
     * is usually neither feasible nor advisable due to poor efficiency).
     *
     * The function returns the sampled position and the inverse probability
     * per unit solid angle associated with the sample.
     *
     * When the Shape subclass does not supply a custom implementation of this
     * function, the \ref Shape class reverts to a fallback approach that
     * piggybacks on \ref sample_position(). This will generally lead to a
     * suboptimal sample placement and higher variance in Monte Carlo
     * estimators using the samples.
     *
     * \param it
     *    A reference position somewhere within the scene.
     *
     * \param sample
     *     A uniformly distributed 2D point on the domain <tt>[0,1]^2</tt>
     *
     * \return
     *     A \ref DirectionSample instance describing the generated sample
     */
    virtual DirectionSample3f sample_direction(const Interaction3f &it, const Point2f &sample,
                                               Mask active = true) const;

    /**
     * \brief Query the probability density of \ref sample_direction()
     *
     * \param it
     *    A reference position somewhere within the scene.
     *
     * \param ps
     *     A position record describing the sample in question
     *
     * \return
     *     The probability density per unit solid angle
     */
    virtual Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                                Mask active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Silhouette sampling routines and other utilities
    // =============================================================

    // Return the silhouette discontinuity type(s) of this shape
    uint32_t silhouette_discontinuity_types() const {
        return m_discontinuity_types;
    }

    /// Return this shape's sampling weight w.r.t. all shapes in the scene
    ScalarFloat silhouette_sampling_weight() const {
        return m_silhouette_sampling_weight;
    }

    /**
     * \brief Map a point sample in boundary sample space to a silhouette
     * segment
     *
     * This method's behavior is undefined when used in non-JIT variants or
     * when the shape is not being differentiated.
     *
     * \param sample
     *      The boundary space sample (a point in the unit cube).
     *
     * \param flags
     *      Flags to select the type of silhouettes to sample
     *      from (see \ref DiscontinuityFlags).
     *      Only one type of discontinuity can be sampled per call.
     *
     * \return
     *     Silhouette sample record.
     */
    virtual SilhouetteSample3f sample_silhouette(const Point3f &sample,
                                                 uint32_t flags,
                                                 Mask active = true) const;

    /**
     * \brief Map a silhouette segment to a point in boundary sample space
     *
     * This method is the inverse of \ref sample_silhouette(). The mapping
     * from/to boundary sample space to/from boundary segments is bijective.
     *
     * This method's behavior is undefined when used in non-JIT variants or
     * when the shape is not being differentiated.
     *
     * \param ss
     *      The sampled boundary segment
     *
     * \return
     *     The corresponding boundary sample space point
     */
    virtual Point3f invert_silhouette_sample(const SilhouetteSample3f &ss,
                                             Mask active = true) const;

    /**
     * \brief Return the attached (AD) point on the shape's surface
     *
     * This method is only useful when using automatic differentiation. The
     * immediate/primal return value of this method is exactly equal to
     * \`si.p\`.
     *
     * The input `si` does not need to be explicitly detached, it is done by the
     * method itself.
     *
     * If the shape cannot be differentiated, this method will return the
     * detached input point.
     *
     * note:: The returned attached point is exactly the same as a point which
     * is computed by calling \ref compute_surface_interaction with the
     * \ref RayFlags::FollowShape flag.
     *
     * \param si
     *      The surface point for which the function will be evaluated.
     *
     *      Not all fields of the object need to be filled. Only the
     *      `prim_index`, `p` and `uv` fields are required. Certain shapes will
     *      only use a subset of these.
     *
     * \return
     *      The same surface point as the input but attached (AD) to the shape's
     *      parameters.
     */
    virtual Point3f differential_motion(const SurfaceInteraction3f &si,
                                        Mask active = true) const;

    /**
     * \brief Projects a point on the surface of the shape to its silhouette
     * as seen from a specified viewpoint.
     *
     * This method only projects the `si.p` point within its primitive.
     *
     * Not all of the fields of the \ref SilhouetteSample3f might be filled by
     * this method. Each shape will at the very least fill its return value with
     * enough information for it to be used by \ref invert_silhouette_sample.
     *
     * The projection operation might not find the closest silhouette point to
     * the given surface point. For example, it can be guided by a random number
     * \c sample. Not all shapes types need this random number, each shape
     * implementation is free to define its own algorithm and guarantees about
     * the projection operation.
     *
     * This method's behavior is undefined when used in non-JIT variants or
     * when the shape is not being differentiated.
     *
     * \param viewpoint
     *      The viewpoint which defines the silhouette to project the point to.
     *
     * \param si
     *      The surface point which will be projected.
     *
     * \param flags
     *      Flags to select the type of \ref SilhouetteSample3f to generate from
     *      the projection. Only one type of discontinuity can be used per call.
     *
     * \param sample
     *      A random number that can be used to define the projection operation.
     *
     * \return
     *      A boundary segment on the silhouette of the shape as seen from
     *      \c viewpoint.
     */
    virtual SilhouetteSample3f primitive_silhouette_projection(const Point3f &viewpoint,
                                                               const SurfaceInteraction3f &si,
                                                               uint32_t flags,
                                                               Float sample,
                                                               Mask active = true) const;

    /**
     * \brief Precompute the visible silhouette of this shape for a given
     * viewpoint.
     *
     * This method is meant to be used for silhouettes that are shared between
     * all threads, as is the case for primarily visible derivatives.
     *
     * The return values are respectively a list of indices and their
     * corresponding weights. The semantic meaning of these indices is different
     * for each shape. For example, a triangle mesh will return the indices
     * of all of its edges that constitute its silhouette. These indices are
     * meant to be re-used as an argument when calling
     * \ref sample_precomputed_silhouette.
     *
     * This method's behavior is undefined when used in non-JIT variants or
     * when the shape is not being differentiated.
     *
     * \param viewpoint
     *      The viewpoint which defines the silhouette of the shape
     *
     * \return
     *      A list of indices used by the shape internally to represent
     *      silhouettes, and a list of the same length containing the
     *      (unnormalized) weights associated to each index.
     */
    virtual std::tuple<DynamicBuffer<UInt32>, DynamicBuffer<Float>>
    precompute_silhouette(const ScalarPoint3f &viewpoint) const;

    /**
     * \brief Samples a boundary segement on the shape's silhouette using
     * precomputed information computed in \ref precompute_silhouette.
     *
     * This method is meant to be used for silhouettes that are shared between
     * all threads, as is the case for primarily visible derivatives.
     *
     * This method's behavior is undefined when used in non-JIT variants or
     * when the shape is not being differentiated.
     *
     * \param viewpoint
     *      The viewpoint that was used for the precomputed silhouette
     *      information
     *
     * \param sample1
     *      A sampled index from the return values of \ref precompute_silhouette
     *
     * \param sample2
            A uniformly distributed sample in <tt>[0,1]</tt>
     *
     * \return
     *      A boundary segment on the silhouette of the shape as seen from
     *      \c viewpoint.
     */
    virtual SilhouetteSample3f sample_precomputed_silhouette(const Point3f &viewpoint,
                                                             Index sample1,
                                                             Float sample2,
                                                             Mask active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    /**
     * \brief Fast ray intersection
     *
     * Efficiently test whether the shape is intersected by the given ray, and
     * return preliminary information about the intersection if that is the
     * case.
     *
     * If the intersection is deemed relevant (e.g. the closest to the ray
     * origin), detailed intersection information can later be obtained via the
     * \ref create_surface_interaction() method.
     *
     * \param ray
     *     The ray to be tested for an intersection
     *
     * \param prim_index
     *     Index of the primitive to be intersected. This index is ignored by a
     *     shape that contains a single primitive. Otherwise, if no index is provided,
     *     the ray intersection will be performed on the shape's first primitive at index 0.
     */
    virtual PreliminaryIntersection3f ray_intersect_preliminary(const Ray3f &ray,
                                                                ScalarIndex prim_index = 0,
                                                                Mask active = true) const;
    /**
     * \brief Fast ray shadow test
     *
     * Efficiently test whether the shape is intersected by the given ray.
     *
     * No details about the intersection are returned, hence the function is
     * only useful for visibility queries. For most shapes, the implementation
     * will simply forward the call to \ref ray_intersect_preliminary(). When
     * the shape actually contains a nested kd-tree, some optimizations are possible.
     *
     * \param ray
     *     The ray to be tested for an intersection
     *
     * \param prim_index
     *     Index of the primitive to be intersected
     */
    virtual Mask ray_test(const Ray3f &ray, ScalarIndex prim_index = 0, Mask active = true) const;

    /**
     * \brief Compute and return detailed information related to a surface interaction
     *
     * The implementation should at most compute the fields \c p, \c uv, \c n,
     * \c sh_frame.n, \c dp_du, \c dp_dv, \c dn_du and \c dn_dv. The \c flags parameter
     * specifies which of those fields should be computed.
     *
     * The fields \c t, \c time, \c wavelengths, \c shape, \c prim_index, \c instance,
     * will already have been initialized by the caller. The field \c wi is initialized
     * by the caller following the call to \ref compute_surface_interaction(), and
     * \c duv_dx, and \c duv_dy are left uninitialized.
     *
     * \param ray
     *      Ray associated with the ray intersection
     * \param pi
     *      Data structure carrying information about the ray intersection
     * \param ray_flags
     *      Flags specifying which information should be computed
     * \param recursion_depth
     *      Integer specifying the recursion depth for nested virtual function
     *      call to this method (e.g. used for instancing).
     * \return
     *      A data structure containing the detailed information
     */
    virtual SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                             const PreliminaryIntersection3f &pi,
                                                             uint32_t ray_flags = +RayFlags::All,
                                                             uint32_t recursion_depth = 0,
                                                             Mask active = true) const;

    /**
     * \brief Test for an intersection and return detailed information
     *
     * This operation combines the prior \ref ray_intersect_preliminary() and
     * \ref compute_surface_interaction() operations.
     *
     * \param ray
     *     The ray to be tested for an intersection
     *
     * \param flags
     *     Describe how the detailed information should be computed
     */
    SurfaceInteraction3f ray_intersect(const Ray3f &ray,
                                       uint32_t ray_flags = +RayFlags::All,
                                       Mask active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Packet versions of ray test/intersection routines
    // =============================================================
    /**
     * \brief Scalar test for an intersection and return detailed information
     *
     * This operation is used by the KDTree acceleration structure.
     *
     * \param ray
     *     The ray to be tested for an intersection
     *
     * \return
     *     A tuple containing the following field: \c t, \c uv, \c shape_index,
     *     \c prim_index. The \c shape_index should be only used by the
     *     \ref ShapeGroup class and be set to \c (uint32_t)-1 otherwise.
     */
    virtual std::tuple<ScalarFloat, ScalarPoint2f, ScalarUInt32, ScalarUInt32>
    ray_intersect_preliminary_scalar(const ScalarRay3f &ray) const;
    virtual bool ray_test_scalar(const ScalarRay3f &ray) const;

    /// Macro to declare packet versions of the scalar routine above
    #define MI_DECLARE_RAY_INTERSECT_PACKET(N)                                  \
        using FloatP##N   = dr::Packet<dr::scalar_t<Float>, N>;                 \
        using UInt32P##N  = dr::uint32_array_t<FloatP##N>;                      \
        using MaskP##N    = dr::mask_t<FloatP##N>;                              \
        using Point2fP##N = Point<FloatP##N, 2>;                                \
        using Point3fP##N = Point<FloatP##N, 3>;                                \
        using Ray3fP##N   = Ray<Point3fP##N, Spectrum>;                         \
        virtual std::tuple<FloatP##N, Point2fP##N, UInt32P##N, UInt32P##N>      \
        ray_intersect_preliminary_packet(const Ray3fP##N &ray,                  \
                                         ScalarIndex prim_index = 0,            \
                                         MaskP##N active = true) const;         \
        virtual MaskP##N ray_test_packet(const Ray3fP##N &ray,                  \
                                         ScalarIndex prim_index = 0,            \
                                         MaskP##N active = true) const;

    MI_DECLARE_RAY_INTERSECT_PACKET(4)
    MI_DECLARE_RAY_INTERSECT_PACKET(8)
    MI_DECLARE_RAY_INTERSECT_PACKET(16)

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Miscellaneous query routines
    // =============================================================

    /**
     * \brief Return an axis aligned box that bounds all shape primitives
     * (including any transformations that may have been applied to them)
     */
    virtual ScalarBoundingBox3f bbox() const = 0;

    /**
     * \brief Return an axis aligned box that bounds a single shape primitive
     * (including any transformations that may have been applied to it)
     *
     * \remark
     *     The default implementation simply calls \ref bbox()
     */
    virtual ScalarBoundingBox3f bbox(ScalarIndex index) const;

    /**
     * \brief Return an axis aligned box that bounds a single shape primitive
     * after it has been clipped to another bounding box.
     *
     * This is extremely important to construct high-quality kd-trees. The
     * default implementation just takes the bounding box returned by
     * \ref bbox(ScalarIndex index) and clips it to \a clip.
     */
    virtual ScalarBoundingBox3f bbox(ScalarIndex index,
                                     const ScalarBoundingBox3f &clip) const;

    /**
     * \brief Return the shape's surface area.
     *
     * The function assumes that the object is not undergoing
     * some kind of time-dependent scaling.
     *
     * The default implementation throws an exception.
     */
    virtual Float surface_area() const;

    /**
     * \brief Returns whether this shape contains the specified attribute.
     *
     * \param name
     *     Name of the attribute
     */
    virtual Mask has_attribute(const std::string &name, Mask active = true) const;

    /**
     * \brief Evaluate a specific shape attribute at the given surface interaction.
     *
     * Shape attributes are user-provided fields that provide extra
     * information at an intersection. An example of this would be a per-vertex
     * or per-face color on a triangle mesh.
     *
     * \param name
     *     Name of the attribute to evaluate
     *
     * \param si
     *     Surface interaction associated with the query
     *
     * \return
     *     An unpolarized spectral power distribution or reflectance value
     */
    virtual UnpolarizedSpectrum eval_attribute(const std::string &name,
                                               const SurfaceInteraction3f &si,
                                               Mask active = true) const;

    /**
     * \brief Monochromatic evaluation of a shape attribute at the given surface interaction
     *
     * This function differs from \ref eval_attribute() in that it provided raw access to
     * scalar intensity/reflectance values without any color processing (e.g.
     * spectral upsampling).
     *
     * \param name
     *     Name of the attribute to evaluate
     *
     * \param si
     *     Surface interaction associated with the query
     *
     * \return
     *     An scalar intensity or reflectance value
     */
    virtual Float eval_attribute_1(const std::string &name,
                                   const SurfaceInteraction3f &si,
                                   Mask active = true) const;

    /**
     * \brief Trichromatic evaluation of a shape attribute at the given surface interaction
     *
     * This function differs from \ref eval_attribute() in that it provided raw access to
     * RGB intensity/reflectance values without any additional color processing
     * (e.g. RGB-to-spectral upsampling).
     *
     * \param name
     *     Name of the attribute to evaluate
     *
     * \param si
     *     Surface interaction associated with the query
     *
     * \return
     *     An trichromatic intensity or reflectance value
     */
    virtual Color3f eval_attribute_3(const std::string &name,
                                     const SurfaceInteraction3f &si,
                                     Mask active = true) const;

    /**
     * \brief Parameterize the mesh using UV values
     *
     * This function maps a 2D UV value to a surface interaction data
     * structure. Its behavior is only well-defined in regions where this
     * mapping is bijective.
     * The default implementation throws.
     */
    virtual SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                                       uint32_t ray_flags = +RayFlags::All,
                                                       Mask active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Miscellaneous
    // =============================================================

    /// Return a string identifier
    std::string id() const override { return m_id; }

    /// Set a string identifier
    void set_id(const std::string& id) override { m_id = id; };

    /// Is this shape a triangle mesh?
    bool is_mesh() const { return (shape_type() == +ShapeType::Mesh); };

    /// Returns the shape type \ref ShapeType of this shape
    uint32_t shape_type() const { return (uint32_t) m_shape_type; }

    /// Is this shape a shapegroup?
    bool is_shapegroup() const { return class_()->name() == "ShapeGroupPlugin"; };

    /// Is this shape an instance?
    bool is_instance() const { return (shape_type() == +ShapeType::Instance); };

    /// Does the surface of this shape mark a medium transition?
    bool is_medium_transition() const { return m_interior_medium.get() != nullptr ||
                                               m_exterior_medium.get() != nullptr; }

    /// Return the medium that lies on the interior of this shape
    const Medium *interior_medium(Mask /*unused*/ = true) const { return m_interior_medium.get(); }

    /// Return the medium that lies on the exterior of this shape
    const Medium *exterior_medium(Mask /*unused*/ = true) const { return m_exterior_medium.get(); }

    /// Return the shape's BSDF
    const BSDF *bsdf(Mask /*unused*/ = true) const { return m_bsdf.get(); }

    /// Return the shape's BSDF
    BSDF *bsdf(Mask /*unused*/ = true) { return m_bsdf.get(); }

    /// Is this shape also an area emitter?
    bool is_emitter() const { return (bool) m_emitter; }

    /// Return the area emitter associated with this shape (if any)
    const Emitter *emitter(Mask /*unused*/ = true) const { return m_emitter.get(); }

    /// Return the area emitter associated with this shape (if any)
    Emitter *emitter(Mask /*unused*/ = true) { return m_emitter.get(); }

    /// Is this shape also an area sensor?
    bool is_sensor() const { return (bool) m_sensor; }

    /// Return the area sensor associated with this shape (if any)
    const Sensor *sensor(Mask /*unused*/ = true) const { return m_sensor.get(); }
    /// Return the area sensor associated with this shape (if any)
    Sensor *sensor(Mask /*unused*/ = true) { return m_sensor.get(); }

    /**
     * \brief Returns the number of sub-primitives that make up this shape
     *
     * \remark
     *     The default implementation simply returns \c 1
     */
    virtual ScalarSize primitive_count() const;

    /**
     * \brief Return the number of primitives (triangles, hairs, ..)
     * contributed to the scene by this shape
     *
     * Includes instanced geometry. The default implementation simply returns
     * the same value as \ref primitive_count().
     */
    virtual ScalarSize effective_primitive_count() const;


#if defined(MI_ENABLE_EMBREE)
    /// Return the Embree version of this shape
    virtual RTCGeometry embree_geometry(RTCDevice device);
#endif

#if defined(MI_ENABLE_CUDA)
    /**
     * \brief Populates the GPU data buffer, used in the OptiX Hitgroup sbt records.
     *
     * \remark
     *      Actual implementations of this method should allocate the field \ref
     *      m_optix_data_ptr on the GPU and populate it with the OptiX representation
     *      of the class.
     *
     * The default implementation throws an exception.
     */
    virtual void optix_prepare_geometry();

    /**
     * \brief Fills the OptixBuildInput associated with this shape.
     *
     * \param build_input
     *     A reference to the build input to be filled. The field
     *     build_input.type has to be set, along with the associated members. For
     *     now, Mitsuba only supports the types \ref OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES
     *     and \ref OPTIX_BUILD_INPUT_TYPE_TRIANGLES.
     *
     * The default implementation assumes that an implicit Shape (custom primitive
     * build type) is begin constructed, with its GPU data stored at \ref m_optix_data_ptr.
     */
    virtual void optix_build_input(OptixBuildInput& build_input) const;

    /**
     * \brief Prepares and fills the \ref OptixInstance(s) associated with this
     * shape. This process includes generating the OptiX instance acceleration
     * structure (IAS) represented by this shape, and pushing OptixInstance
     * structs to the provided instances vector.
     *
     * \remark
     *     This method is currently only implemented for the \ref Instance
     *     and \ref ShapeGroup plugin.
     *
     * \param context
     *     The OptiX context that was used to construct the rest of the scene's
     *     OptiX representation.
     *
     * \param instances
     *     The array to which new OptixInstance should be appended.
     *
     * \param instance_id
     *     The instance id, used internally inside OptiX to detect when a Shape
     *     is part of an Instance.
     *
     * \param transf
     *     The current to_world transformation (should allow for recursive instancing).
     *
     * The default implementation throws an exception.
     */
    virtual void optix_prepare_ias(const OptixDeviceContext& /*context*/,
                                   std::vector<OptixInstance>& /*instances*/,
                                   uint32_t /*instance_id*/,
                                   const ScalarTransform4f& /*transf*/);

    /**
     * \brief Creates and appends the HitGroupSbtRecord(s) associated with this
     * shape to the provided array.
     *
     * \remark
     *     This method can append multiple hitgroup records to the array
     *     (see the \ref Shapegroup plugin for an example).
     *
     * \param hitgroup_records
     *     The array of hitgroup records where the new HitGroupRecords should be
     *     appended.
     *
     * \param program_groups
     *     The array of available program groups (used to pack the OptiX header
     *     at the beginning of the record).
     *
     * The default implementation creates a new HitGroupSbtRecord and fills its
     * \ref data field with \ref m_optix_data_ptr. It then calls \ref
     * optixSbtRecordPackHeader with one of the OptixProgramGroup of the \ref
     * program_groups array (the actual program group index is inferred by the
     * type of the Shape, see \ref get_shape_descr_idx()).
     */
    virtual void optix_fill_hitgroup_records(std::vector<HitGroupSbtRecord> &hitgroup_records,
                                             const OptixProgramGroup *program_groups);
#endif

    void traverse(TraversalCallback *callback) override;
    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override;

    /// Return whether the shape's geometry has changed
    bool dirty() const { return m_dirty; }

    /// Mark that the shape's geometry has changed
    void mark_dirty() { m_dirty = true; }

    // Mark that shape as an instance
    void mark_as_instance() { m_is_instance = true; }

    /// The \c Scene and \c ShapeGroup class needs access to \c Shape::m_dirty
    friend class Scene<Float, Spectrum>;
    friend class ShapeGroup<Float, Spectrum>;

    /** \brief Return whether any shape's parameters that introduce visibility
     *  discontinuities require gradients (default return false)
     */
    virtual bool parameters_grad_enabled() const;

    //! @}
    // =============================================================

    MI_DECLARE_CLASS()

protected:
    Shape(const Properties &props);
    inline Shape() { }

protected:
    virtual void initialize();
    std::string get_children_string() const;
protected:
    ref<BSDF> m_bsdf;
    ref<Emitter> m_emitter;
    ref<Sensor> m_sensor;
    ref<Medium> m_interior_medium;
    ref<Medium> m_exterior_medium;
    std::string m_id;
    ShapeType m_shape_type = ShapeType::Other;

    uint32_t m_discontinuity_types = (uint32_t) DiscontinuityFlags::Empty;
    /// Sampling weight (proportional to scene)
    float m_silhouette_sampling_weight;

    std::unordered_map<std::string, ref<Texture>> m_texture_attributes;

    field<Transform4f, ScalarTransform4f> m_to_world;
    field<Transform4f, ScalarTransform4f> m_to_object;

    /// True if the shape is used in a \c ShapeGroup
    bool m_is_instance = false;

#if defined(MI_ENABLE_CUDA)
    /// OptiX hitgroup data buffer
    void* m_optix_data_ptr = nullptr;
#endif

protected:
    /// True if the shape's geometry has changed
    bool m_dirty = true;

    /// True if the shape has called iniatlize() at least once
    bool m_initialized = false;
};

// -----------------------------------------------------------------------
//! @{ \name Misc implementations
// -----------------------------------------------------------------------

template <typename Float, typename Spectrum>
std::ostream &operator<<(std::ostream &os,
                         const SilhouetteSample<Float, Spectrum> &ss) {
    os << "SilhouetteSample[" << std::endl
       << "  p = " << string::indent(ss.p, 6) << "," << std::endl
       << "  discontinuity_type = " << string::indent(ss.discontinuity_type, 23) << "," << std::endl
       << "  d = " << string::indent(ss.d, 6) << "," << std::endl
       << "  silhouette_d = " << string::indent(ss.silhouette_d, 17) << "," << std::endl
       << "  n = " << string::indent(ss.n, 6) << "," << std::endl
       << "  prim_index = " << ss.prim_index << "," << std::endl
       << "  scene_index = " << ss.scene_index << "," << std::endl
       << "  flags = " << ss.flags << "," << std::endl
       << "  projection_index = " << ss.projection_index << "," << std::endl
       << "  uv = " << string::indent(ss.uv, 7) << "," << std::endl
       << "  pdf = " << ss.pdf << "," << std::endl
       << "  shape = " << string::indent(ss.shape) << "," << std::endl
       << "  foreshortening = " << ss.foreshortening << "," << std::endl
       << "  offset = " << ss.offset << "," << std::endl
       << "]";
    return os;
}

//! @}
// -----------------------------------------------------------------------

MI_EXTERN_CLASS(Shape)
NAMESPACE_END(mitsuba)

#define MI_IMPLEMENT_RAY_INTERSECT_PACKET(N)                                                \
    using typename Base::FloatP##N;                                                         \
    using typename Base::UInt32P##N;                                                        \
    using typename Base::MaskP##N;                                                          \
    using typename Base::Point2fP##N;                                                       \
    using typename Base::Point3fP##N;                                                       \
    using typename Base::Ray3fP##N;                                                         \
    std::tuple<FloatP##N, Point2fP##N, UInt32P##N, UInt32P##N>                              \
    ray_intersect_preliminary_packet(                                                       \
        const Ray3fP##N &ray, ScalarIndex prim_index, MaskP##N active) const override {     \
        (void) ray; (void) prim_index; (void) active;                                       \
        if constexpr (!dr::is_cuda_v<Float>)                                                \
            return ray_intersect_preliminary_impl<FloatP##N>(ray, prim_index, active);      \
        else                                                                                \
            Throw("ray_intersect_preliminary_packet() CUDA not supported");                 \
    }                                                                                       \
    MaskP##N ray_test_packet(const Ray3fP##N &ray, ScalarIndex prim_index, MaskP##N active) \
        const override {                                                                    \
        (void) ray; (void) prim_index; (void) active;                                       \
        if constexpr (!dr::is_cuda_v<Float>)                                                \
            return ray_test_impl<FloatP##N>(ray, prim_index, active);                       \
        else                                                                                \
            Throw("ray_intersect_preliminary_packet() CUDA not supported");                 \
    }

// Macro to define ray intersection methods given an *_impl() templated implementation
#define MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()                                             \
    PreliminaryIntersection3f ray_intersect_preliminary(                                    \
        const Ray3f &ray, ScalarIndex prim_index, Mask active) const override {             \
        MI_MASK_ARGUMENT(active);                                                           \
        PreliminaryIntersection3f pi = dr::zeros<PreliminaryIntersection3f>();              \
        std::tie(pi.t, pi.prim_uv, pi.shape_index, pi.prim_index) =                         \
            ray_intersect_preliminary_impl<Float>(ray, prim_index, active);                 \
        pi.shape = this;                                                                    \
        return pi;                                                                          \
    }                                                                                       \
    Mask ray_test(const Ray3f &ray, ScalarIndex prim_index, Mask active) const override {   \
        MI_MASK_ARGUMENT(active);                                                           \
        return ray_test_impl<Float>(ray, prim_index, active);                               \
    }                                                                                       \
    using typename Base::ScalarRay3f;                                                       \
    std::tuple<ScalarFloat, ScalarPoint2f, ScalarUInt32, ScalarUInt32>                      \
    ray_intersect_preliminary_scalar(const ScalarRay3f &ray) const override {               \
        return ray_intersect_preliminary_impl<ScalarFloat>(ray, 0, true);                   \
    }                                                                                       \
    ScalarMask ray_test_scalar(const ScalarRay3f &ray) const override {                     \
        return ray_test_impl<ScalarFloat>(ray, 0, true);                                    \
    }                                                                                       \
    MI_IMPLEMENT_RAY_INTERSECT_PACKET(4)                                                    \
    MI_IMPLEMENT_RAY_INTERSECT_PACKET(8)                                                    \
    MI_IMPLEMENT_RAY_INTERSECT_PACKET(16)

// -----------------------------------------------------------------------
//! @{ \name Dr.Jit support for vectorized function calls
// -----------------------------------------------------------------------

DRJIT_CALL_TEMPLATE_BEGIN(mitsuba::Shape)
    DRJIT_CALL_METHOD(compute_surface_interaction)
    DRJIT_CALL_METHOD(has_attribute)
    DRJIT_CALL_METHOD(eval_attribute)
    DRJIT_CALL_METHOD(eval_attribute_1)
    DRJIT_CALL_METHOD(eval_attribute_3)
    DRJIT_CALL_METHOD(eval_parameterization)
    DRJIT_CALL_METHOD(ray_intersect_preliminary)
    DRJIT_CALL_METHOD(ray_intersect)
    DRJIT_CALL_METHOD(ray_test)
    DRJIT_CALL_METHOD(sample_position)
    DRJIT_CALL_METHOD(pdf_position)
    DRJIT_CALL_METHOD(sample_direction)
    DRJIT_CALL_METHOD(pdf_direction)
    DRJIT_CALL_METHOD(sample_silhouette)
    DRJIT_CALL_METHOD(invert_silhouette_sample)
    DRJIT_CALL_METHOD(primitive_silhouette_projection)
    DRJIT_CALL_METHOD(differential_motion)
    DRJIT_CALL_METHOD(sample_precomputed_silhouette)
    DRJIT_CALL_METHOD(surface_area)
    DRJIT_CALL_GETTER(emitter)
    DRJIT_CALL_GETTER(sensor)
    DRJIT_CALL_GETTER(bsdf)
    DRJIT_CALL_GETTER(interior_medium)
    DRJIT_CALL_GETTER(exterior_medium)
    DRJIT_CALL_GETTER(silhouette_discontinuity_types)
    DRJIT_CALL_GETTER(silhouette_sampling_weight)
    DRJIT_CALL_GETTER(shape_type)
    auto is_emitter() const { return emitter() != nullptr; }
    auto is_sensor() const { return sensor() != nullptr; }
    auto is_mesh() const { return shape_type() == (uint32_t) mitsuba::ShapeType::Mesh; }
    auto is_medium_transition() const { return interior_medium() != nullptr ||
                                               exterior_medium() != nullptr; }
DRJIT_CALL_END(mitsuba::Shape)

//! @}
// -----------------------------------------------------------------------
