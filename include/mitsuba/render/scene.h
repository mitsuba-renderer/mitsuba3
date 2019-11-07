#pragma once

#include <mitsuba/core/ddistr.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Scene : public Object {
public:
    MTS_DECLARE_CLASS_VARIANT(Scene, Object, "scene")
    MTS_IMPORT_TYPES();
    using Sampler              = typename RenderAliases::Sampler;
    using Shape                = typename RenderAliases::Shape;
    using Integrator           = typename RenderAliases::Integrator;
    using BSDF                 = typename RenderAliases::BSDF;
    using Sensor               = typename RenderAliases::Sensor;
    using Emitter              = typename RenderAliases::Emitter;
    // using Medium               = typename RenderAliases::Medium;
    using Film                 = typename RenderAliases::Film;
    // using BSDFPtr              = typename RenderAliases::BSDFPtr;
    // using MediumPtr            = typename RenderAliases::MediumPtr;
    // using ShapePtr             = typename RenderAliases::ShapePtr;
    // using EmitterPtr           = typename RenderAliases::EmitterPtr;

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
    SurfaceInteraction3f ray_intersect(const Ray3f &ray, Mask active = true) const;

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
    bool ray_test(const Ray3f &ray, Mask active = true) const;

#if !defined(MTS_USE_EMBREE)
    /**
     * \brief Ray intersection using brute force search. Used in
     * unit tests to validate the kdtree-based ray tracer.
     */
    SurfaceInteraction3f ray_intersect_naive(const Ray3f &ray, Mask active = true) const;
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
    std::pair<DirectionSample3f, Spectrum>
    sample_emitter_direction(const Interaction3f &ref,
                             const Point2f &sample,
                             bool test_visibility = true,
                             Mask active = true) const;

    /**
     * \brief Modified version of \ref sample_emitter_direction_pol() that also
     * samples accross index-matched interfaces.
     */
    std::pair<DirectionSample3f, Spectrum>
    sample_emitter_direction_attenuated(const Interaction3f &ref,
                                        Sampler *sampler,
                                        bool test_visibility = true,
                                        Mask active = true) const;

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

    /// Sampling distribution for emitters
    DiscreteDistribution<Float> m_emitter_distr;

#if defined(MTS_USE_EMBREE)
    RTCScene m_embree_scene;
#endif

#if defined(MTS_USE_OPTIX)
    struct OptixState;
    OptixState *m_optix_state = nullptr;
#endif
};

/// Dummy function which can be called to ensure that the librender shared library is loaded
extern MTS_EXPORT_RENDER void librender_nop();

NAMESPACE_END(mitsuba)
