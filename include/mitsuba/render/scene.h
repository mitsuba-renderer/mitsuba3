#pragma once

#include <mitsuba/render/fwd.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/core/ddistr.h>

#if defined(MTS_USE_EMBREE)
    #include <embree3/rtcore.h>
#endif

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

    /// Vectorized version of \ref ray_intersect
    SurfaceInteraction3fP ray_intersect(const Ray3fP &ray, MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref ray_intersect()
    SurfaceInteraction3f ray_intersect(const Ray3f &ray, bool /* unused */) const {
        return ray_intersect(ray);
    }

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

    /// Vectorized version of \ref ray_test
    MaskP ray_test(const Ray3fP &ray, MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref ray_test()
    bool ray_test(const Ray3f &ray, bool /* unused */) const {
        return ray_test(ray);
    }

    /**
     * \brief Ray intersection using the naive (brute force) method. Exposed
     * for testing purposes.
     */
    SurfaceInteraction3f ray_intersect_naive(const Ray3f &ray) const;

    /// Vectorized version of \ref ray_intersect_naive
    SurfaceInteraction3fP ray_intersect_naive(const Ray3fP &ray, MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref ray_intersect_naive()
    SurfaceInteraction3f ray_intersect_naive(const Ray3f &ray, bool /* unused */) const {
        return ray_intersect_naive(ray);
    }

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

    /// Vectorized variant of \ref sample_emitter_direction
    std::pair<DirectionSample3fP, SpectrumfP>
    sample_emitter_direction(const Interaction3fP &ref,
                             const Point2fP &sample,
                             bool test_visibility = true,
                             MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref sample_emitter_direction()
    std::pair<DirectionSample3f, Spectrumf>
    sample_emitter_direction(const Interaction3f &ref,
                             const Point2f &sample,
                             bool test_visibility,
                             bool /* active */) const {
        return sample_emitter_direction(ref, sample, test_visibility);
    }

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

    /// Vectorized version of \ref pdf_emitter_direction
    FloatP pdf_emitter_direction(const Interaction3fP &ref,
                                 const DirectionSample3fP &ds,
                                 MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref pdf_emitter_direction()
    Float pdf_emitter_direction(const Interaction3f &ref,
                                const DirectionSample3f &ds,
                                bool /* unused */) const {
        return pdf_emitter_direction(ref, ds);
    }

    /// Return the environment emitter (if any)
    const Emitter *environment() const { return m_environment.get(); }

    //! @}
    // =============================================================


    // =============================================================
    //! @{ \name Accessors
    // =============================================================

    /// Return the scene's KD-tree
    const ShapeKDTree *kdtree() const { return m_kdtree; }
    /// Return the scene's KD-tree
    ShapeKDTree *kdtree() { return m_kdtree; }

    /// Return the current sensor
    Sensor* sensor() { return m_sensors.front(); }
    /// Return the current sensor
    const Sensor* sensor() const { return m_sensors.front(); }

    /// Return the list of emitters
    std::vector<ref<Emitter>> &emitters() { return m_emitters; }
    /// Return the list of emitters
    const std::vector<ref<Emitter>> &emitters() const { return m_emitters; }

    /// Return the list of sensors
    std::vector<ref<Sensor>> &sensors() { return m_sensors; }
    /// Return the list of sensors
    const std::vector<ref<Sensor>> &sensors() const { return m_sensors; }

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

    //! @}
    // =============================================================

    /// Return a human-readable string representation of the scene contents.
    virtual std::string to_string() const override;

    MTS_DECLARE_CLASS()
protected:
    virtual ~Scene();

    ref<ShapeKDTree> m_kdtree;
    std::vector<ref<Sensor>> m_sensors;
    std::vector<ref<Emitter>> m_emitters;
    ref<Sampler> m_sampler;
    ref<Integrator> m_integrator;
    ref<Emitter> m_environment;
    /// Whether monochrome mode is enabled.
    bool m_monochrome = false;

    /// Precomputed distribution of emitters' intensity.
    DiscreteDistribution m_emitter_distr;

#if defined(MTS_USE_EMBREE)
    RTCScene m_embree_scene;
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
