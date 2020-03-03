#pragma once

#include <mitsuba/core/profiler.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Endpoint: an abstract interface to light sources and sensors
 *
 * This class implements an abstract interface to all sensors and light sources
 * emitting radiance and importance, respectively. Subclasses implement
 * functions to evaluate and sample the profile, and to compute probability
 * densities associated with the provided sampling techniques.
 *
 * The name \a endpoint refers to the property that while a light path may
 * involve any number of scattering events, it always starts and ends with
 * emission and a measurement, respectively.
 *
 * In addition to \ref Endpoint::sample_ray, which generates a sample from the
 * profile, subclasses also provide a specialized direction sampling method.
 * This is a generalization of direct illumination techniques to both emitters
 * \a and sensors. A direction sampling method is given an arbitrary reference
 * position in the scene and samples a direction from the reference point
 * towards the endpoint (ideally proportional to the emission/sensitivity
 * profile). This reduces the sampling domain from 4D to 2D, which often
 * enables the construction of smarter specialized sampling techniques.
 *
 * When rendering scenes involving participating media, it is important to know
 * what medium surrounds the sensors and light sources. For this reason, every
 * endpoint instance keeps a reference to a medium (which may be set to \c
 * nullptr when it is surrounded by vacuum).
 */
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Endpoint : public Object {
public:
    MTS_IMPORT_TYPES(Medium, Scene, Shape)

    // =============================================================
    //! @{ \name Sampling interface
    // =============================================================

    /**
     * \brief Importance sample a ray proportional to the endpoint's
     * sensitivity/emission profile.
     *
     * The endpoint profile is a six-dimensional quantity that depends on time,
     * wavelength, surface position, and direction. This function takes a given
     * time value and five uniformly distributed samples on the interval [0, 1]
     * and warps them so that the returned ray follows the profile. Any
     * discrepancies between ideal and actual sampled profile are absorbed into
     * a spectral importance weight that is returned along with the ray.
     *
     * \param time
     *    The scene time associated with the ray to be sampled
     *
     * \param sample1
     *     A uniformly distributed 1D value that is used to sample the spectral
     *     dimension of the emission profile.
     *
     * \param sample2
     *    A uniformly distributed sample on the domain <tt>[0,1]^2</tt>. For
     *    sensor endpoints, this argument corresponds to the sample position in
     *    fractional pixel coordinates relative to the crop window of the
     *    underlying film.
     *    This argument is ignored if <tt>needs_sample_2() == false</tt>.
     *
     * \param sample3
     *    A uniformly distributed sample on the domain <tt>[0,1]^2</tt>. For
     *    sensor endpoints, this argument determines the position on the
     *    aperture of the sensor.
     *    This argument is ignored if <tt>needs_sample_3() == false</tt>.
     *
     * \return
     *    The sampled ray and (potentially spectrally varying) importance
     *    weights. The latter account for the difference between the profile
     *    and the actual used sampling density function.
     */
    virtual std::pair<Ray3f, Spectrum>
    sample_ray(Float time, Float sample1, const Point2f &sample2,
               const Point2f &sample3, Mask active = true) const;

    /**
     * \brief Given a reference point in the scene, sample a direction from the
     * reference point towards the endpoint (ideally proportional to the
     * emission/sensitivity profile)
     *
     * This operation is a generalization of direct illumination techniques to
     * both emitters \a and sensors. A direction sampling method is given an
     * arbitrary reference position in the scene and samples a direction from
     * the reference point towards the endpoint (ideally proportional to the
     * emission/sensitivity profile). This reduces the sampling domain from 4D
     * to 2D, which often enables the construction of smarter specialized
     * sampling techniques.
     *
     * Ideally, the implementation should importance sample the product of
     * the emission profile and the geometry term between the reference point
     * and the position on the endpoint.
     *
     * The default implementation throws an exception.
     *
     * \param ref
     *    A reference position somewhere within the scene.
     *
     * \param sample
     *     A uniformly distributed 2D point on the domain <tt>[0,1]^2</tt>
     *
     * \return
     *     A \ref DirectionSample instance describing the generated sample
     *     along with a spectral importance weight.
     */
    virtual std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &ref,
                     const Point2f &sample,
                     Mask active = true) const;

    //! @}
    // =============================================================

    /**
     * \brief Evaluate the probability density of the \a direct sampling
     * method implemented by the \ref sample_direction() method.
     *
     * \param ds
     *    A direct sampling record, which specifies the query
     *    location.
     */
    virtual Float pdf_direction(const Interaction3f &ref,
                                const DirectionSample3f &ds,
                                Mask active = true) const;

    //! @}
    // =============================================================


    // =============================================================
    //! @{ \name Other query functions
    // =============================================================

    /**
     * \brief Given a ray-surface intersection, return the emitted
     * radiance or importance traveling along the reverse direction
     *
     * This function is e.g. used when an area light source has been hit by a
     * ray in a path tracing-style integrator, and it subsequently needs to be
     * queried for the emitted radiance along the negative ray direction. The
     * default implementation throws an exception, which states that the method
     * is not implemented.
     *
     * \param si
     *    An intersect record that specfies both the query position
     *    and direction (using the <tt>si.wi</tt> field)
     * \return
     *    The emitted radiance or importance
     */
    virtual Spectrum eval(const SurfaceInteraction3f &si, Mask active = true) const;


    /// Return the local space to world space transformation
    const AnimatedTransform *world_transform() const {
        return m_world_transform.get();
    }

    /**
     * \brief Does the method \ref sample_ray() require a uniformly distributed
     * 2D sample for the \c sample2 parameter?
     */
    bool needs_sample_2() const { return m_needs_sample_2; }

    /**
     * \brief Does the method \ref sample_ray() require a uniformly distributed
     * 2D sample for the \c sample3 parameter?
     */
    bool needs_sample_3() const { return m_needs_sample_3; }


    //! @}
    // =============================================================


    // =============================================================
    //! @{ \name Miscellaneous
    // =============================================================

    /// Return the shape, to which the emitter is currently attached
    Shape *shape() { return m_shape; }

    /// Return the shape, to which the emitter is currently attached (const version)
    const Shape *shape() const { return m_shape; }

    /// Return a pointer to the medium that surrounds the emitter
    Medium *medium() { return m_medium; }

    /// Return a pointer to the medium that surrounds the emitter (const version)
    const Medium *medium() const { return m_medium.get(); }

    /**
     * \brief Return an axis-aligned box bounding the spatial
     * extents of the emitter
     */
    virtual ScalarBoundingBox3f bbox() const = 0;

    /// Set the shape associated with this endpoint.
    virtual void set_shape(Shape *shape);

    /// Set the medium that surrounds the emitter.
    virtual void set_medium(Medium *medium);

    /**
     * \brief Inform the emitter about the properties of the scene
     *
     * Various emitters that surround the scene (e.g. environment emitters)
     * must be informed about the scene dimensions to operate correctly.
     * This function is invoked by the \ref Scene constructor.
     */
    virtual void set_scene(const Scene *scene);

    /// Return a string identifier
    std::string id() const override;

    //! @}
    // =============================================================

    ENOKI_PINNED_OPERATOR_NEW(Float)

    MTS_DECLARE_CLASS()
protected:
    Endpoint(const Properties &props);

    virtual ~Endpoint();

protected:
    ref<const AnimatedTransform> m_world_transform;
    ref<Medium> m_medium;
    Shape *m_shape = nullptr;
    bool m_needs_sample_2 = true;
    bool m_needs_sample_3 = true;
    std::string m_id;
};

MTS_EXTERN_CLASS_RENDER(Endpoint)
NAMESPACE_END(mitsuba)
