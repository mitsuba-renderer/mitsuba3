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
     * In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``),
     * the function processes arrays of rays and returns arrays of surface
     * interactions following the usual conventions.
     *
     * This method is a convenience wrapper of the generalized version of
     * ``ray_intersect()`` below. It assumes that incoherent rays are being traced,
     * that the user desires access to all fields of the
     * `SurfaceInteraction3f`, and that no thread reordering is requested. In
     * other words, it simply invokes the general ``ray_intersect()`` overload
     * with ``coherent=false``, ``ray_flags`` equal to `RayFlags.Default`,
     * and ``reorder=false``.
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
        return ray_intersect(ray, +RayFlags::Default, false, false, 0, 0, active);
    }

    /**
     * Intersect a ray with the shapes comprising the scene and return a
     * detailed data structure describing the intersection, if one is found
     *
     * In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``),
     * the function processes arrays of rays and returns arrays of surface
     * interactions following the usual conventions.
     *
     * This ray intersection method exposes two additional flags to control the
     * intersection process. Internally, it is split into two steps:
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
     * The ``coherent`` flag is a hint that can improve performance in the first
     * step of finding the `PreliminaryIntersection3f` if the input set of rays
     * is coherent (e.g., when they are generated by `Sensor.sample_ray()`,
     * which means that adjacent rays will traverse essentially the same region
     * of space). This flag is currently only used by the combination of
     * ``llvm_*`` variants and the Embree ray tracing backend.
     *
     * This method is a convenience wrapper of the generalized
     * ``ray_intersect()`` method below. It assumes that ``reorder=false``.
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
     *     visibility_mask: Ray-side visibility mask (see `RayMask`). A shape
     *         can only be intersected when the bitwise AND of this value and
     *         the shape's `Shape.visibility_mask()` is nonzero. The default,
     *         `RayMask.All`, matches every shape; camera rays should pass
     *         `RayMask.Camera` so that emitters flagged as invisible are
     *         skipped.
     *
     * Returns:
     *     A detailed surface interaction record. Its ``is_valid()`` method
     *     should be queried to check if an intersection was actually found.
     */
    SurfaceInteraction3f ray_intersect(const Ray3f &ray,
                                       uint32_t ray_flags,
                                       Mask coherent,
                                       Mask active = true,
                                       const UInt32 &visibility_mask
                                           = (uint32_t) RayMask::All) const {
        return ray_intersect(ray, ray_flags, coherent, false, 0, 0, active,
                             visibility_mask);
    }

    /**
     * Intersect a ray with the shapes comprising the scene and return a
     * detailed data structure describing the intersection, if one is found
     *
     * In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``),
     * the function processes arrays of rays and returns arrays of surface
     * interactions following the usual conventions.
     *
     * This generalized ray intersection method exposes two additional flags to
     * control the intersection process. Internally, it is split into two
     * steps:
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
     * The ``coherent`` flag is a hint that can improve performance in the first
     * step of finding the `PreliminaryIntersection3f` if the input set of rays
     * is coherent (e.g., when they are generated by `Sensor.sample_ray()`,
     * which means that adjacent rays will traverse essentially the same region
     * of space). This flag is currently only used by the combination of
     * ``llvm_*`` variants and the Embree ray tracing backend.
     *
     * The ``reorder`` flag is a trigger for the Shader Execution Reordering (SER)
     * feature on NVIDIA GPUs. It can improve performance in highly divergent
     * workloads by shuffling threads into coherent warps. This shuffling
     * operation uses the result of the intersection (the shape ID) as a sorting
     * key to group threads into coherent warps.
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
     *     visibility_mask: Ray-side visibility mask (see `RayMask`). A shape
     *         can only be intersected when the bitwise AND of this value and
     *         the shape's `Shape.visibility_mask()` is nonzero. The default,
     *         `RayMask.All`, matches every shape; camera rays should pass
     *         `RayMask.Camera` so that emitters flagged as invisible are
     *         skipped.
     *
     * Returns:
     *     A detailed surface interaction record. Its ``is_valid()`` method
     *     should be queried to check if an intersection was actually found.
     */
    SurfaceInteraction3f ray_intersect(const Ray3f &ray,
                                       uint32_t ray_flags,
                                       Mask coherent,
                                       bool reorder,
                                       UInt32 reorder_hint,
                                       uint32_t reorder_hint_bits,
                                       Mask active = true,
                                       const UInt32 &visibility_mask
                                           = (uint32_t) RayMask::All) const;

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
     * In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``),
     * the function processes arrays of rays and returns arrays of booleans
     * following the usual conventions.
     *
     * Testing for the mere presence of intersections is considerably faster
     * than finding an actual intersection, hence this function should be
     * preferred over `ray_intersect()` when geometric information about the
     * first visible intersection is not needed.
     *
     * This method is a convenience wrapper of the generalized version of ``ray_test()`` below, which assumes that incoherent rays are being traced.
     * In other words, it simply invokes the general ``ray_test()`` overload
     * with ``coherent=false``.
     *
     * Args:
     *     ray: A 3D ray including maximum extent (`Ray3f.maxt`) and time
     *         (`Ray3f.time`) information, which matters when the shapes are in motion
     *
     * Returns:
     *     ``True`` if an intersection was found
     */
    Mask ray_test(const Ray3f &ray, Mask active = true) const {
        return ray_test(ray, false, active);
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
     * The ``coherent`` flag is a hint that can improve performance in the first
     * step of finding the `PreliminaryIntersection3f` if the input set of rays
     * is coherent, which means that adjacent rays will traverse essentially
     * the same region of space. This flag is currently only used by the
     * combination of ``llvm_*`` variants and the Embree ray tracing
     * backend.
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
     *     visibility_mask: Ray-side visibility mask (see `RayMask`). A shape
     *         can only occlude the ray when the bitwise AND of this value and
     *         the shape's `Shape.visibility_mask()` is nonzero.
     *
     * Returns:
     *     ``True`` if an intersection was found
     */
    Mask ray_test(const Ray3f &ray, Mask coherent, Mask active,
                  const UInt32 &visibility_mask
                      = (uint32_t) RayMask::All) const;

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
     * This method is a convenience wrapper of the generalized version of ``ray_intersect_preliminary()`` below, which assumes that no reordering is
     * requested. In other words, it simply invokes the general
     * ``ray_intersect_preliminary()`` overload with ``reorder=false``.
     *
     * The ``coherent`` flag is a hint that can improve performance if the input
     * set of rays is coherent (e.g., when they are generated by
     * `Sensor.sample_ray()`, which means that adjacent rays will traverse
     * essentially the same region of space). This flag is currently only used
     * by the combination of ``llvm_*`` variants and the Embree ray
     * intersector.
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
     * Returns:
     *     A preliminary surface interaction record. Its ``is_valid()`` method
     *     should be queried to check if an intersection was actually found.
     */
    PreliminaryIntersection3f ray_intersect_preliminary(const Ray3f &ray,
                                                        Mask coherent = false,
                                                        Mask active = true,
                                                        const UInt32 &visibility_mask
                                                            = (uint32_t) RayMask::All) const {
        return ray_intersect_preliminary(ray, coherent, false, 0, 0, active,
                                         visibility_mask);
    }

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
     * The ``coherent`` flag is a hint that can improve performance if the input
     * set of rays is coherent (e.g., when they are generated by
     * `Sensor.sample_ray()`, which means that adjacent rays will traverse
     * essentially the same region of space). This flag is currently only used
     * by the combination of ``llvm_*`` variants and the Embree ray
     * intersector.
     *
     * The ``reorder`` flag is a trigger for the Shader Execution Reordering (SER)
     * feature on NVIDIA GPUs. It can improve performance in highly divergent
     * workloads by shuffling threads into coherent warps. This shuffling
     * operation uses the result of the intersection (the shape ID) as a sorting
     * key to group threads into coherent warps.
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
     *     visibility_mask: Ray-side visibility mask (see `RayMask`). A shape
     *         can only be intersected when the bitwise AND of this value and
     *         the shape's `Shape.visibility_mask()` is nonzero. The default,
     *         `RayMask.All`, matches every shape; camera rays should pass
     *         `RayMask.Camera` so that emitters flagged as invisible are
     *         skipped.
     *
     * Returns:
     *     A preliminary surface interaction record. Its ``is_valid()`` method
     *     should be queried to check if an intersection was actually found.
     */
    PreliminaryIntersection3f ray_intersect_preliminary(const Ray3f &ray,
                                                        Mask coherent,
                                                        bool reorder,
                                                        UInt32 reorder_hint,
                                                        uint32_t reorder_hint_bits,
                                                        Mask active = true,
                                                        const UInt32 &visibility_mask
                                                            = (uint32_t) RayMask::All) const;

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
     * Currently, the sampling scheme implemented by the `Scene` class is
     * very simplistic (uniform).
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
    /// Unmarks all shapes as dirty
    void clear_shapes_dirty();

    /// Repack the per-instance transform records (see below)
    void update_instance_transforms();

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

    std::vector<ref<Sensor>> m_sensors;
    DynamicBuffer<SensorPtr> m_sensors_dr;

    std::vector<ref<Object>> m_children;
    ref<Integrator> m_integrator;
    ref<Emitter> m_environment;

    ScalarFloat m_emitter_pmf;
    std::unique_ptr<DiscreteDistribution<Float>> m_emitter_distr = nullptr;

    std::vector<ref<Shape>> m_silhouette_shapes;
    DynamicBuffer<ShapePtr> m_silhouette_shapes_dr;
    std::unique_ptr<DiscreteDistribution<Float>> m_silhouette_distr = nullptr;

    bool m_shapes_grad_enabled;
    bool m_thread_reordering;
    /// Compact GPU acceleration structures after building. This reduces BLAS
    /// memory at the cost of an extra build-time query and compaction pass.
    bool m_compact_accel;
    /// Has an acceleration structure build already taken place?
    bool m_accel_built = false;
    /// Enable/disable automatic BVH compaction criterion in compact_accel().
    bool m_compact_accel_auto;

    /// Instances in order of appearance in ``m_shapes``.
    /// `PreliminaryIntersection3f.instance_index` references this array biased
    /// by one, since 0 marks non-instanced intersections.
    std::vector<const Shape *> m_instances;

    /// Flattened sequence of instance ``to_world`` matrices (12 floats each)
    DynamicBuffer<Float> m_instance_transforms;

    /// Per-instance animated keyframe data, populated only when at least one
    /// instance has an animated ``to_world``. Concatenated stride-12 chunks
    /// ``[time, S.x,S.y,S.z, Q.x,Q.y,Q.z,Q.w, T.x,T.y,T.z, pad]`` per keyframe,
    /// matching AnimatedTransform's storage layout. The remaining buffers hold,
    /// per instance, the start chunk index, the keyframe count (1 = static),
    /// and the uniform time grid (first keyframe time and spacing).
    DynamicBuffer<Float>  m_instance_kf_data;
    DynamicBuffer<UInt32> m_instance_kf_offset;
    DynamicBuffer<UInt32> m_instance_kf_count;
    DynamicBuffer<Float>  m_instance_kf_tmin;
    DynamicBuffer<Float>  m_instance_kf_tstep;

    /// Number of instances with a static ``to_world``. Zero means the static
    /// matrices in ``m_instance_transforms`` are never selected.
    size_t m_static_instance_count = 0;

    /// Instancing-aware expansion of a preliminary intersection (see
    /// ``compute_surface_interaction()``, which forwards here when the
    /// record may reference instanced geometry)
    SurfaceInteraction3f compute_surface_interaction_instanced(
        const Ray3f &ray, const PreliminaryIntersection3f &pi,
        uint32_t ray_flags, Mask active) const;

    /// Evaluate instance ``i0`` (0-based) ``to_world`` at ``time``. Animated
    /// instances interpolate their keyframes (matching the Embree/OptiX SRT
    /// motion accel); static instances fall back to the differentiable matrix
    /// in ``m_instance_transforms``.
    AffineTransform4f eval_instance_to_world(const UInt32 &i0, const Float &time,
                                             Mask active) const;

    // The Accel class needs to access the scene's protected members.
    friend SceneAccel<Float, Spectrum>;

    MI_DECLARE_TRAVERSE_CB(m_accel, m_emitters, m_emitters_dr, m_shapes,
                           m_shapes_dr, m_shapegroups, m_sensors, m_sensors_dr,
                           m_children, m_integrator, m_environment,
                           m_emitter_pmf, m_emitter_distr, m_silhouette_shapes,
                           m_silhouette_shapes_dr, m_silhouette_distr,
                           m_instance_transforms, m_instance_kf_data,
                           m_instance_kf_offset, m_instance_kf_count,
                           m_instance_kf_tmin, m_instance_kf_tstep)
};

// See interaction.h
template <typename Float, typename Spectrum>
typename SurfaceInteraction<Float, Spectrum>::EmitterPtr
SurfaceInteraction<Float, Spectrum>::emitter(
        const Scene *scene, Mask active,
        const dr::uint32_array_t<Float> &visibility_mask) const {
    if constexpr (!dr::is_jit_v<Float>) {
        DRJIT_MARK_USED(active);
        if (is_valid())
            return shape->emitter();
        const Emitter *env = scene->environment();
        return (env && (visibility_mask & env->visibility_mask()) != 0)
                   ? env : nullptr;
    } else {
        EmitterPtr emitter = shape->emitter(active);
        const Emitter *env = scene ? scene->environment() : nullptr;
        if (env) {
            Mask env_visible =
                active && ((visibility_mask & env->visibility_mask()) != 0u);
            emitter = dr::select(is_valid(), emitter, env & env_visible);
        }
        return emitter;
    }
}

MI_EXTERN_CLASS(Scene)
NAMESPACE_END(mitsuba)
