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
    /// Register this class with Mitsuba's RTTI system
    MTS_DECLARE_CLASS_VARIANT(Scene, Object, "scene")
    MTS_IMPORT_TYPES(BSDF, Emitter, Film, Sampler, Shape, Sensor, Integrator)

    /// Instantiate a scene from a \ref Properties object
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
    SurfaceInteraction3f ray_intersect(const Ray3f &ray,
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
    const BoundingBox3f &bbox() const { return m_bbox; }

    /// Return the list of sensors
    std::vector<ref<Sensor>> &sensors() { return m_sensors; }
    /// Return the list of sensors (const version)
    const std::vector<ref<Sensor>> &sensors() const { return m_sensors; }

    /// Return the list of emitters
    host_vector<ref<Emitter>> &emitters() { return m_emitters; }
    /// Return the list of emitters (const version)
    const host_vector<ref<Emitter>> &emitters() const { return m_emitters; }

    /// Return the environment emitter (if any)
    const Emitter *environment() const { return m_environment.get(); }

    /// Return the list of shapes
    std::vector<ref<Shape>> &shapes() { return m_shapes; }
    /// Return the list of shapes
    const std::vector<ref<Shape>> &shapes() const { return m_shapes; }

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

    /// Create the ray-intersection acceleration data structure
    void accel_init_cpu(const Properties &props);
    void accel_init_gpu(const Properties &props);

    /// Release the ray-intersection acceleration data structure
    void accel_release_cpu();
    void accel_release_gpu();

    /// Trace a ray
    MTS_INLINE SurfaceInteraction3f ray_intersect_cpu(const Ray3f &ray, Mask active) const;
    MTS_INLINE SurfaceInteraction3f ray_intersect_gpu(const Ray3f &ray, Mask active) const;
    MTS_INLINE SurfaceInteraction3f ray_intersect_naive_cpu(const Ray3f &ray, Mask active) const;

    /// Trace a shadow ray
    MTS_INLINE Mask ray_test_cpu(const Ray3f &ray, Mask active) const;
    MTS_INLINE Mask ray_test_gpu(const Ray3f &ray, Mask active) const;

    using ShapeKDTree = mitsuba::ShapeKDTree<Float, Spectrum>;
protected:
    /// Acceleration data structure (type depends on implementation)
    void *m_accel = nullptr;

    ScalarBoundingBox3f m_bbox;

    host_vector<ref<Emitter>> m_emitters;
    std::vector<ref<Shape>> m_shapes;
    std::vector<ref<Sensor>> m_sensors;
    std::vector<ref<Object>> m_children;
    ref<Sampler> m_sampler;
    ref<Integrator> m_integrator;
    ref<Emitter> m_environment;

    /// Sampling distribution for emitters
    DiscreteDistribution<ScalarFloat> m_emitter_distr;
};

/// Dummy function which can be called to ensure that the librender shared library is loaded
extern MTS_EXPORT_RENDER void librender_nop();

NAMESPACE_END(mitsuba)
