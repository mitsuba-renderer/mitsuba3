#pragma once

#include <mitsuba/render/bsdf.h>
#include <drjit/call.h>
#include <mitsuba/render/records.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/field.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/scene_ir.h>
#include <mitsuba/core/animated_transform.h>
#include <drjit/packet.h>
#include <map>

#if defined(MI_ENABLE_CUDA)
#  include <mitsuba/render/optix/common.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/**
 * This list of flags is used to control the behavior of discontinuity
 * related routines.
 */
enum class DiscontinuityFlags : uint32_t {
    // =============================================================
    //                    Discontinuity types
    // =============================================================

    /// No flags set (default value)
    Empty = 0x0,

    /// Open boundary or jumping normal type of discontinuity
    PerimeterType = 0x1,

    /// Smooth normal type of discontinuity
    InteriorType = 0x2,

    // =============================================================
    //               Encoding and projection flags
    // =============================================================

    /**
     * Use spherical lune to encode segment direction
     *
     * This flag is only relevant for certain shape types.
     */
    DirectionLune = 0x4,

    /**
     * Use spherical coordinates to encode segment direction
     *
     * This flag is only relevant for certain shape types.
     */
    DirectionSphere = 0x8,

    /**
     * Project to an edge using a heuristic probability
     *
     * This flag only applies to triangle meshes.
     *
     * By default a projection operation on a mesh triangle would uniformly pick
     * one of its three edges. This flag modifies that operation such that each
     * edge is weighted according to the angle it forms between the two adjacent
     * faces.
     */
    HeuristicWalk = 0x10,

    // =============================================================
    //                  Compound types
    // =============================================================

    /// All types of discontinuities
    AllTypes = PerimeterType | InteriorType
};
MI_DECLARE_ENUM_OPERATORS(DiscontinuityFlags)

// Forward declaration for SilhouetteSample3f
template <typename Float, typename Spectrum> class Shape;

/**
 * Data structure holding the result of visibility silhouette sampling
 * operations on geometry.
 */
template <typename Float_, typename Spectrum_>
struct SilhouetteSample : public PositionSample<Float_, Spectrum_> {
    // =============================================================
    // Type declarations
    // =============================================================
    using Float    = Float_;
    using Spectrum = Spectrum_;

    MI_IMPORT_BASE(PositionSample, p, n, uv, time, pdf, delta)

    MI_IMPORT_RENDER_BASIC_TYPES()
    MI_IMPORT_OBJECT_TYPES()

    // =============================================================

    // =============================================================
    // Fields
    // =============================================================

    /// Type of discontinuity (`DiscontinuityFlags`)
    UInt32 discontinuity_type;

    /// Direction of the boundary segment sample
    Vector3f d;

    /// Direction of the silhouette curve at the boundary point
    Vector3f silhouette_d;

    /// Primitive index, e.g. the triangle ID (if applicable)
    UInt32 prim_index;

    /// Index of the shape in the scene (if applicable)
    UInt32 scene_index;

    /// The set of `DiscontinuityFlags` that were used to generate this sample
    UInt32 flags;

    /**
     * Projection index indicator
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
     * Local-form boundary foreshortening term.
     *
     * It stores ``sin_phi_B`` for perimeter silhouettes or the normal curvature
     * for interior silhouettes.
     */
    Float foreshortening;

    /**
     * Offset along the boundary segment direction (``d``) to avoid
     * self-intersections.
     */
    Float offset;

    // =============================================================

    // =============================================================
    // Methods
    // =============================================================

    /// Partially initialize a boundary segment from a position sample
    SilhouetteSample(const PositionSample<Float, Spectrum> &ps)
        : Base(ps), discontinuity_type((uint32_t) DiscontinuityFlags::Empty),
          d(0), silhouette_d(0), prim_index(0), scene_index(0), flags(0),
          projection_index(0), shape(nullptr), foreshortening(0), offset(0) {}

    /// Is the current boundary segment valid?
    Mask is_valid() const {
        return discontinuity_type != (uint32_t) DiscontinuityFlags::Empty;
    }

    /**
     * Spawn a ray on the silhouette point in the direction of ``d``
     *
     * The ray origin is offset in the direction of the segment (``d``) as well
     * as in the direction of the silhouette normal (``n``). Without this
     * offsetting, during a ray intersection, the ray could potentially find
     * an intersection point at its origin due to numerical instabilities in
     * the intersection routines.
     */
    Ray3f spawn_ray(Wavelength wavelengths = dr::zeros<Wavelength>()) const {
        Vector3f o_offset = (1 + dr::max(dr::abs(p))) *
                            (d * offset + n * math::ShapeEpsilon<Float>);
        return Ray3f(p + o_offset, d, 0.f, wavelengths);
    }

    // =============================================================

    DRJIT_STRUCT(SilhouetteSample, p, n, uv, time, pdf, delta,
                 discontinuity_type, d, silhouette_d, prim_index, scene_index,
                 flags, projection_index, shape, foreshortening, offset)
};

/**
 * Base class of all geometric shapes in Mitsuba
 *
 * This class provides core functionality for sampling positions on surfaces,
 * computing ray intersections, and bounding shapes within ray intersection
 * acceleration data structures.
 *
 * Two types of attributes can be associated with a shape:
 *
 * 1. Texture attributes (`Shape.add_texture_attribute`), which must be
 *    a `Texture` instance but can have arbitrary resolution. The UV
 *    parametrization of the shape is used to look up texture attribute values.
 *
 * 2. Mesh attributes (`Mesh.add_attribute`), which can only be added
 *    to mesh-type Shapes. They must be either per-vertex or per-face attributes,
 *    their name must start with ``vertex_`` (resp. ``face_``), and their size
 *    must match the number of vertices (resp. faces) of the mesh.
 *
 * Once registered, attributes are queried with the `Shape.eval_attribute`,
 * `Shape.eval_attribute_1` and `Shape.eval_attribute_3` methods.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Shape : public JitObject<Shape<Float, Spectrum>> {
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
    // Sampling routines
    // =============================================================

    /**
     * Sample a point on the surface of this shape
     *
     * The sampling strategy is ideally uniform over the surface, though
     * implementations are allowed to deviate from a perfectly uniform
     * distribution as long as this is reflected in the returned probability
     * density.
     *
     * Args:
     *     time: The scene time associated with the position sample
     *
     *     sample: A uniformly distributed 2D point on the domain :math:`[0,1]^2`
     *
     * Returns:
     *     A `PositionSample3f` instance describing the generated sample
     */
    virtual PositionSample3f sample_position(Float time, const Point2f &sample,
                                             Mask active = true) const;

    /**
     * Query the probability density of `sample_position()` for
     * a particular point on the surface.
     *
     * Args:
     *     ps: A position record describing the sample in question
     *
     * Returns:
     *     The probability density per unit area
     */
    virtual Float pdf_position(const PositionSample3f &ps, Mask active = true) const;

    /**
     * Sample a direction towards this shape with respect to solid
     * angles measured at a reference position within the scene
     *
     * An ideal implementation of this interface would achieve a uniform solid
     * angle density within the surface region that is visible from the
     * reference position ``it.p`` (though such an ideal implementation
     * is usually neither feasible nor advisable due to poor efficiency).
     *
     * The function returns the sampled position and the inverse probability
     * per unit solid angle associated with the sample.
     *
     * When the Shape subclass does not supply a custom implementation of this
     * function, the `Shape` class reverts to a fallback approach that
     * piggybacks on `sample_position()`. This will generally lead to a
     * suboptimal sample placement and higher variance in Monte Carlo
     * estimators using the samples.
     *
     * Args:
     *     it: A reference position somewhere within the scene.
     *
     *     sample: A uniformly distributed 2D point on the domain :math:`[0,1]^2`
     *
     * Returns:
     *     A `DirectionSample3f` instance describing the generated sample
     */
    virtual DirectionSample3f sample_direction(const Interaction3f &it, const Point2f &sample,
                                               Mask active = true) const;

    /**
     * Query the probability density of `sample_direction()`
     *
     * Args:
     *     it: A reference position somewhere within the scene.
     *
     *     ps: A position record describing the sample in question
     *
     * Returns:
     *     The probability density per unit solid angle
     */
    virtual Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                                Mask active = true) const;

    // =============================================================

    // =============================================================
    // Silhouette sampling routines and other utilities
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
     * Map a point sample in boundary sample space to a silhouette
     * segment
     *
     * This method's behavior is undefined when used in non-JIT variants or
     * when the shape is not being differentiated.
     *
     * Args:
     *     sample: The boundary space sample (a point in the unit cube).
     *
     *     flags: Flags to select the type of silhouettes to sample
     *         from (see `DiscontinuityFlags`).
     *         Only one type of discontinuity can be sampled per call.
     *
     * Returns:
     *     Silhouette sample record.
     */
    virtual SilhouetteSample3f sample_silhouette(const Point3f &sample,
                                                 uint32_t flags,
                                                 Mask active = true) const;

    /**
     * Map a silhouette segment to a point in boundary sample space
     *
     * This method is the inverse of `sample_silhouette()`. The mapping
     * from/to boundary sample space to/from boundary segments is bijective.
     *
     * This method's behavior is undefined when used in non-JIT variants or
     * when the shape is not being differentiated.
     *
     * Args:
     *     ss: The sampled boundary segment
     *
     * Returns:
     *     The corresponding boundary sample space point
     */
    virtual Point3f invert_silhouette_sample(const SilhouetteSample3f &ss,
                                             Mask active = true) const;

    /**
     * Return the attached (AD) point on the shape's surface
     *
     * This method is only useful when using automatic differentiation. The
     * immediate/primal return value of this method is exactly equal to
     * ``si.p``.
     *
     * The input ``si`` does not need to be explicitly detached, it is done by the
     * method itself.
     *
     * If the shape cannot be differentiated, this method will return the
     * detached input point.
     *
     * Args:
     *     si: The surface point for which the function will be evaluated.
     *
     *         Not all fields of the object need to be filled. Only the
     *         `SurfaceInteraction3f.prim_index`, `Interaction3f.p` and
     *         `SurfaceInteraction3f.uv` fields are required. Certain shapes
     *         will only use a subset of these.
     *
     * Returns:
     *     The same surface point as the input but attached (AD) to the shape's
     *     parameters.
     *
     * Note:
     *     The returned attached point is exactly the same as a point which
     *     is computed by calling `compute_surface_interaction` with the
     *     `RayFlags.FollowShape` flag.
     */
    virtual Point3f differential_motion(const SurfaceInteraction3f &si,
                                        Mask active = true) const;

    /**
     * Projects a point on the surface of the shape to its silhouette
     * as seen from a specified viewpoint.
     *
     * This method only projects the ``si.p`` point within its primitive.
     *
     * Not all of the fields of the `SilhouetteSample3f` might be filled by
     * this method. Each shape will at the very least fill its return value with
     * enough information for it to be used by `invert_silhouette_sample`.
     *
     * The projection operation might not find the closest silhouette point to
     * the given surface point. For example, it can be guided by a random number
     * ``sample``. Not all shapes types need this random number, each shape
     * implementation is free to define its own algorithm and guarantees about
     * the projection operation.
     *
     * This method's behavior is undefined when used in non-JIT variants or
     * when the shape is not being differentiated.
     *
     * Args:
     *     viewpoint: The viewpoint which defines the silhouette to project the point to.
     *
     *     si: The surface point which will be projected.
     *
     *     flags: Flags to select the type of `SilhouetteSample3f` to generate from
     *         the projection. Only one type of discontinuity can be used per call.
     *
     *     sample: A random number that can be used to define the projection operation.
     *
     * Returns:
     *     A boundary segment on the silhouette of the shape as seen from
     *     ``viewpoint``.
     */
    virtual SilhouetteSample3f primitive_silhouette_projection(const Point3f &viewpoint,
                                                               const SurfaceInteraction3f &si,
                                                               uint32_t flags,
                                                               Float sample,
                                                               Mask active = true) const;

    /**
     * Precompute the visible silhouette of this shape for a given
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
     * `sample_precomputed_silhouette`.
     *
     * This method's behavior is undefined when used in non-JIT variants or
     * when the shape is not being differentiated.
     *
     * Args:
     *     viewpoint: The viewpoint which defines the silhouette of the shape
     *
     * Returns:
     *     A list of indices used by the shape internally to represent
     *     silhouettes, and a list of the same length containing the
     *     (unnormalized) weights associated to each index.
     */
    virtual std::tuple<DynamicBuffer<UInt32>, DynamicBuffer<Float>>
    precompute_silhouette(const ScalarPoint3f &viewpoint) const;

    /**
     * Samples a boundary segment on the shape's silhouette using
     * precomputed information computed in `Shape.precompute_silhouette`.
     *
     * This method is meant to be used for silhouettes that are shared between
     * all threads, as is the case for primarily visible derivatives.
     *
     * This method's behavior is undefined when used in non-JIT variants or
     * when the shape is not being differentiated.
     *
     * Args:
     *     viewpoint: The viewpoint that was used for the precomputed silhouette
     *         information
     *
     *     sample1: A sampled index from the return values of `Shape.precompute_silhouette`
     *
     *     sample2: A uniformly distributed sample in ``[0,1]``
     *
     * Returns:
     *     A boundary segment on the silhouette of the shape as seen from
     *     ``viewpoint``.
     */
    virtual SilhouetteSample3f sample_precomputed_silhouette(const Point3f &viewpoint,
                                                             Index sample1,
                                                             Float sample2,
                                                             Mask active = true) const;

    // =============================================================

    // =============================================================
    // Ray tracing routines
    // =============================================================

    /**
     * Fast ray intersection
     *
     * Efficiently test whether the shape is intersected by the given ray, and
     * return preliminary information about the intersection if that is the
     * case.
     *
     * If the intersection is deemed relevant (e.g. the closest to the ray
     * origin), detailed intersection information can later be obtained via the
     * `compute_surface_interaction()` method.
     *
     * Args:
     *     ray: The ray to be tested for an intersection
     *
     *     prim_index: Index of the primitive to be intersected. This index is ignored by a
     *         shape that contains a single primitive. Otherwise, if no index is provided,
     *         the ray intersection will be performed on the shape's first primitive at index 0.
     */
    virtual PreliminaryIntersection3f ray_intersect_preliminary(const Ray3f &ray,
                                                                ScalarIndex prim_index = 0,
                                                                Mask active = true) const;
    /**
     * Fast ray shadow test
     *
     * Efficiently test whether the shape is intersected by the given ray.
     *
     * No details about the intersection are returned, hence the function is
     * only useful for visibility queries. For most shapes, the implementation
     * will simply forward the call to `ray_intersect_preliminary()`. When
     * the shape actually contains a nested kd-tree, some optimizations are possible.
     *
     * Args:
     *     ray: The ray to be tested for an intersection
     */
    virtual Mask ray_test(const Ray3f &ray, ScalarIndex prim_index = 0, Mask active = true) const;

    /**
     * Compute and return detailed information related to a surface interaction
     *
     * The implementation should at most compute the fields ``p``, ``uv``, ``n``,
     * ``sh_frame.n``, ``dp_du``, ``dp_dv``, ``dn_du`` and ``dn_dv``. The ``ray_flags``
     * parameter specifies which of those fields should be computed.
     *
     * The fields ``t``, ``time``, ``wavelengths``, ``shape``, ``prim_index``, ``instance``,
     * will already have been initialized by the caller. The field ``wi`` is initialized
     * by the caller following the call to `compute_surface_interaction()`, and
     * ``duv_dx``, and ``duv_dy`` are left uninitialized.
     *
     * Args:
     *     ray: Ray associated with the ray intersection
     *
     *     pi: Data structure carrying information about the ray intersection
     *
     *     ray_flags: Flags specifying which information should be computed
     *
     * Returns:
     *     A data structure containing the detailed information
     */
    virtual SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                             const PreliminaryIntersection3f &pi,
                                                             uint32_t ray_flags = +RayFlags::Default,
                                                             Mask active = true) const;

    /**
     * Test for an intersection and return detailed information
     *
     * This operation combines the prior `ray_intersect_preliminary()` and
     * `compute_surface_interaction()` operations.
     *
     * Args:
     *     ray: The ray to be tested for an intersection
     *
     *     ray_flags: Describe how the detailed information should be computed
     */
    SurfaceInteraction3f ray_intersect(const Ray3f &ray,
                                       uint32_t ray_flags = +RayFlags::Default,
                                       Mask active = true) const;

    // =============================================================

    // =============================================================
    // Packet versions of ray test/intersection routines
    // =============================================================
    /**
     * Scalar test for an intersection and return detailed information
     *
     * This operation is used by the KDTree acceleration structure.
     *
     * Args:
     *     ray: The ray to be tested for an intersection
     *
     * Returns:
     *     A tuple containing the following field: ``valid``, ``t``, ``uv``,
     *     ``shape_index``, ``prim_index``. The ``shape_index`` should be only used by the
     *     ``ShapeGroup`` class and be set to ``(uint32_t)-1`` otherwise.
     */
    virtual std::tuple<bool, ScalarFloat, ScalarPoint2f,
                       ScalarUInt32, ScalarUInt32>
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
        virtual std::tuple<MaskP##N, FloatP##N, Point2fP##N,                    \
                           UInt32P##N, UInt32P##N>                              \
        ray_intersect_preliminary_packet(const Ray3fP##N &ray,                  \
                                         ScalarIndex prim_index = 0,            \
                                         MaskP##N active = true) const;         \
        virtual MaskP##N ray_test_packet(const Ray3fP##N &ray,                  \
                                         ScalarIndex prim_index = 0,            \
                                         MaskP##N active = true) const;

    MI_DECLARE_RAY_INTERSECT_PACKET(4)
    MI_DECLARE_RAY_INTERSECT_PACKET(8)
    MI_DECLARE_RAY_INTERSECT_PACKET(16)

    // =============================================================

    // =============================================================
    // Miscellaneous query routines
    // =============================================================

    /**
     * Return an axis aligned box that bounds all shape primitives
     * (including any transformations that may have been applied to them)
     */
    virtual ScalarBoundingBox3f bbox() const = 0;

    /**
     * Return an axis aligned box that bounds a single shape primitive
     * (including any transformations that may have been applied to it)
     *
     * Note:
     *     The default implementation simply calls `bbox()`
     */
    virtual ScalarBoundingBox3f bbox(ScalarIndex index) const;

    /**
     * Return an axis aligned box that bounds a single shape primitive
     * after it has been clipped to another bounding box.
     *
     * This is extremely important to construct high-quality kd-trees. The
     * default implementation just takes the bounding box returned by
     * `Shape.bbox` and clips it to ``clip``.
     */
    virtual ScalarBoundingBox3f bbox(ScalarIndex index,
                                     const ScalarBoundingBox3f &clip) const;

    /**
     * Return the shape's surface area.
     *
     * The function assumes that the object is not undergoing
     * some kind of time-dependent scaling.
     *
     * The default implementation throws an exception.
     */
    virtual Float surface_area() const;

    /**
     * Add a texture attribute with the given ``name``.
     *
     * If an attribute with the same name already exists, it is replaced.
     *
     * Note that `Mesh` shapes can additionally handle per-vertex
     * and per-face attributes via the `Mesh.add_attribute` method.
     *
     * Args:
     *     name: Name of the attribute
     *
     *     texture: `Texture` to store. The dimensionality of the attribute
     *         is simply the channel count of the texture.
     */
    virtual void add_texture_attribute(std::string_view name, Texture *texture);

    /// Return the texture attribute associated with ``name``.
    Texture *texture_attribute(std::string_view name);

    /// Return the texture attribute associated with ``name``.
    const Texture *texture_attribute(std::string_view name) const;

    /**
     * Remove a texture attribute with the given ``name``.
     *
     * Throws an exception if the attribute was not registered.
     */
    virtual void remove_attribute(std::string_view name);

    /**
     * Returns whether this shape contains the specified attribute.
     *
     * Args:
     *     name: Name of the attribute
     */
    virtual Mask has_attribute(std::string_view name, Mask active = true) const;

    /**
     * Evaluate a specific shape attribute at the given surface interaction.
     *
     * Shape attributes are user-provided fields that provide extra
     * information at an intersection. An example of this would be a per-vertex
     * or per-face color on a triangle mesh.
     *
     * An attribute that the shape does not carry, or that does not fit the
     * requested channel count, evaluates to zero. Use `has_attribute()`
     * to distinguish this from an attribute that is present and zero.
     *
     * Args:
     *     name: Name of the attribute to evaluate
     *
     *     si: Surface interaction associated with the query
     *
     * Returns:
     *     An unpolarized spectral power distribution or reflectance value
     */
    virtual UnpolarizedSpectrum eval_attribute(std::string_view name,
                                               const SurfaceInteraction3f &si,
                                               Mask active = true) const;

    /**
     * Monochromatic evaluation of a shape attribute at the given surface interaction
     *
     * This function differs from `eval_attribute()` in that it provides raw access to
     * scalar intensity/reflectance values without any color processing (e.g.
     * spectral upsampling).
     *
     * Args:
     *     name: Name of the attribute to evaluate
     *
     *     si: Surface interaction associated with the query
     *
     * Returns:
     *     A scalar intensity or reflectance value
     */
    virtual Float eval_attribute_1(std::string_view name,
                                   const SurfaceInteraction3f &si,
                                   Mask active = true) const;

    /**
     * Trichromatic evaluation of a shape attribute at the given surface interaction
     *
     * This function differs from `eval_attribute()` in that it provides raw access to
     * RGB intensity/reflectance values without any additional color processing
     * (e.g. RGB-to-spectral upsampling).
     *
     * Args:
     *     name: Name of the attribute to evaluate
     *
     *     si: Surface interaction associated with the query
     *
     * Returns:
     *     A trichromatic intensity or reflectance value
     */
    virtual Color3f eval_attribute_3(std::string_view name,
                                     const SurfaceInteraction3f &si,
                                     Mask active = true) const;

    /**
     * Evaluate a dynamically sized shape attribute at the given surface interaction.
     *
     * Args:
     *     name: Name of the attribute to evaluate
     *
     *     si: Surface interaction associated with the query
     *
     * Returns:
     *     A dynamic array of attribute values
     */
    virtual dr::DynamicArray<Float> eval_attribute_x(std::string_view name,
                                                     const SurfaceInteraction3f &si,
                                                     Mask active = true) const;

    /**
     * Parameterize the mesh using UV values
     *
     * This function maps a 2D UV value to a surface interaction data
     * structure. Its behavior is only well-defined in regions where this
     * mapping is bijective.
     * The default implementation throws.
     */
    virtual SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                                       uint32_t ray_flags = +RayFlags::Default,
                                                       Mask active = true) const;

    // =============================================================

    // =============================================================
    // Miscellaneous
    // =============================================================

    /// Is this shape a triangle mesh?
    bool is_mesh() const { return shape_type() & ShapeType::Mesh; }

    /// Is this shape a `ShapeType.Ellipsoids` or `ShapeType.EllipsoidsMesh`
    bool is_ellipsoids() const {
        uint32_t st = shape_type();
        st &= ~ShapeType::Mesh;
        return (st & ShapeType::Ellipsoids) | (st & ShapeType::EllipsoidsMesh);
    }

    /// Returns the shape type `ShapeType` of this shape
    uint32_t shape_type() const { return (uint32_t) m_shape_type; }

    /// Is this shape a shape group?
    bool is_shape_group() const { return (shape_type() == +ShapeType::ShapeGroup); };

    /// Is this shape an instance?
    bool is_instance() const { return shape_type() == +ShapeType::Instance; };

    /// Return the object-to-world transformation
    AffineTransform4f to_world() const { return m_to_world->eval(0.f); }

    /// Return the object-to-world transformation (scalar form)
    ScalarAffineTransform4f to_world_scalar() const { return m_to_world->eval_scalar(0.f); }

    /// Return the underlying (possibly animated) object-to-world transformation
    const AnimatedTransform4f *animated_to_world() const { return m_to_world.get(); }

    /// Does the surface of this shape mark a medium transition?
    bool is_medium_transition() const { return m_interior_medium.get() != nullptr ||
                                               m_exterior_medium.get() != nullptr; }

    /// Return the medium that lies on the interior of this shape
    const Medium *interior_medium(Mask /*unused*/ = true) const { return m_interior_medium.get(); }

    /// Return the medium that lies on the exterior of this shape
    const Medium *exterior_medium(Mask /*unused*/ = true) const { return m_exterior_medium.get(); }

    /// Return the shape's `BSDF`
    const BSDF *bsdf(Mask /*unused*/ = true) const { return m_bsdf.get(); }

    /// Return the shape's `BSDF`
    BSDF *bsdf(Mask /*unused*/ = true) { return m_bsdf.get(); }

    /// Set the shape's `BSDF`
    virtual void set_bsdf(BSDF *bsdf);

    /// Is this shape also an area emitter?
    bool is_emitter() const { return (bool) m_emitter; }

    /// Return the area emitter associated with this shape (if any)
    const Emitter *emitter(Mask /*unused*/ = true) const { return m_emitter.get(); }

    /// Return the area emitter associated with this shape (if any)
    Emitter *emitter(Mask /*unused*/ = true) { return m_emitter.get(); }

    /**
     * Return the shape's 8-bit visibility mask (see `RayMask`)
     *
     * A ray can only intersect this shape when the bitwise AND of its
     * ray-side mask and this value is nonzero. Ordinary shapes match every
     * ray. Shapes with an attached emitter return its
     * `Emitter.visibility_mask()`.
     */
    uint32_t visibility_mask() const;

    /// Is this shape also an area sensor?
    bool is_sensor() const { return (bool) m_sensor; }

    /// Return the area sensor associated with this shape (if any)
    const Sensor *sensor(Mask /*unused*/ = true) const { return m_sensor.get(); }
    /// Return the area sensor associated with this shape (if any)
    Sensor *sensor(Mask /*unused*/ = true) { return m_sensor.get(); }

    /**
     * Returns the number of sub-primitives that make up this shape
     *
     * Note:
     *     The default implementation simply returns ``1``
     */
    virtual ScalarSize primitive_count() const;

    /**
     * Return the number of primitives (triangles, hairs, ..)
     * contributed to the scene by this shape
     *
     * Includes instanced geometry. The default implementation simply returns
     * the same value as `primitive_count()`.
     */
    virtual ScalarSize effective_primitive_count() const;

    /// Does this shape have flipped normals?
    virtual bool has_flipped_normals() const;

    /**
     * Describe this shape's geometry to the ray-tracing backends.
     *
     * Fills a backend-neutral ``ShapeIR`` descriptor that each backend's
     * scene builder consumes to construct its acceleration structures. Called
     * once per shape at build/update time.
     *
     * The default implementation describes a single-primitive custom
     * (bounding-box) shape whose AABB is the shape's bounding box.
     */
    virtual void describe(ShapeIR &g) const;

    /// Describe a custom shape and register a ``fill_data`` callback that emits
    /// one ``PodT`` per primitive via ``Derived::gpu_fill_data()``.
    template <typename Derived, typename PodT>
    void describe_with_data(ShapeIR &g) const {
        Shape::describe(g);
        g.pdata_size = sizeof(PodT);
        g.fill_data = [](const void *ctx, void *out) {
            static_cast<const Derived *>(ctx)->gpu_fill_data(out);
        };
    }

    /// Invalidate hits whose geometric normal faces along the ray, enforcing the
    /// single-sided contract on backends (Metal) that report both faces.
    void cull_backface(SurfaceInteraction3f &si, const Ray3f &ray, Mask active) const {
        Mask backface = active & (dr::dot(si.n, ray.d) > 0.f);
        si.t = dr::select(backface, dr::Infinity<Float>, si.t);
    }

    void traverse(TraversalCallback *callback) override;
    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override;

    /// Return whether the shape's geometry has changed
    bool dirty() const { return m_dirty; }

    /// Mark that the shape's geometry has changed
    void mark_dirty() { m_dirty = true; }

    // Mark that shape as an instance
    void mark_as_instance() { m_is_instance = true; }

    /// The `Scene` and ``ShapeGroup`` class needs access to `Shape.m_dirty`
    friend class Scene<Float, Spectrum>;
    friend class ShapeGroup<Float, Spectrum>;

    /**
     * Return whether any shape's parameters that introduce visibility
     * discontinuities require gradients (default return false)
     */
    virtual bool parameters_grad_enabled() const;

    // =============================================================

    MI_DECLARE_PLUGIN_BASE_CLASS(Shape)

protected:
    Shape(const Properties &props);
    inline Shape() : JitObject<Shape>("") { }

protected:
    virtual void initialize();
    std::string get_children_string() const;

protected:
    ref<BSDF> m_bsdf;
    ref<Emitter> m_emitter;
    ref<Sensor> m_sensor;
    ref<Medium> m_interior_medium;
    ref<Medium> m_exterior_medium;
    ShapeType m_shape_type = ShapeType::Invalid;

    uint32_t m_discontinuity_types = (uint32_t) DiscontinuityFlags::Empty;
    /// Sampling weight (proportional to scene)
    float m_silhouette_sampling_weight;

    std::map<std::string, ref<Texture>, std::less<>> m_texture_attributes;

    ref<AnimatedTransform4f> m_to_world;

    /// True if the shape is used in a ``ShapeGroup``
    bool m_is_instance = false;

protected:
    /// True if the shape's geometry has changed
    bool m_dirty = true;

    /// True if the shape has called initialize() at least once
    bool m_initialized = false;

    MI_DECLARE_TRAVERSE_CB(m_bsdf, m_emitter, m_sensor, m_interior_medium,
                           m_exterior_medium, m_texture_attributes, m_to_world)
};

// -----------------------------------------------------------------------
// Misc implementations
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
    std::tuple<MaskP##N, FloatP##N, Point2fP##N, UInt32P##N, UInt32P##N>                    \
    ray_intersect_preliminary_packet(                                                       \
        const Ray3fP##N &ray, ScalarIndex prim_index, MaskP##N active) const override {     \
        (void) ray; (void) prim_index; (void) active;                                       \
        if constexpr (!dr::is_cuda_v<Float> && !dr::is_metal_v<Float>)                      \
            return ray_intersect_preliminary_impl<FloatP##N>(ray, prim_index, active);      \
        else                                                                                \
            Throw("ray_intersect_preliminary_packet() CUDA/Metal not supported");           \
    }                                                                                       \
    MaskP##N ray_test_packet(const Ray3fP##N &ray, ScalarIndex prim_index, MaskP##N active) \
        const override {                                                                    \
        (void) ray; (void) prim_index; (void) active;                                       \
        if constexpr (!dr::is_cuda_v<Float> && !dr::is_metal_v<Float>)                      \
            return ray_test_impl<FloatP##N>(ray, prim_index, active);                       \
        else                                                                                \
            Throw("ray_intersect_preliminary_packet() CUDA/Metal not supported");           \
    }

// Macro to define ray intersection methods given an *_impl() templated implementation
#define MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()                                             \
    PreliminaryIntersection3f ray_intersect_preliminary(                                    \
        const Ray3f &ray, ScalarIndex prim_index, Mask active) const override {             \
        MI_MASK_ARGUMENT(active);                                                           \
        PreliminaryIntersection3f pi = dr::zeros<PreliminaryIntersection3f>();              \
        std::tie(pi.valid, pi.t, pi.prim_uv, std::ignore, pi.prim_index) =                  \
            ray_intersect_preliminary_impl<Float>(ray, prim_index, active);                 \
        pi.shape = this;                                                                    \
        return pi;                                                                          \
    }                                                                                       \
    Mask ray_test(const Ray3f &ray, ScalarIndex prim_index, Mask active) const override {   \
        MI_MASK_ARGUMENT(active);                                                           \
        return ray_test_impl<Float>(ray, prim_index, active);                               \
    }                                                                                       \
    using typename Base::ScalarRay3f;                                                       \
    std::tuple<bool, ScalarFloat, ScalarPoint2f, ScalarUInt32, ScalarUInt32>                \
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
// Enables vectorized method calls on Dr.Jit arrays of shapes
// -----------------------------------------------------------------------

DRJIT_CALL_TEMPLATE_BEGIN(mitsuba::Shape)
    DRJIT_CALL_METHOD(compute_surface_interaction)
    DRJIT_CALL_METHOD(has_attribute)
    DRJIT_CALL_METHOD(eval_attribute)
    DRJIT_CALL_METHOD(eval_attribute_1)
    DRJIT_CALL_METHOD(eval_attribute_3)
    DRJIT_CALL_METHOD(eval_attribute_x)
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
    DRJIT_CALL_GETTER(has_flipped_normals)
    DRJIT_CALL_GETTER(shape_type)
    auto is_emitter() const { return emitter() != nullptr; }
    auto is_sensor() const { return sensor() != nullptr; }
    auto is_mesh() const { return (shape_type() & +mitsuba::ShapeType::Mesh) != 0; }
    auto is_shape_group() const { return shape_type() == +mitsuba::ShapeType::ShapeGroup; }
    auto is_ellipsoids() const {
        auto st = shape_type();
        st &= ~mitsuba::ShapeType::Mesh;
        return ((st & (uint32_t) mitsuba::ShapeType::Ellipsoids) |
                (st & (uint32_t) mitsuba::ShapeType::EllipsoidsMesh)) != 0;
    }
    auto is_medium_transition() const { return interior_medium() != nullptr ||
                                               exterior_medium() != nullptr; }
DRJIT_CALL_END()

// -----------------------------------------------------------------------
