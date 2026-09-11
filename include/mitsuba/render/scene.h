#pragma once

#include <mitsuba/core/distr_1d.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/shapegroup.h>
#include <mitsuba/render/accel.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Central scene data structure
 *
 * Mitsuba's scene class encapsulates a tree of mitsuba `Object` instances
 * including emitters, sensors, shapes, materials, participating media, the
 * integrator (i.e. the method used to render the image) etc.
 *
 * It organizes these objects into groups that can be accessed through getters
 * (see `shapes()`, `emitters()`, `sensors()`, etc.), and it provides
 * three key abstractions implemented on top of these groups, specifically:
 *
 * * Ray intersection queries and shadow ray tests
 *   (See `ray_intersect_preliminary()`, `ray_intersect()`,
 *   and `ray_test()`).
 *
 * * Sampling rays approximately proportional to the emission profile of
 *   light sources in the scene (see `sample_emitter_ray()`)
 *
 * * Sampling directions approximately proportional to the
 *   direct radiance from emitters received at a given scene location
 *   (see `sample_emitter_direction()`).
 *
 * In AD-enabled variants, all methods follow the same convention: sampling
 * decisions and probability densities are detached, while values evaluated at
 * those fixed samples are attached. Sampling routines thus return the attached
 * value divided by the detached density. Emitter samples and ray intersections
 * (by default) treat the ray as fixed and produce hits that slide along the
 * intersected surface.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Scene final : public JitObject<Scene<Float, Spectrum>> {
public:
    MI_IMPORT_TYPES(BSDF, Emitter, EmitterPtr, SensorPtr, Film, Sampler, Shape,
                    ShapePtr, ShapeGroup, Sensor, Integrator, Medium, MediumPtr,
                    Mesh)

    /// Instantiate a scene from a `Properties` object
    Scene(const Properties &props);

    /// Destructor
    ~Scene();

    // =============================================================
    // Ray tracing
    // =============================================================

    /**
     * Intersect a ray with the shapes comprising the scene and return a
     * detailed data structure describing the intersection, if one is found.
     *
     * This convenience overload traces incoherent secondary rays with
     * `RayFlags.Default` and no thread reordering. See the general overload
     * of ``ray_intersect()`` below for details.
     *
     * Args:
     *     ray: A 3D ray including maximum extent (`Ray3f.maxt`) and time
     *         (`Ray3f.time`) information, which matters when the shapes are in motion
     *
     * Returns:
     *     A detailed surface interaction record. Its ``is_valid()`` method
     *     should be queried to check if an intersection was actually found.
     */
    SurfaceInteraction3f ray_intersect(const Ray3f &ray,
                                       Mask active = true) const {
        return ray_intersect(ray, +RayFlags::Default, false,
                             (uint32_t) RayMask::Secondary, active);
    }

    /**
     * Intersect a ray with the shapes comprising the scene and return a
     * detailed data structure describing the intersection, if one is found
     *
     * In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``),
     * the function processes arrays of rays and returns arrays of surface
     * interactions following the usual conventions.
     *
     * Internally, the intersection is split into two steps:
     *
     * #. Finding a `PreliminaryIntersection3f` using the ray tracing
     *    backend underlying the current variant (i.e., Mitsuba's builtin
     *    kd-tree, Embree, or OptiX). This is done using the
     *    `ray_intersect_preliminary()` function that is also available
     *    directly below (and preferable if a full `SurfaceInteraction3f`
     *    is not needed.).
     *
     * #. Expanding the `PreliminaryIntersection3f` into a full
     *    `SurfaceInteraction3f` (this part happens within Mitsuba/Dr.Jit
     *    and tracks derivative information in AD variants of the system).
     *
     * The `SurfaceInteraction3f` data structure is large, and computing its
     * contents in the second step requires a non-trivial amount of computation
     * and sequence of memory accesses. The ``ray_flags`` parameter can be used
     * to specify that only a sub-set of the full intersection data structure
     * actually needs to be computed, which can improve performance.
     *
     * In the context of differentiable rendering, the ``ray_flags`` parameter
     * also influences how derivatives propagate between the input ray, the
     * shape parameters, and the computed intersection (see
     * `RayFlags.FollowShape` and `RayFlags.DetachShape` for details on
     * this). The default, `RayFlags.Default`, propagates derivatives through
     * all steps of the intersection computation.
     *
     * Args:
     *     ray: A 3D ray including maximum extent (`Ray3f.maxt`) and time
     *         (`Ray3f.time`) information, which matters when the shapes are in motion
     *
     *     ray_flags: An integer combining flag bits from `RayFlags` (merged using
     *         binary or).
     *
     *     coherent: Setting this flag to ``True`` can noticeably improve performance when
     *         ``ray`` contains a coherent set of rays (e.g. primary camera rays),
     *         and when using ``llvm_*`` variants of the renderer along with
     *         Embree. It has no effect in scalar or CUDA/OptiX variants.
     *
     *     ray_mask: Ray-side visibility mask (see `RayMask`). A shape can
     *         only be intersected when this value matches its
     *         `Shape.visibility()` class. The default, `RayMask.Secondary`,
     *         matches every shape that is visible to secondary rays. Camera
     *         rays should pass `RayMask.Primary`.
     *
     *     reorder: Setting this flag to ``True`` will trigger a reordering of the threads
     *         using the GPU's Shader Execution Reordering (SER) functionality if the
     *         scene's ``allow_thread_reordering`` flag was also set. This flag has no
     *         effect in scalar or LLVM variants.
     *
     *     reorder_hint: The reordering will always shuffle the threads based on the shape
     *         the thread's ray intersected. However, additional granularity can be
     *         achieved by providing an extra sorting key with this parameter.
     *         This flag has no effect in scalar or LLVM variants, or if the
     *         ``reorder`` parameter is ``False``.
     *
     *     reorder_hint_bits: Number of bits from the ``reorder_hint`` to use (starting from the
     *         least significant bit). It is recommended to use as few as possible.
     *         At most, 16 bits can be used. This flag has no effect in scalar or
     *         LLVM variants, or if the ``reorder`` parameter is ``False``.
     *
     * Returns:
     *     A detailed surface interaction record. Its ``is_valid()`` method
     *     should be queried to check if an intersection was actually found.
     */
    SurfaceInteraction3f ray_intersect(const Ray3f &ray,
                                       uint32_t ray_flags,
                                       Mask coherent = false,
                                       UInt32 ray_mask = (uint32_t) RayMask::Secondary,
                                       Mask active = true,
                                       bool reorder = false,
                                       UInt32 reorder_hint = 0,
                                       uint32_t reorder_hint_bits = 0) const;

    /**
     * Expand a preliminary intersection into a detailed surface interaction
     *
     * This function turns a `PreliminaryIntersection3f` into a
     * `SurfaceInteraction3f`, which provides a richer description of the
     * intersection's differentiable geometry.
     *
     * Args:
     *     ray: Ray associated with the preliminary ray intersection ``pi``
     *
     *     pi: Preliminary intersection to be expanded
     *
     *     ray_flags: An integer combining flag bits from `RayFlags` (merged
     *         using binary or).
     *
     * Returns:
     *     A detailed surface interaction record. Its ``is_valid()`` method
     *     should be queried to check if an intersection was actually found.
     */
    SurfaceInteraction3f compute_surface_interaction(
        const Ray3f &ray, const PreliminaryIntersection3f &pi,
        uint32_t ray_flags = +RayFlags::Default, Mask active = true) const;

    /// Return the ``instance`` shape with the given index.
    const Shape *instance(size_t index) const { return m_instances[index]; }

    /**
     * Intersect a ray with the shapes comprising the scene and return a
     * boolean specifying whether or not an intersection was found.
     *
     * This convenience overload traces incoherent secondary rays. See the
     * general overload of ``ray_test()`` below for details.
     *
     * Args:
     *     ray: A 3D ray including maximum extent (`Ray3f.maxt`) and time
     *         (`Ray3f.time`) information, which matters when the shapes are in motion
     *
     * Returns:
     *     ``True`` if an intersection was found
     */
    Mask ray_test(const Ray3f &ray, Mask active = true) const {
        return ray_test(ray, false, (uint32_t) RayMask::Secondary, active);
    }

    /**
     * Intersect a ray with the shapes comprising the scene and return a
     * boolean specifying whether or not an intersection was found.
     *
     * In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``),
     * the function processes arrays of rays and returns arrays of booleans
     * following the usual conventions.
     *
     * Testing for the mere presence of intersections is considerably faster
     * than finding an actual intersection, hence this function should be
     * preferred over `ray_intersect()` when geometric information about the
     * first visible intersection is not needed.
     *
     * Args:
     *     ray: A 3D ray including maximum extent (`Ray3f.maxt`) and time
     *         (`Ray3f.time`) information, which matters when the shapes are in motion
     *
     *     coherent: Setting this flag to ``True`` can noticeably improve performance when
     *         ``ray`` contains a coherent set of rays (e.g. primary camera rays),
     *         and when using ``llvm_*`` variants of the renderer along with
     *         Embree. It has no effect in scalar or CUDA/OptiX variants.
     *
     *     ray_mask: Ray-side visibility mask (see `RayMask`). A shape can
     *         only occlude the ray when this value matches its
     *         `Shape.visibility()` class.
     *
     * Returns:
     *     ``True`` if an intersection was found
     */
    Mask ray_test(const Ray3f &ray, Mask coherent,
                  UInt32 ray_mask = (uint32_t) RayMask::Secondary,
                  Mask active = true) const;

    /**
     * Shadow ray test that passes through surfaces with null transmission
     *
     * Shapes with a `BSDFFlags.Null` component do not occlude the ray. The
     * method instead returns the product of their `BSDF.eval_null()` values
     * along the segment, which is zero when an opaque shape blocks the ray.
     * Participating media are ignored. Only the differentiation bits of
     * ``ray_flags`` are used. See `ray_test()` for further detail on the
     * function's arguments.
     *
     * Returns:
     *     The transmittance along the segment. In polarized rendering modes,
     *     this is a Mueller matrix in world coordinates whose factors are
     *     ordered like the throughput of the path tracer: the surface
     *     closest to the ray origin is the leftmost factor.
     */
    Spectrum ray_test_tr(const Ray3f &ray,
                         uint32_t ray_flags = (uint32_t) RayFlags::Default,
                         Mask coherent = false,
                         UInt32 ray_mask = (uint32_t) RayMask::Secondary,
                         Mask active = true) const;

    /**
     * Ray intersection that passes through surfaces with null transmission
     *
     * This method finds the closest opaque or emissive surface along the ray
     * and returns it together with the transmittance of the null surfaces in
     * front of it (see `ray_test_tr()`). Integrators use it for rays that
     * should reach the same emitters as shadow rays. See `ray_intersect()`
     * for further detail on the function's arguments.
     *
     * Returns:
     *     A pair ``(si, tr)`` of the surface interaction and the
     *     transmittance up to it
     */
    std::pair<SurfaceInteraction3f, Spectrum>
    ray_intersect_tr(const Ray3f &ray,
                     uint32_t ray_flags = (uint32_t) RayFlags::Default,
                     Mask coherent = false,
                     UInt32 ray_mask = (uint32_t) RayMask::Secondary,
                     Mask active = true) const;

    /**
     * Intersect a ray with the shapes comprising the scene and return
     * preliminary information, if one is found
     *
     * This function invokes the ray tracing backend underlying the current
     * variant (i.e., Mitsuba's builtin kd-tree, Embree, or OptiX) and returns
     * preliminary intersection information consisting of
     *
     * * the ray distance up to the intersection (if one is found).
     *
     * * the intersected shape and primitive index.
     *
     * * local UV coordinates of the intersection within the primitive.
     *
     * * A pointer to the intersected shape or instance.
     *
     * The information is only preliminary at this point, because it lacks
     * various other information (geometric and shading frame, texture
     * coordinates, curvature, etc.) that is generally needed by shading
     * models. In variants of Mitsuba that perform automatic differentiation,
     * it is important to know that computation done by the ray tracing
     * backend is not reflected in Dr.Jit's computation graph. The
     * `ray_intersect()` method will re-evaluate certain parts of the computation
     * with derivative tracking to rectify this.
     *
     * In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``),
     * the function processes arrays of rays and returns arrays of preliminary
     * intersection records following the usual conventions.
     *
     * Args:
     *     ray: A 3D ray including maximum extent (`Ray3f.maxt`) and time
     *         (`Ray3f.time`) information, which matters when the shapes are in motion
     *
     *     coherent: Setting this flag to ``True`` can noticeably improve performance when
     *         ``ray`` contains a coherent set of rays (e.g. primary camera rays),
     *         and when using ``llvm_*`` variants of the renderer along with
     *         Embree. It has no effect in scalar or CUDA/OptiX variants.
     *
     *     ray_mask: Ray-side visibility mask (see `RayMask`). A shape can
     *         only be intersected when this value matches its
     *         `Shape.visibility()` class. The default, `RayMask.Secondary`,
     *         matches every shape that is visible to secondary rays. Camera
     *         rays should pass `RayMask.Primary`.
     *
     *     reorder: Setting this flag to ``True`` will trigger a reordering of the threads
     *         using the GPU's Shader Execution Reordering (SER) functionality if the
     *         scene's ``allow_thread_reordering`` flag was also set. This flag has
     *         no effect in scalar or LLVM variants.
     *
     *     reorder_hint: The reordering will always shuffle the threads based on the shape
     *         the thread's ray intersected. However, additional granularity can be
     *         achieved by providing an extra sorting key with this parameter.
     *         This flag has no effect in scalar or LLVM variants, or if the
     *         ``reorder`` parameter is ``False``.
     *
     *     reorder_hint_bits: Number of bits from the ``reorder_hint`` to use (starting from the
     *         least significant bit). It is recommended to use as few as possible.
     *         At most, 16 bits can be used. This flag has no effect in scalar or
     *         LLVM variants, or if the ``reorder`` parameter is ``False``.
     *
     * Returns:
     *     A preliminary surface interaction record. Its ``is_valid()`` method
     *     should be queried to check if an intersection was actually found.
     */
    PreliminaryIntersection3f ray_intersect_preliminary(
        const Ray3f &ray,
        Mask coherent = false,
        UInt32 ray_mask = (uint32_t) RayMask::Secondary,
        Mask active = true,
        bool reorder = false,
        UInt32 reorder_hint = 0,
        uint32_t reorder_hint_bits = 0) const;

    /**
     * Ray intersection using a brute force search. Used in
     * unit tests to validate the kdtree-based ray tracer.
     *
     * Note:
     *     Not implemented by the Embree/OptiX backends
     */
    SurfaceInteraction3f ray_intersect_naive(const Ray3f &ray,
                                             Mask active = true) const;

    // =============================================================

    // =============================================================
    // Emitter sampling interface
    // =============================================================

    /**
     * Sample one emitter in the scene and rescale the input sample
     * for reuse.
     *
     * The emitters are chosen proportionally to `Emitter.sampling_weight()`,
     * which is zero for emitters hidden from secondary rays. When no emitter
     * can be sampled, the returned index is ``-1`` and the weight is zero.
     *
     * Args:
     *     sample: A uniformly distributed number in [0, 1).
     *
     * Returns:
     *     The index of the chosen emitter along with the sampling weight (equal
     *     to the inverse PDF), and the transformed random sample for reuse.
     */
    std::tuple<UInt32, Float, Float>
    sample_emitter(Float index_sample, Mask active = true) const;

    /**
     * Evaluate the discrete probability of the
     * `sample_emitter()` technique for the given a emitter index.
     */
    Float pdf_emitter(UInt32 index, Mask active = true) const;

    /**
     * Sample a ray according to the emission profile of scene emitters
     *
     * This function combines both steps of choosing a ray origin on a light
     * source and an outgoing ray direction. It does not return any auxiliary
     * sampling information and is mainly meant to be used by unidirectional
     * rendering techniques like particle tracing.
     *
     * Sampling is ideally perfectly proportional to the emission profile,
     * though approximations are acceptable as long as these are reflected
     * in the returned Monte Carlo sampling weight.
     *
     * Args:
     *     time: The scene time associated with the ray to be sampled.
     *
     *     sample1: A uniformly distributed 1D value that is used to sample the spectral
     *         dimension of the emission profile.
     *
     *     sample2: A uniformly distributed sample on the domain :math:`[0,1]^2`.
     *
     *     sample3: A uniformly distributed sample on the domain :math:`[0,1]^2`.
     *
     * Returns:
     *     A tuple ``(ray, weight, emitter)``, where
     *
     *
     *     * ``ray`` is the sampled ray (e.g. starting on the surface of an
     *       area emitter)
     *
     *     * ``weight`` returns the emitted radiance divided by the
     *       spatio-directional sampling density
     *
     *     * ``emitter`` is a pointer specifying the sampled emitter
     */
    std::tuple<Ray3f, Spectrum, const EmitterPtr>
    sample_emitter_ray(Float time, Float sample1, const Point2f &sample2,
                       const Point2f &sample3, Mask active = true) const;

    /**
     * Direct illumination sampling routine
     *
     * This method implements stochastic connections to emitters, which is
     * variously known as *emitter sampling*, *direct illumination
     * sampling*, or *next event estimation*.
     *
     * The function expects a 3D reference location ``ref`` as input, which may
     * influence the sampling process. Normally, this would be the location of
     * a surface position being shaded. Ideally, the implementation of this
     * function should then draw samples proportional to the scene's emission
     * profile and the inverse square distance between the reference point and
     * the sampled emitter position. However, approximations are acceptable as
     * long as these are reflected in the returned Monte Carlo sampling weight.
     *
     * Args:
     *     ref: A 3D reference location within the scene, which may influence the
     *         sampling process.
     *
     *     sample: A uniformly distributed 2D random variate
     *
     *     test_visibility: When set to ``True``, a shadow ray will be cast to ensure that the
     *         sampled emitter position and the reference point are mutually visible.
     *         The shadow ray passes through surfaces with null transmission and
     *         the returned weight includes their transmittance (see `ray_test_tr()`).
     *
     * Returns:
     *     A tuple ``(ds, spec)`` where
     *
     *
     *     * ``ds`` is a fully populated `DirectionSample3f` data
     *       structure, which provides further detail about the sampled
     *       emitter position (e.g. its surface normal, solid angle density,
     *       whether Dirac delta distributions were involved, etc.)
     *
     *     * ``spec`` is a Monte Carlo sampling weight specifying the ratio
     *       of the radiance incident from the emitter and the sample
     *       probability per unit solid angle.
     */
    std::pair<DirectionSample3f, Spectrum>
    sample_emitter_direction(const Interaction3f &ref,
                             const Point2f &sample,
                             bool test_visibility = true,
                             Mask active = true) const;

    /**
     * Evaluate the PDF of direct illumination sampling
     *
     * This function evaluates the probability density (per unit solid angle)
     * of the sampling technique implemented by the
     * `sample_emitter_direction()` function. The returned probability will always
     * be zero when the emission profile contains a Dirac delta term (e.g.
     * point or directional emitters/sensors).
     *
     * Args:
     *     ref: A 3D reference location within the scene, which may influence the
     *         sampling process.
     *
     *     ds: A direction sampling record, which specifies the query location.
     *
     * Returns:
     *     The solid angle density of the sample
     */
    Float pdf_emitter_direction(const Interaction3f &ref,
                                const DirectionSample3f &ds,
                                Mask active = true) const;

    /**
     * Re-evaluate the incident direct radiance of the
     * `sample_emitter_direction()` method.
     *
     * This function re-evaluates the incident direct radiance and sample
     * probability due to the emitter so that division by ``ds.pdf``
     * equals the sampling weight returned by `sample_emitter_direction()`.
     * This may appear redundant, and indeed such a function would not find use
     * in "normal" rendering algorithms.
     *
     * However, the ability to re-evaluate the contribution of a direct
     * illumination sample is important for differentiable rendering. For
     * example, we might want to track derivatives in the sampled direction
     * (``ds.d``) without also differentiating the sampling technique.
     *
     * In contrast to `pdf_emitter_direction()`, evaluating this function can
     * yield a nonzero result in the case of emission profiles containing a
     * Dirac delta term (e.g. point or directional lights).
     *
     * Args:
     *     ref: A 3D reference location within the scene, which may influence the
     *         sampling process.
     *
     *     ds: A direction sampling record, which specifies the query location.
     *
     * Returns:
     *     The incident radiance and discrete or solid angle density of the
     *     sample.
     */
    Spectrum eval_emitter_direction(const Interaction3f &ref,
                                    const DirectionSample3f &ds,
                                    Mask active = true) const;

    // =============================================================

    // =============================================================
    // Silhouette sampling interface
    // =============================================================

    /**
     * Map a point sample in boundary sample space to a silhouette
     * segment
     *
     * This method will sample a `SilhouetteSample3f` object from all the
     * shapes in the scene that are being differentiated and have non-zero
     * sampling weight (see `Shape.silhouette_sampling_weight()`).
     *
     * Args:
     *     sample: The boundary space sample (a point in the unit cube).
     *
     *     flags: Flags to select the type of silhouettes to sample from (see
     *         `DiscontinuityFlags`). Multiple types of discontinuities can be
     *         sampled in a single call.
     *         If a single type of silhouette is specified, shapes that do not have
     *         that types might still be sampled. In which case, the
     *         `SilhouetteSample3f` field ``discontinuity_type`` will be
     *         `DiscontinuityFlags.Empty`.
     *
     * Returns:
     *     Silhouette sample record.
     */
    SilhouetteSample3f sample_silhouette(const Point3f &sample,
                                         uint32_t flags,
                                         Mask active = true) const;

    /**
     * Map a silhouette segment to a point in boundary sample space
     *
     * This method is the inverse of `sample_silhouette()`. The mapping
     * from boundary sample space to boundary segments is bijective.
     *
     * Args:
     *     ss: The sampled boundary segment
     *
     * Returns:
     *     The corresponding boundary sample space point
     */
    Point3f invert_silhouette_sample(const SilhouetteSample3f &ss,
                                     Mask active = true) const;

    // =============================================================

    // =============================================================
    // Accessors
    // =============================================================

    /// Return a bounding box surrounding the scene
    const ScalarBoundingBox3f &bbox() const { return m_bbox; }

    /// Return the list of sensors
    std::vector<ref<Sensor>> &sensors() { return m_sensors; }
    /// Return the list of sensors (const version)
    const std::vector<ref<Sensor>> &sensors() const { return m_sensors; }

    /// Return the list of emitters
    std::vector<ref<Emitter>> &emitters() { return m_emitters; }
    /// Return the list of emitters (const version)
    const std::vector<ref<Emitter>> &emitters() const { return m_emitters; }

    /// Return the environment emitter (if any)
    const Emitter *environment() const { return m_environment.get(); }

    /// Return the list of shapes
    std::vector<ref<Shape>> &shapes() { return m_shapes; }
    /// Return the list of shapes
    const std::vector<ref<Shape>> &shapes() const { return m_shapes; }

    /// Return the list of shape groups
    std::vector<ref<ShapeGroup>> &shapegroups() { return m_shapegroups; }
    /// Return the list of shape groups
    const std::vector<ref<ShapeGroup>> &shapegroups() const { return m_shapegroups; }

    /// Return the list of shapes that can have their silhouette sampled
    const std::vector<ref<Shape>> &silhouette_shapes() const { return m_silhouette_shapes; }

    /// Return the list of light portals
    const std::vector<ref<Emitter>> &portals() const { return m_portals; }

    /**
     * Packed light portal records
     *
     * Environment emitters gather from this buffer so that their kernels do
     * not depend on the number of portals. Each record holds 12 entries: the
     * rectangle's center, its two orthogonal half-edge vectors, and its unit
     * normal, which points into the region that receives light.
     */
    struct PortalData {
        DynamicBuffer<Float> records;
        field<UInt32> count = 0u;
        /// Probability of sampling the portals rather than the emitter's own strategy
        ScalarFloat weight = .5f;

        DRJIT_TRAVERSE(PortalData, records, count)
    };

    /// Return the light portal records
    const PortalData &portal_data() const { return m_portal_data; }

    /// Return the scene's `Integrator`
    Integrator* integrator() { return m_integrator; }
    /// Return the scene's `Integrator`
    const Integrator* integrator() const { return m_integrator; }

    /// Return the list of emitters as a Dr.Jit array
    const DynamicBuffer<EmitterPtr> &emitters_dr() const { return m_emitters_dr; }

    /// Return the list of shapes as a Dr.Jit array
    const DynamicBuffer<ShapePtr> &shapes_dr() const { return m_shapes_dr; }

    /// Return the list of sensors as a Dr.Jit array
    const DynamicBuffer<SensorPtr> &sensors_dr() const { return m_sensors_dr; }

    // =============================================================

    /// Traverse the scene graph and invoke the given callback for each object
    void traverse(TraversalCallback *callback) override;

    /// Update internal state following a parameter update
    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override;

    /**
     * Specifies whether any of the scene's shape parameters have
     * gradient tracking enabled
     */
    bool shapes_grad_enabled() const { return m_shapes_grad_enabled; };

    /// Returns a union of ShapeType flags denoting what is present in the scene
    uint32_t shape_types() const;

    /// Does the scene contain shapes of the `RayMask.Null` classes?
    bool has_null_shapes() const { return m_has_null_shapes; }

    /**
     * \brief Should the BVH builder compact the acceleration data structure?
     *
     * BVH Compaction can significantly reduce memory usage but also requires
     * device <-> host synchronization. If ``m_compact_accel_auto`` is set,
     * only compact on the first build and switch to non-compacting builds later
     * to avoid the sync cost in inverse rendering optimization iterations.
     */
    bool compact_accel();

    /// Return a human-readable string representation of the scene contents.
    virtual std::string to_string() const override;

    /// Static initialization of ray-intersection acceleration data structure
    static void static_accel_initialization();

    /// Static shutdown of ray-intersection acceleration data structure
    static void static_accel_shutdown();

    MI_DECLARE_PLUGIN_BASE_CLASS(Scene)

protected:
    /// Clears the dirty flag of every shape and refreshes has_null_shapes()
    void clear_shapes_dirty();

    /**
     * Walk along ``ray`` and accumulate transmittance due to surfaces with
     * ``null`` BSDFs
     *
     * The walk visits the shapes matching ``ray_mask`` from front to back
     * and accumulates the product of their `BSDF.eval_null()` values.
     *
     * Args:
     *     ray: The ray to follow.
     *     ray_flags: Only the `RayFlags.FollowShape` and
     *         `RayFlags.DetachShape` bits are used. It influences
     *         the derivative computation of intersected surfaces.
     *     ray_mask: Visibility mask of the shapes to consider.
     *     stop_at_surface: When ``true``, the walk continues until it
     *         encounters a shape that is opaque or an emitter.
     *         When ``false``, it continues until no more intersections
     *         can be found or when the transmittance reaches zero.
     *         The caller should restrict ``ray_mask`` to null shapes
     *         in this cae, and the returned intersection is always invalid.
     *     active: Active mask
     *
     * Returns:
     *     A pair ``(pi, tr)`` of the preliminary intersection and the
     *     transmittance product.
     */
    std::pair<PreliminaryIntersection3f, Spectrum>
    null_walk(const Ray3f &ray,
              uint32_t ray_flags,
              UInt32 ray_mask,
              bool stop_at_surface,
              Mask active) const;

    /// Repack the per-instance transform records (see below)
    void update_instance_transforms();

    /// Build the light portal records from ``m_portals``
    void update_portal_data();

    using ShapeKDTree = mitsuba::ShapeKDTree<Float, Spectrum>;

    /// Updates the discrete distribution used to select an emitter
    void update_emitter_sampling_distribution();

    /// Updates the discrete distribution used to select a shape's silhouette
    void update_silhouette_sampling_distribution();

protected:
    /// Backend-specific acceleration data structure state
    SceneAccel<Float, Spectrum> m_accel;

    ScalarBoundingBox3f m_bbox;

    std::vector<ref<Emitter>> m_emitters;
    DynamicBuffer<EmitterPtr> m_emitters_dr;

    std::vector<ref<Shape>> m_shapes;
    DynamicBuffer<ShapePtr> m_shapes_dr;
    std::vector<ref<ShapeGroup>> m_shapegroups;

    /// Light portals, excluded from ``m_emitters`` and never sampled directly
    std::vector<ref<Emitter>> m_portals;
    PortalData m_portal_data;

    std::vector<ref<Sensor>> m_sensors;
    DynamicBuffer<SensorPtr> m_sensors_dr;

    std::vector<ref<Object>> m_children;
    ref<Integrator> m_integrator;
    ref<Emitter> m_environment;

    /// Uniform emitter selection probability. Zero when the scene has no
    /// emitter that sample_emitter() could pick.
    ScalarFloat m_emitter_pmf;
    std::unique_ptr<DiscreteDistribution<Float>> m_emitter_distr = nullptr;

    std::vector<ref<Shape>> m_silhouette_shapes;
    DynamicBuffer<ShapePtr> m_silhouette_shapes_dr;
    std::unique_ptr<DiscreteDistribution<Float>> m_silhouette_distr = nullptr;

    bool m_shapes_grad_enabled;
    bool m_thread_reordering;
    /// Compact GPU acceleration structures after building?
    bool m_compact_accel;
    /// Has an acceleration structure build already taken place?
    bool m_accel_built = false;
    /// Enable/disable automatic BVH compaction criterion in compact_accel().
    bool m_compact_accel_auto;
    /// Does the scene contain shapes with 'null' BSDFs?
    bool m_has_null_shapes = false;

    /// Instances in order of appearance in ``m_shapes``.
    std::vector<const Shape *> m_instances;

    /// Flattened sequence of instance ``to_world`` matrices (12 floats each)
    DynamicBuffer<Float> m_instance_transforms;

    /// Instancing-aware expansion of a preliminary intersection (see
    /// ``compute_surface_interaction()``, which forwards here when the
    /// record may reference instanced geometry)
    SurfaceInteraction3f compute_surface_interaction_instanced(
        const Ray3f &ray, const PreliminaryIntersection3f &pi,
        uint32_t ray_flags, Mask active) const;

    // The Accel class needs to access the scene's protected members.
    friend SceneAccel<Float, Spectrum>;

    MI_DECLARE_TRAVERSE_CB(m_accel, m_emitters, m_emitters_dr, m_shapes,
                           m_shapes_dr, m_shapegroups, m_portals,
                           m_portal_data, m_sensors, m_sensors_dr,
                           m_children, m_integrator, m_environment,
                           m_emitter_pmf, m_emitter_distr, m_silhouette_shapes,
                           m_silhouette_shapes_dr, m_silhouette_distr,
                           m_instance_transforms)
};

// See interaction.h
template <typename Float, typename Spectrum>
typename SurfaceInteraction<Float, Spectrum>::EmitterPtr
SurfaceInteraction<Float, Spectrum>::emitter(
        const Scene *scene, dr::uint32_array_t<Float> ray_mask,
        Mask active) const {
    if constexpr (!dr::is_jit_v<Float>) {
        DRJIT_MARK_USED(active);
        if (is_valid())
            return shape->emitter();
        // Without a shape, the environment's mask is its ShapeVisibility value
        const Emitter *env = scene->environment();
        return (env && (ray_mask & (uint32_t) env->visibility()) != 0)
                   ? env : nullptr;
    } else {
        EmitterPtr emitter = shape->emitter(active);
        const Emitter *env = scene ? scene->environment() : nullptr;
        if (env) {
            uint32_t env_class = (uint32_t) env->visibility();
            Mask env_visible = active && ((ray_mask & env_class) != 0u);
            emitter = dr::select(is_valid(), emitter, env & env_visible);
        }
        return emitter;
    }
}

MI_EXTERN_CLASS(Scene)
NAMESPACE_END(mitsuba)
