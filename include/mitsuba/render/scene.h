#pragma once

#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/shapegroup.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Central scene data structure
 *
 * Mitsuba's scene class encapsulates a tree of mitsuba \ref Object instances
 * including emitters, sensors, shapes, materials, participating media, the
 * integrator (i.e. the method used to render the image) etc.
 *
 * It organizes these objects into groups that can be accessed through getters
 * (see \ref shapes(), \ref emitters(), \ref sensors(), etc.), and it provides
 * three key abstractions implemented on top of these groups, specifically:
 *
 * <ul>
 *    <li>Ray intersection queries and shadow ray tests
 *        (See \ray_intersect_preliminary(), \ref ray_intersect(),
 *         and \ref ray_test()).</li>
 *
 *    <li>Sampling rays approximately proportional to the emission profile of
 *        light sources in the scene (see \ref sample_emitter_ray())</li>
 *
 *    <li>Sampling directions approximately proportional to the
 *        direct radiance from emitters received at a given scene location
 *        (see \ref sample_emitter_direction()).</li>
 * </ul>
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Scene : public Object {
public:
    MI_IMPORT_TYPES(BSDF, Emitter, EmitterPtr, Film, Sampler, Shape, ShapePtr,
                    ShapeGroup, Sensor, Integrator, Medium, MediumPtr, Mesh)

    /// Instantiate a scene from a \ref Properties object
    Scene(const Properties &props);

    // =============================================================
    //! @{ \name Ray tracing
    // =============================================================

    /**
     * \brief Intersect a ray with the shapes comprising the scene and return a
     * detailed data structure describing the intersection, if one is found.
     *
     * In vectorized variants of Mitsuba (<tt>cuda_*</tt> or <tt>llvm_*</tt>),
     * the function processes arrays of rays and returns arrays of surface
     * interactions following the usual conventions.
     *
     * This method is a convenience wrapper of the generalized version of
     * \c ray_intersect() below. It assumes that incoherent rays are being traced,
     * and that the user desires access to all fields of the
     * \ref SurfaceInteraction. In other words, it simply invokes the general
     * \c ray_intersect() overload with <tt>coherent=false</tt> and
     * \c ray_flags equal to \ref RayFlags::All.
     *
     * \param ray
     *    A 3D ray including maximum extent (\ref Ray::maxt) and time (\ref
     *    Ray::time) information, which matters when the shapes are in motion
     *
     * \return
     *    A detailed surface interaction record. Its <tt>is_valid()</tt> method
     *    should be queried to check if an intersection was actually found.
     */
    SurfaceInteraction3f ray_intersect(const Ray3f &ray,
                                       Mask active = true) const {
        return ray_intersect(ray, +RayFlags::All, false, active);
    }

    /**
     * \brief Intersect a ray with the shapes comprising the scene and return a
     * detailed data structure describing the intersection, if one is found
     *
     * In vectorized variants of Mitsuba (<tt>cuda_*</tt> or <tt>llvm_*</tt>),
     * the function processes arrays of rays and returns arrays of surface
     * interactions following the usual conventions.
     *
     * This generalized ray intersection method exposes two additional flags to
     * control the intersection process. Internally, it is split into two
     * steps:
     *
     * <ol>
     *   <li> Finding a \ref PreliminaryInteraction using the ray tracing
     *        backend underlying the current variant (i.e., Mitsuba's builtin
     *        kd-tree, Embree, or OptiX). This is done using the
     *        \ref ray_intersect_preliminary() function that is also available
     *        directly below (and preferable if a full \ref SurfaceInteraction
     *        is not needed.).
     *   </li>
     *
     *   <li> Expanding the \ref PreliminaryInteraction into a full
     *        \ref SurfaceInteraction (this part happens within Mitsuba/Dr.Jit
     *        and tracks derivative information in AD variants of the system).
     *   </li>
     * </ol>
     *
     * The \ref SurfaceInteraction data structure is large, and computing its
     * contents in the second step requires a non-trivial amount of computation
     * and sequence of memory accesses. The \c ray_flags parameter can be used
     * to specify that only a sub-set of the full intersection data structure
     * actually needs to be computed, which can improve performance.
     *
     * In the context of differentiable rendering, the \c ray_flags parameter
     * also influences how derivatives propagate between the input ray, the
     * shape parameters, and the computed intersection (see
     * \ref RayFlags::FollowShape and \ref RayFlags::DetachShape for details on
     * this). The default, \ref RayFlags::All, propagates derivatives through
     * all steps of the intersection computation.
     *
     * The \c coherent flag is a hint that can improve performance in the first
     * step of finding the \ref PreliminaryInteraction if the input set of rays
     * is coherent (e.g., when they are generated by \ref Sensor::sample_ray(),
     * which means that adjacent rays will traverse essentially the same region
     * of space). This flag is currently only used by the combination of
     * <tt>llvm_*</tt> variants and the Embree ray tracing backend.
     *
     * \param ray
     *    A 3D ray including maximum extent (\ref Ray::maxt) and time (\ref
     *    Ray::time) information, which matters when the shapes are in motion
     *
     * \param ray_flags
     *    An integer combining flag bits from \ref RayFlags (merged using
     *    binary or).
     *
     * \param coherent
     *    Setting this flag to \c true can noticeably improve performance when
     *    \c ray contains a coherent set of rays (e.g. primary camera rays),
     *    and when using <tt>llvm_*</tt> variants of the renderer along with
     *    Embree. It has no effect in scalar or CUDA/OptiX variants.
     *
     * \return
     *    A detailed surface interaction record. Its <tt>is_valid()</tt> method
     *    should be queried to check if an intersection was actually found.
     */
    SurfaceInteraction3f ray_intersect(const Ray3f &ray,
                                       uint32_t ray_flags,
                                       Mask coherent,
                                       Mask active = true) const;

    /**
     * \brief Intersect a ray with the shapes comprising the scene and return a
     * boolean specifying whether or not an intersection was found.
     *
     * In vectorized variants of Mitsuba (<tt>cuda_*</tt> or <tt>llvm_*</tt>),
     * the function processes arrays of rays and returns arrays of booleans
     * following the usual conventions.
     *
     * Testing for the mere presence of intersections is considerably faster
     * than finding an actual intersection, hence this function should be
     * preferred over \ref ray_intersect() when geometric information about the
     * first visible intersection is not needed.
     *
     * This method is a convenience wrapper of the generalized version of \c
     * ray_test() below, which assumes that incoherent rays are being traced.
     * In other words, it simply invokes the general \c ray_test() overload
     * with <tt>coherent=false</tt>.
     *
     * \param ray
     *    A 3D ray including maximum extent (\ref Ray::maxt) and time (\ref
     *    Ray::time) information, which matters when the shapes are in motion
     *
     * \return \c true if an intersection was found
     */
    Mask ray_test(const Ray3f &ray, Mask active = true) const {
        return ray_test(ray, false, active);
    }

    /**
     * \brief Intersect a ray with the shapes comprising the scene and return a
     * boolean specifying whether or not an intersection was found.
     *
     * In vectorized variants of Mitsuba (<tt>cuda_*</tt> or <tt>llvm_*</tt>),
     * the function processes arrays of rays and returns arrays of booleans
     * following the usual conventions.
     *
     * Testing for the mere presence of intersections is considerably faster
     * than finding an actual intersection, hence this function should be
     * preferred over \ref ray_intersect() when geometric information about the
     * first visible intersection is not needed.
     *
     * The \c coherent flag is a hint that can improve performance in the first
     * step of finding the \ref PreliminaryInteraction if the input set of rays
     * is coherent, which means that adjacent rays will traverse essentially
     * the same region of space. This flag is currently only used by the
     * combination of <tt>llvm_*</tt> variants and the Embree ray tracing
     * backend.
     *
     * \param ray
     *    A 3D ray including maximum extent (\ref Ray::maxt) and time (\ref
     *    Ray::time) information, which matters when the shapes are in motion
     *
     * \param coherent
     *    Setting this flag to \c true can noticeably improve performance when
     *    \c ray contains a coherent set of rays (e.g. primary camera rays),
     *    and when using <tt>llvm_*</tt> variants of the renderer along with
     *    Embree. It has no effect in scalar or CUDA/OptiX variants.
     *
     * \return \c true if an intersection was found
     */
    Mask ray_test(const Ray3f &ray, Mask coherent, Mask active) const;

    /**
     * \brief Intersect a ray with the shapes comprising the scene and return
     * preliminary information, if one is found
     *
     * This function invokes the ray tracing backend underlying the current
     * variant (i.e., Mitsuba's builtin kd-tree, Embree, or OptiX) and returns
     * preliminary intersection information consisting of
     *
     * <ul>
     *    <li>the ray distance up to the intersection (if one is found).</li>
     *    <li>the intersected shape and primitive index.</li>
     *    <li>local UV coordinates of the intersection within the primitive.</li>
     *    <li>A pointer to the intersected shape or instance.</li>
     * </ul>
     *
     * The information is only preliminary at this point, because it lacks
     * various other information (geometric and shading frame, texture
     * coordinates, curvature, etc.) that is generally needed by shading
     * models. In variants of Mitsuba that perform automatic differentiation,
     * it is important to know that computation done by the ray tracing
     * backend is not reflected in Dr.Jit's computation graph. The \ref
     * ray_intersect() method will re-evaluate certain parts of the computation
     * with derivative tracking to rectify this.
     *
     * In vectorized variants of Mitsuba (<tt>cuda_*</tt> or <tt>llvm_*</tt>),
     * the function processes arrays of rays and returns arrays of preliminary
     * intersection records following the usual conventions.
     *
     * The \c coherent flag is a hint that can improve performance if the input
     * set of rays is coherent (e.g., when they are generated by \ref
     * Sensor::sample_ray(), which means that adjacent rays will traverse
     * essentially the same region of space). This flag is currently only used
     * by the combination of <tt>llvm_*</tt> variants and the Embree ray
     * intersector.
     *
     * \param ray
     *    A 3D ray including maximum extent (\ref Ray::maxt) and time (\ref
     *    Ray::time) information, which matters when the shapes are in motion
     *
     * \param coherent
     *    Setting this flag to \c true can noticeably improve performance when
     *    \c ray contains a coherent set of rays (e.g. primary camera rays),
     *    and when using <tt>llvm_*</tt> variants of the renderer along with
     *    Embree. It has no effect in scalar or CUDA/OptiX variants.
     *
     * \return
     *    A preliminary surface interaction record. Its <tt>is_valid()</tt> method
     *    should be queried to check if an intersection was actually found.
     */
    PreliminaryIntersection3f ray_intersect_preliminary(const Ray3f &ray,
                                                        Mask coherent = false,
                                                        Mask active = true) const;

    /**
     * \brief Ray intersection using a brute force search. Used in
     * unit tests to validate the kdtree-based ray tracer.
     *
     * \remark Not implemented by the Embree/OptiX backends
     */
    SurfaceInteraction3f ray_intersect_naive(const Ray3f &ray,
                                             Mask active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Emitter sampling interface
    // =============================================================

    /**
     * \brief Sample one emitter in the scene and rescale the input sample
     * for reuse.
     *
     * Currently, the sampling scheme implemented by the \ref Scene class is
     * very simplistic (uniform).
     *
     * \param sample
     *    A uniformly distributed number in [0, 1).
     *
     * \return
     *    The index of the chosen emitter along with the sampling weight (equal
     *    to the inverse PDF), and the transformed random sample for reuse.
     */
    std::tuple<UInt32, Float, Float>
    sample_emitter(Float index_sample, Mask active = true) const;

    /**
     * \brief Evaluate the discrete probability of the \ref
     * sample_emitter() technique for the given a emitter index.
     */
    Float pdf_emitter(UInt32 index, Mask active = true) const;

    /**
     * \brief Sample a ray according to the emission profile of scene emitters
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
     * \param time
     *    The scene time associated with the ray to be sampled.
     *
     * \param sample1
     *     A uniformly distributed 1D value that is used to sample the spectral
     *     dimension of the emission profile.
     *
     * \param sample2
     *    A uniformly distributed sample on the domain <tt>[0,1]^2</tt>.
     *
     * \param sample3
     *    A uniformly distributed sample on the domain <tt>[0,1]^2</tt>.
     *
     * \return A tuple <tt>(ray, weight, emitter, radiance)</tt>, where
     *    <ul>
     *       <li>\c ray is the sampled ray (e.g. starting on the surface of an
     *            area emitter)</li>
     *       <li>\c weight returns the emitted radiance divided by the
     *           spatio-directional sampling density</li>
     *       <li>\c emitter is a pointer specifying the sampled emitter</li>
     *    </ul>
     */
    std::tuple<Ray3f, Spectrum, const EmitterPtr>
    sample_emitter_ray(Float time, Float sample1, const Point2f &sample2,
                       const Point2f &sample3, Mask active = true) const;

    /**
     * \brief Direct illumination sampling routine
     *
     * This method implements stochastic connections to emitters, which is
     * variously known as <em>emitter sampling</em>, <em>direct illumination
     * sampling</em>, or <em>next event estimation</em>.
     *
     * The function expects a 3D reference location \c ref as input, which may
     * influence the sampling process. Normally, this would be the location of
     * a surface position being shaded. Ideally, the implementation of this
     * function should then draw samples proportional to the scene's emission
     * profile and the inverse square distance between the reference point and
     * the sampled emitter position. However, approximations are acceptable as
     * long as these are reflected in the returned Monte Carlo sampling weight.
     *
     * \param ref
     *    A 3D reference location within the scene, which may influence the
     *    sampling process.
     *
     * \param sample
     *    A uniformly distributed 2D random variate
     *
     * \param test_visibility
     *    When set to \c true, a shadow ray will be cast to ensure that the
     *    sampled emitter position and the reference point are mutually visible.
     *
     * \return
     *    A tuple <tt>(ds, spec)</tt> where
     *    <ul>
     *      <li>\c ds is a fully populated \ref DirectionSample3f data
     *          structure, which provides further detail about the sampled
     *          emitter position (e.g. its surface normal, solid angle density,
     *          whether Dirac delta distributions were involved, etc.)</li>
     *      <li>
     *      <li>\c spec is a Monte Carlo sampling weight specifying the ratio
     *          of the radiance incident from the emitter and the sample
     *          probability per unit solid angle.</li>
     *    </ul>
     */
    std::pair<DirectionSample3f, Spectrum>
    sample_emitter_direction(const Interaction3f &ref,
                             const Point2f &sample,
                             bool test_visibility = true,
                             Mask active = true) const;

    /**
     * \brief Evaluate the PDF of direct illumination sampling
     *
     * This function evaluates the probability density (per unit solid angle)
     * of the sampling technique implemented by the \ref
     * sample_emitter_direct() function. The returned probability will always
     * be zero when the emission profile contains a Dirac delta term (e.g.
     * point or directional emitters/sensors).
     *
     * \param ref
     *    A 3D reference location within the scene, which may influence the
     *    sampling process.
     *
     * \param ds
     *    A direction sampling record, which specifies the query location.
     *
     * \return
     *    The solid angle density of the sample
     */
    Float pdf_emitter_direction(const Interaction3f &ref,
                                const DirectionSample3f &ds,
                                Mask active = true) const;

    /**
     * \brief Re-evaluate the incident direct radiance of the \ref
     * sample_emitter_direction() method.
     *
     * This function re-evaluates the incident direct radiance and sample
     * probability due to the emitter *so that division by * <tt>ds.pdf</tt>
     * equals the sampling weight returned by \ref sample_emitter_direction().
     * This may appear redundant, and indeed such a function would not find use
     * in "normal" rendering algorithms.
     *
     * However, the ability to re-evaluate the contribution of a direct
     * illumination sample is important for differentiable rendering. For
     * example, we might want to track derivatives in the sampled direction
     * (<tt>ds.d</tt>) without also differentiating the sampling technique.
     * Alternatively (or additionally), it may be necessary to apply a
     * spherical reparameterization to <tt>ds.d</tt>  to handle
     * visibility-induced discontinuities during differentiation. Both steps
     * require re-evaluating the contribution of the emitter while tracking
     * derivative information through the calculation.
     *
     * In contrast to \ref pdf_emitter_direction(), evaluating this function can
     * yield a nonzero result in the case of emission profiles containing a
     * Dirac delta term (e.g. point or directional lights).
     *
     * \param ref
     *    A 3D reference location within the scene, which may influence the
     *    sampling process.
     *
     * \param ds
     *    A direction sampling record, which specifies the query location.
     *
     * \return
     *    The incident radiance and discrete or solid angle density of the
     *    sample.
     */
    Spectrum eval_emitter_direction(const Interaction3f &ref,
                                    const DirectionSample3f &ds,
                                    Mask active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Accessors
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

    /// Return the scene's integrator
    Integrator* integrator() { return m_integrator; }
    /// Return the scene's integrator
    const Integrator* integrator() const { return m_integrator; }

    /// Return the list of emitters as an Dr.Jit array
    const DynamicBuffer<EmitterPtr> &emitters_dr() const { return m_emitters_dr; }

    /// Return the list of shapes as an Dr.Jit array
    const DynamicBuffer<ShapePtr> &shapes_dr() const { return m_shapes_dr; }

    //! @}
    // =============================================================

    /// Traverse the scene graph and invoke the given callback for each object
    void traverse(TraversalCallback *callback) override;

    /// Update internal state following a parameter update
    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override;

    /**
     * \brief Specifies whether any of the scene's shape parameters have
     * tracking enabled
     *
     * Knowing this is important in the context of differentiable rendering:
     * intersections (e.g. provided by OptiX or Embree) must then be
     * re-computed differentiably within Dr.Jit to correctly track gradient
     * information. Furthermore, differentiable geometry introduces bias
     * through visibility-induced discontinuities, and reparameterizations
     * (Loubet et al., SIGGRAPH 2019) are needed to avoid this bias.
     */
    bool shapes_grad_enabled() const { return m_shapes_grad_enabled; };

    /// Return a human-readable string representation of the scene contents.
    virtual std::string to_string() const override;

    /// Static initialization of ray-intersection acceleration data structure
    static void static_accel_initialization();

    /// Static shutdown of ray-intersection acceleration data structure
    static void static_accel_shutdown();

    MI_DECLARE_CLASS()
protected:
    /// Virtual destructor
    virtual ~Scene();

    /// Unmarks all shapes as dirty
    void clear_shapes_dirty();

    /// Create the ray-intersection acceleration data structure
    void accel_init_cpu(const Properties &props);
    void accel_init_gpu(const Properties &props);

    /// Updates the ray-intersection acceleration data structure
    void accel_parameters_changed_cpu();
    void accel_parameters_changed_gpu();

    /// Release the ray-intersection acceleration data structure
    void accel_release_cpu();
    void accel_release_gpu();

    static void static_accel_initialization_cpu();
    static void static_accel_initialization_gpu();
    static void static_accel_shutdown_cpu();
    static void static_accel_shutdown_gpu();

    /// Trace a ray and only return a preliminary intersection data structure
    MI_INLINE PreliminaryIntersection3f ray_intersect_preliminary_cpu(
        const Ray3f &ray, Mask coherent, Mask active) const;
    MI_INLINE PreliminaryIntersection3f ray_intersect_preliminary_gpu(
        const Ray3f &ray, Mask active) const;

    /// Trace a ray
    MI_INLINE SurfaceInteraction3f ray_intersect_cpu(const Ray3f &ray, uint32_t ray_flags, Mask coherent, Mask active) const;
    MI_INLINE SurfaceInteraction3f ray_intersect_gpu(const Ray3f &ray, uint32_t ray_flags, Mask active) const;
    MI_INLINE SurfaceInteraction3f ray_intersect_naive_cpu(const Ray3f &ray, Mask active) const;

    /// Trace a shadow ray
    MI_INLINE Mask ray_test_cpu(const Ray3f &ray, Mask coherent, Mask active) const;
    MI_INLINE Mask ray_test_gpu(const Ray3f &ray, Mask active) const;

    using ShapeKDTree = mitsuba::ShapeKDTree<Float, Spectrum>;

protected:
    /// Acceleration data structure (IAS) (type depends on implementation)
    void *m_accel = nullptr;
    /// Handle to the IAS used to ensure its lifetime in jit variants
    UInt64 m_accel_handle;

    ScalarBoundingBox3f m_bbox;

    std::vector<ref<Emitter>> m_emitters;
    DynamicBuffer<EmitterPtr> m_emitters_dr;
    std::vector<ref<Shape>> m_shapes;
    DynamicBuffer<ShapePtr> m_shapes_dr;
    std::vector<ref<ShapeGroup>> m_shapegroups;
    std::vector<ref<Sensor>> m_sensors;
    std::vector<ref<Object>> m_children;
    ref<Integrator> m_integrator;
    ref<Emitter> m_environment;
    ScalarFloat m_emitter_pmf;

    bool m_shapes_grad_enabled;
};

/// Dummy function which can be called to ensure that the librender shared library is loaded
extern MI_EXPORT_LIB void librender_nop();

// See interaction.h
template <typename Float, typename Spectrum>
typename SurfaceInteraction<Float, Spectrum>::EmitterPtr
SurfaceInteraction<Float, Spectrum>::emitter(const Scene *scene, Mask active) const {
    if constexpr (!dr::is_jit_v<Float>) {
        DRJIT_MARK_USED(active);
        return is_valid() ? shape->emitter() : scene->environment();
    } else {
        EmitterPtr emitter = shape->emitter(active);
        if (scene->environment())
            emitter = dr::select(is_valid(), emitter, scene->environment() & active);
        return emitter;
    }
}

MI_EXTERN_CLASS(Scene)
NAMESPACE_END(mitsuba)
