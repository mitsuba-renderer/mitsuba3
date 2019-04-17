#pragma once

#include <mitsuba/render/fwd.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/core/ddistr.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Scene : public Object {
public:
    Scene(const Properties &props);

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
    SurfaceInteraction3f ray_intersect(const Ray3f &ray) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref ray_intersect()
    SurfaceInteraction3f ray_intersect(const Ray3f &ray, bool /* unused */) const {
        return ray_intersect(ray);
    }

    /// Vectorized version of \ref ray_intersect
    SurfaceInteraction3fP ray_intersect(const Ray3fP &ray, MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref ray_intersect
    SurfaceInteraction3fD ray_intersect(const Ray3fD &ray, MaskD active = true) const;
#endif

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
    bool ray_test(const Ray3f &ray) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref ray_test()
    bool ray_test(const Ray3f &ray, bool /* unused */) const {
        return ray_test(ray);
    }

    /// Vectorized version of \ref ray_test
    MaskP ray_test(const Ray3fP &ray, MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref ray_test
    MaskD ray_test(const Ray3fD &ray, MaskD active = true) const;
#endif

#if !defined(MTS_USE_EMBREE)
    /**
     * \brief Ray intersection using brute force search. Used in
     * unit tests to validate the kdtree-based ray tracer.
     */
    SurfaceInteraction3f ray_intersect_naive(const Ray3f &ray) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref ray_intersect_naive()
    SurfaceInteraction3f ray_intersect_naive(const Ray3f &ray, bool /* unused */) const {
        return ray_intersect_naive(ray);
    }

    /// Vectorized version of \ref ray_intersect_naive
    SurfaceInteraction3fP ray_intersect_naive(const Ray3fP &ray, MaskP active = true) const;
#endif

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Sampling interface
    // =============================================================

    /**
     * \brief Direct illumination sampling routine
     *
     * Given an arbitrary reference point in the scene, this method samples a
     * direction from the reference point to towards an emitter.
     *
     * Ideally, the implementation should importance sample the product of
     * the emission profile and the geometry term between the reference point
     * and the position on the emitter.
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
    std::pair<DirectionSample3f, Spectrumf>
    sample_emitter_direction(const Interaction3f &ref,
                             const Point2f &sample,
                             bool test_visibility = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref sample_emitter_direction()
    std::pair<DirectionSample3f, Spectrumf>
    sample_emitter_direction(const Interaction3f &ref,
                             const Point2f &sample,
                             bool test_visibility,
                             bool /* active */) const {
        return sample_emitter_direction(ref, sample, test_visibility);
    }

    /// Vectorized variant of \ref sample_emitter_direction
    std::pair<DirectionSample3fP, SpectrumfP>
    sample_emitter_direction(const Interaction3fP &ref,
                             const Point2fP &sample,
                             bool test_visibility = true,
                             MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable variant of \ref sample_emitter_direction
    std::pair<DirectionSample3fD, SpectrumfD>
    sample_emitter_direction(const Interaction3fD &ref,
                             const Point2fD &sample,
                             bool test_visibility = true,
                             MaskD active = true) const;
#endif

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
                                const DirectionSample3f &ds) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref pdf_emitter_direction()
    Float pdf_emitter_direction(const Interaction3f &ref,
                                const DirectionSample3f &ds,
                                bool /* unused */) const {
        return pdf_emitter_direction(ref, ds);
    }

    /// Vectorized version of \ref pdf_emitter_direction
    FloatP pdf_emitter_direction(const Interaction3fP &ref,
                                 const DirectionSample3fP &ds,
                                 MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref pdf_emitter_direction
    FloatD pdf_emitter_direction(const Interaction3fD &ref,
                                 const DirectionSample3fD &ds,
                                 MaskD active = true) const;
#endif

    //! @}
    // =============================================================


    // =============================================================
    //! @{ \name Accessors
    // =============================================================

    const BoundingBox3f &bbox() const { return m_bbox; }

#if !defined(MTS_USE_EMBREE)
    /// Return the scene's KD-tree
    const ShapeKDTree *kdtree() const { return m_kdtree; }
    /// Return the scene's KD-tree
    ShapeKDTree *kdtree() { return m_kdtree; }
#endif

    /// Return the current sensor
    Sensor* sensor() { return m_sensors[m_current_sensor]; }
    /// Return the current sensor
    const Sensor* sensor() const { return m_sensors[m_current_sensor]; }
    /// Return the list of sensors
    std::vector<ref<Sensor>> &sensors() { return m_sensors; }
    /// Return the list of sensors
    const std::vector<ref<Sensor>> &sensors() const { return m_sensors; }
    /// Sets the current sensor from the given index.
    void set_current_sensor(size_t index) {
        if (index >= m_sensors.size())
            Throw("Invalid sensor index %d, expected index in [0, %d).",
                  index, m_sensors.size());
        m_current_sensor = index;
        m_sampler = m_sensors[m_current_sensor]->sampler();
    }

    /// Return the list of emitters
    auto &emitters() { return m_emitters; }
    /// Return the list of emitters
    const auto &emitters() const { return m_emitters; }

    /// Return the environment emitter (if any)
    const Emitter *environment() const { return m_environment.get(); }

    /// Return the list of shapes
    auto &shapes() { return m_shapes; }
    /// Return the list of shapes
    const auto &shapes() const { return m_shapes; }

    /// Return the current sensor's film
    Film *film() { return m_sensors.front()->film(); }
    /// Return the current sensor's film
    const Film *film() const { return m_sensors.front()->film(); }

    /// Return the scene's sampler
    Sampler* sampler() { return m_sampler; }
    /// Return the scene's sampler
    const Sampler* sampler() const { return m_sampler; }

    /// Return the scene's integrator
    Integrator* integrator() { return m_integrator; }
    /// Return the scene's integrator
    const Integrator* integrator() const { return m_integrator; }

    // Return a list of objects that are referenced or owned by the scene
    std::vector<ref<Object>> children() override;

    //! @}
    // =============================================================

    /// Return a human-readable string representation of the scene contents.
    virtual std::string to_string() const override;

    MTS_DECLARE_CLASS()
protected:
    /// Virtual destructor
    virtual ~Scene();

#if defined(MTS_USE_EMBREE)
    void embree_init();
    void embree_release();
    void embree_register(Shape *shape);
    void embree_build();
#endif

#if defined(MTS_USE_OPTIX)
    void optix_init(uint32_t n_shapes);
    void optix_release();
    void optix_register(Shape *shape);
    void optix_build();
#endif

protected:
    BoundingBox3f m_bbox;
#if !defined(MTS_USE_EMBREE)
    ref<ShapeKDTree> m_kdtree;
#endif

#if defined(MTS_ENABLE_AUTODIFF)
    std::vector<ref<Emitter>, enoki::cuda_host_allocator<ref<Emitter>>> m_emitters;
#else
    std::vector<ref<Emitter>> m_emitters;
#endif

    std::vector<ref<Shape>> m_shapes;
    size_t m_current_sensor;
    std::vector<ref<Sensor>> m_sensors;
    std::vector<ref<Object>> m_children;
    ref<Sampler> m_sampler;
    ref<Integrator> m_integrator;
    ref<Emitter> m_environment;
    /// Whether monochrome mode is enabled.
    bool m_monochrome = false;

    /// Sampling distribution for emitters
    DiscreteDistribution m_emitter_distr;

#if defined(MTS_USE_EMBREE)
    RTCScene m_embree_scene;
#endif

#if defined(MTS_USE_OPTIX)
    struct OptixState;
    OptixState *m_optix_state = nullptr;
#endif

private:
    template <typename Interaction,
              typename Value = typename Interaction::Value,
              typename Spectrum = mitsuba::Spectrum<Value>,
              typename Point3 = Point<Value, 3>,
              typename Point2 = Point<Value, 2>,
              typename DirectionSample = mitsuba::DirectionSample<Point3>,
              typename Mask = mask_t<Value>>
    std::pair<DirectionSample, Spectrum>
    sample_emitter_direction_impl(const Interaction &it,
                                  Point2 sample,
                                  bool test_visibility,
                                  Mask active) const;

    template <typename Interaction,
              typename DirectionSample,
              typename Value = typename Interaction::Value,
              typename Mask  = mask_t<Value>>
    Value pdf_emitter_direction_impl(const Interaction &it,
                                     const DirectionSample &ds,
                                     Mask active) const;
};

NAMESPACE_END(mitsuba)
