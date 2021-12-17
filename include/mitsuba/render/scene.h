#pragma once

#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/shapegroup.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Scene : public Object {
public:
    MTS_IMPORT_TYPES(BSDF, Emitter, EmitterPtr, Film, Sampler, Shape, ShapePtr,
                     ShapeGroup, Sensor, Integrator, Medium, MediumPtr)

    /// Instantiate a scene from a \ref Properties object
    Scene(const Properties &props);

    // =============================================================
    //! @{ \name Rendering
    // =============================================================

    /**
     * \brief Convenience function to render the scene and return a bitmap
     *
     * This function renders the scene from the viewpoint of the sensor with
     * index \c sensor_index. All other parameters are optional and control
     * different aspects of the rendering process. In particular:
     *
     * \param seed
     *     This parameter controls the initialization of the random number
     *     generator. It is crucial that you specify different seeds (e.g., an
     *     increasing sequence) if subsequent \c render() calls should produce
     *     statistically independent images.
     *
     * \param spp
     *     Set this parameter to a nonzero value to override the number of
     *     samples per pixel. This value then takes precedence over whatever
     *     was specified in the construction of <tt>sensor->sampler()</tt>.
     *     This parameter may be useful in research applications where an image
     *     must be rendered multiple times using different quality levels.
     */
    ref<Bitmap> render(uint32_t sensor_index = 0, uint32_t seed = 0,
                       uint32_t spp = 0);

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing
    // =============================================================

    /**
     * \brief Intersect a ray against all primitives stored in the scene
     * and return information about the resulting surface interaction
     *
     * \param ray
     *    A 3-dimensional ray data structure with minimum/maximum
     *    extent information, as well as a time value (which matters
     *    when the shapes are in motion)
     *
     * \return
     *    A detailed surface interaction record. Query its \ref
     *    <tt>is_valid()</tt> method to determine whether an
     *    intersection was actually found.
     */
    SurfaceInteraction3f ray_intersect(const Ray3f &ray,
                                       Mask active = true) const;

    SurfaceInteraction3f ray_intersect(const Ray3f &ray,
                                       uint32_t hit_flags,
                                       Mask active = true) const;

    PreliminaryIntersection3f ray_intersect_preliminary(const Ray3f &ray,
                                                        Mask active = true) const;

    PreliminaryIntersection3f ray_intersect_preliminary(const Ray3f &ray,
                                                        uint32_t hit_flags,
                                                        Mask active = true) const;

    /**
     * \brief Ray intersection using brute force search. Used in
     * unit tests to validate the kdtree-based ray tracer.
     *
     * \remark Not implemented by the Embree/OptiX backends
     */
    SurfaceInteraction3f ray_intersect_naive(const Ray3f &ray,
                                             Mask active = true) const;

    /**
     * \brief Intersect a ray against all primitives stored in the scene
     * and \a only determine whether or not there is an intersection.
     *
     * Testing for the mere presence of intersections (as in \ref
     * ray_intersect) is considerably faster than finding an actual
     * intersection, hence this function should be preferred when
     * detailed information is not needed.
     *
     * \param ray
     *    A 3-dimensional ray data structure with minimum/maximum
     *    extent information, as well as a time value (which matterns
     *    when the shapes are in motion)
     *
     * \return \c true if an intersection was found
     */
    Mask ray_test(const Ray3f &ray, Mask active = true) const;
    Mask ray_test(const Ray3f &ray, uint32_t hit_flags,
                  Mask active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Sampling interface
    // =============================================================

    /**
     * \brief Sample one emitter in the scene and rescale the random sample for reuse.
     * If possible, it is sampled proportional to its radiance.
     *
     * \param sample
     *    A uniformly distributed number in [0, 1).
     *
     * \return
     *    The index of the chosen emitter along with the sampling weight (equal
     *    to the inverse PDF), and the rescaled random sample for reusing.
     */
    std::tuple<UInt32, Float, Float>
    sample_emitter(Float index_sample, Mask active = true) const;

    /**
     * \brief Importance sample a ray according to the emission profile
     * defined by the emitters in the scene.
     *
     * This function combines both steps of choosing a ray origin on a light
     * source and an outgoing ray direction.
     * It does not return any auxiliary sampling information and is mainly
     * meant to be used by unidirectional rendering techniques.
     *
     * Note that this function may use a different sampling strategy compared to
     * the sequence of running \ref sample_emitter_position()
     * and \ref Emitter::sample_direction(). The reason for this is that it may
     * be possible to switch to a better technique when sampling both
     * position and direction at the same time.
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
     * \return (ray, importance weight, emitter, radiance)
     *    ray: sampled ray, starting from the surface of an emitter in the scene
     *    importance weight: accounts for the difference between the profile and
     *        the sampling density function actualy used.
     *    emitter: pointer to the selected emitter.
     *    radiance: emitted radiance along the sampled ray.
     */
    std::tuple<Ray3f, Spectrum, const EmitterPtr>
    sample_emitter_ray(Float time, Float sample1, const Point2f &sample2,
                       const Point2f &sample3, Mask active = true) const;

    /**
     * \brief Direct illumination sampling routine
     *
     * Given an arbitrary reference point in the scene, this method samples a
     * direction from the reference point to towards an emitter.
     *
     * Ideally, the implementation should importance sample the product of the
     * emission profile and the geometry term between the reference point and
     * the position on the emitter.
     *
     * \param ref
     *    A reference point somewhere within the scene
     *
     * \param sample
     *    A uniformly distributed 2D vector
     *
     * \param test_visibility
     *    When set to \c true, a shadow ray will be cast to ensure that the
     *    sampled emitter position and the reference point are mutually visible.
     *
     * \return
     *    Radiance received along the sampled ray divided by the sample
     *    probability.
     */
    std::pair<DirectionSample3f, Spectrum>
    sample_emitter_direction(const Interaction3f &ref,
                             const Point2f &sample,
                             bool test_visibility = true,
                             Mask active = true) const;

    /**
     * \brief Evaluate the probability density of the \ref
     * sample_emitter() technique given a sampled emitter index.
     */
    Float pdf_emitter(UInt32 index, Mask active = true) const;

    /**
     * \brief Evaluate the probability density of the  \ref
     * sample_emitter_direct() technique given an filled-in \ref
     * DirectionSample record.
     *
     * \param ref
     *    A reference point somewhere within the scene
     *
     * \param ds
     *    A direction sampling record, which specifies the query location.
     *
     * \return
     *    The solid angle density expressed of the sample
     */
    Float pdf_emitter_direction(const Interaction3f &ref,
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

    /// Return the list of emitters as an Enoki array
    const DynamicBuffer<EmitterPtr> &emitters_ek() const { return m_emitters_ek; }

    /// Return the list of shapes as an Enoki array
    const DynamicBuffer<ShapePtr> &shapes_ek() const { return m_shapes_ek; }

    //! @}
    // =============================================================

    /// Perform a custom traversal over the scene graph
    void traverse(TraversalCallback *callback) override;

    /// Update internal state following a parameter update
    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override;

    /// Return whether any of the shape's parameters require gradient
    bool shapes_grad_enabled() const { return m_shapes_grad_enabled; };

    /// Return a human-readable string representation of the scene contents.
    virtual std::string to_string() const override;

    /// Static initialization of ray-intersection acceleration data structure
    static void static_accel_initialization();

    /// Static shutdown of ray-intersection acceleration data structure
    static void static_accel_shutdown();

    MTS_DECLARE_CLASS()
protected:
    /// Virtual destructor
    virtual ~Scene();

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
    MTS_INLINE PreliminaryIntersection3f ray_intersect_preliminary_cpu(
        const Ray3f &ray, uint32_t hit_flags, Mask active) const;
    MTS_INLINE PreliminaryIntersection3f ray_intersect_preliminary_gpu(
        const Ray3f &ray, uint32_t hit_flags, Mask active) const;

    /// Trace a ray
    MTS_INLINE SurfaceInteraction3f ray_intersect_cpu(const Ray3f &ray, uint32_t hit_flags, Mask active) const;
    MTS_INLINE SurfaceInteraction3f ray_intersect_gpu(const Ray3f &ray, uint32_t hit_flags, Mask active) const;
    MTS_INLINE SurfaceInteraction3f ray_intersect_naive_cpu(const Ray3f &ray, Mask active) const;

    /// Trace a shadow ray
    MTS_INLINE Mask ray_test_cpu(const Ray3f &ray, uint32_t hit_flags, Mask active) const;
    MTS_INLINE Mask ray_test_gpu(const Ray3f &ray, uint32_t hit_flags, Mask active) const;

    using ShapeKDTree = mitsuba::ShapeKDTree<Float, Spectrum>;

protected:
    /// Acceleration data structure (IAS) (type depends on implementation)
    void *m_accel = nullptr;
    /// Handle to the IAS used to ensure its lifetime in jit variants
    UInt64 m_accel_handle;

    ScalarBoundingBox3f m_bbox;

    std::vector<ref<Emitter>> m_emitters;
    DynamicBuffer<EmitterPtr> m_emitters_ek;
    std::vector<ref<Shape>> m_shapes;
    DynamicBuffer<ShapePtr> m_shapes_ek;
    std::vector<ref<ShapeGroup>> m_shapegroups;
    std::vector<ref<Sensor>> m_sensors;
    std::vector<ref<Object>> m_children;
    ref<Integrator> m_integrator;
    ref<Emitter> m_environment;

    bool m_shapes_grad_enabled;
};

/// Dummy function which can be called to ensure that the librender shared library is loaded
extern MTS_EXPORT_RENDER void librender_nop();

// See interaction.h
template <typename Float, typename Spectrum>
typename SurfaceInteraction<Float, Spectrum>::EmitterPtr
SurfaceInteraction<Float, Spectrum>::emitter(const Scene *scene, Mask active) const {
    if constexpr (!ek::is_array_v<ShapePtr>) {
        if (is_valid())
            return shape->emitter(active);
        else
            return scene->environment();
    } else {
        Mask valid = is_valid();
        return ek::select(valid, shape->emitter(active && valid), scene->environment());
    }
}

MTS_EXTERN_CLASS_RENDER(Scene)
NAMESPACE_END(mitsuba)
