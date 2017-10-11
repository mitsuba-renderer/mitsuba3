#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/common.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Emitter : public Endpoint {
public:
    /**
     * \brief This list of flags is used to additionally characterize
     * and classify the response functions of different types of sensors
     *
     * \sa Endpoint::EFlags
     */
    enum EEmitterFlags {
        /// Is this an environment emitter, such as a HDRI map?
        EEnvironmentEmitter = 0x010
    };

    // =================================================================
    //! @{ \name Additional emitter-related sampling and query functions
    // =================================================================

    /**
     * \brief Return the radiant emittance for the given surface intersection
     *
     * This function is used when an area light source has been hit by a
     * ray in a path tracing-style integrator, and it subsequently needs to
     * be queried for the emitted radiance along the negative ray direction.
     *
     * It efficiently computes the product of \ref eval_position()
     * and \ref eval_direction() \a divided by the absolute cosine of the
     * angle between \c d and \c its.sh_frame.n.
     *
     * This function is provided here as a fast convenience function for
     * unidirectional rendering techniques. The default implementation
     * throws an exception, which states that the method is not implemented.
     *
     * \param its
     *    An intersect record that specfies the query position
     * \param d
     *    A unit vector, which specifies the query direction
     * \return
     *    The radiant emittance
     */
    virtual Spectrumf eval(const SurfaceInteraction3f &its,
                           const Vector3f &d) const;
    Spectrumf eval(const SurfaceInteraction3f &its, const Vector3f &d,
                   bool /*unused*/) const {
        return eval(its, d);
    }
    virtual SpectrumfP eval(const SurfaceInteraction3fP &its,
                            const Vector3fP &d,
                            const mask_t<FloatP> &active = true) const;

    /**
     * \brief Importance sample a ray according to the emission profile
     *
     * This function combines both steps of choosing a ray origin and
     * direction value. It does not return any auxiliary sampling
     * information and is mainly meant to be used by unidirectional
     * rendering techniques.
     *
     *
     * Note that this function potentially uses a different sampling
     * strategy compared to the sequence of running \ref sample_position()
     * and \ref sample_direction(). The reason for this is that it may
     * be possible to switch to a better technique when sampling both
     * position and direction at the same time.
     *
     * \param position_sample
     *    Denotes the sample that is used to choose the spatial component
     *
     * \param direction_sample
     *    Denotes the sample that is used to choose the directional component
     *
     * \param time_sample
     *    Scene time value to be associated with the sample
     *
     * \return (ray, weight).
     *    ray: A ray data structure to be populated with a position
     *    and direction value.
     *    weight: An importance weight associated with the sampled ray.
     *    This accounts for the difference between the emission profile
     *    and the sampling density function.
     */
    virtual std::pair<Ray3f, Spectrumf> sample_ray(
            const Point2f &position_sample, const Point2f &direction_sample,
            Float time_sample) const;
    std::pair<Ray3f, Spectrumf> sample_ray(
            const Point2f &position_sample, const Point2f &direction_sample,
            Float time_sample, bool /*unused*/) const {
        return sample_ray(position_sample, direction_sample, time_sample);
    }
    virtual std::pair<Ray3fP, SpectrumfP> sample_ray(
            const Point2fP &position_sample, const Point2fP &direction_sample,
            FloatP time_sample, const mask_t<FloatP> &active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Miscellaneous
    // =============================================================

    /**
     * \brief Return a bitmap representation of the emitter
     *
     * Some types of light sources (projection lights, environment maps)
     * are closely tied to an underlying bitmap data structure. This function
     * can be used to return this information for various purposes.
     *
     * When the class implementing this interface is a bitmap-backed texture,
     * this function directly returns the underlying bitmap. When it is procedural,
     * a bitmap version must first be generated. In this case, the parameter
     * \ref size_hint is used to control the target size. The default
     * value <tt>-1, -1</tt> allows the implementation to choose a suitable
     * size by itself.
     *
     * \remark The default implementation throws an exception
     */
    virtual ref<Bitmap> bitmap(const Vector2i &size_hint = Vector2i(-1, -1)) const;

    /// Serialize this emitter to a binary data stream
    // virtual void serialize(Stream *stream, InstanceManager *manager) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Functionality related to environment emitters
    // =============================================================

    /// Is this an environment emitter? (e.g. an HDRI environment map?)
    bool is_environment_emitter() const {
        return m_type & EEnvironmentEmitter;
    }

    /**
     * \brief Return the radiant emittance from an environment emitter
     *
     * This is function is called by unidirectional rendering techniques
     * (e.g. a path tracer) when no scene object has been intersected, and
     * the scene has been determined to contain an environment emitter.
     *
     * The default implementation throws an exception.
     *
     * \param ray
     *    Specifies the ray direction that should be queried
     */
    virtual Spectrumf eval_environment(const RayDifferential3f &ray) const;
    Spectrumf eval_environment(const RayDifferential3f &ray,
                               bool /*unused*/) const {
        return eval_environment(ray);
    }
    virtual SpectrumfP eval_environment(
            const RayDifferential3fP &ray,
            const mask_t<FloatP> &active = true) const;

    /**
     * \brief Fill out a data record that can be used to query the direct
     * illumination sampling density of an environment emitter.
     *
     * This is function is mainly called by unidirectional rendering
     * techniques (e.g. a path tracer) when no scene object has been
     * intersected, and the (hypothetical) sampling density of the
     * environment emitter needs to be known by a multiple importance
     * sampling technique.
     *
     * The default implementation throws an exception.
     *
     * \param d_rec
     *    The direct illumination sampling record to be filled
     *
     * \param ray
     *    Specifies the ray direction that should be queried
     *
     * \return \c true upon success
     */
    virtual bool fill_direct_sample(
            DirectSample3f &d_rec, const Ray3f &ray) const;
    bool fill_direct_sample(DirectSample3f &d_rec, const Ray3f &ray,
                            bool /*unused*/) const {
        return fill_direct_sample(d_rec, ray);
    }
    virtual bool fill_direct_sample(
            DirectSample3fP &d_rec, const Ray3fP &ray,
            const mask_t<FloatP> &active = true) const;

    //! @}
    // =============================================================

    MTS_DECLARE_CLASS()

protected:
    Emitter(const Properties &props);

    /// Unserialize an Emitter instance from a binary data stream
    // Emitter(Stream *stream, InstanceManager *manager);

    virtual ~Emitter();
};

NAMESPACE_END(mitsuba)
