#pragma once

#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Sensor : public Endpoint {
public:

    /**
     * \brief This list of flags is used to additionally characterize
     * and classify the response functions of different types of sensors
     *
     * \sa Endpoint::EFlags
     */
    enum ESensorFlags {
        /// Sensor response contains a Dirac delta term with respect to time
        EDeltaTime             = 0x010,

        /// Does the \ref sample_ray() function need an aperture sample?
        ENeedsApertureSample   = 0x020,

        /// Is the sensor a projective camera?
        EProjectiveCamera      = 0x100,

        /// Is the sensor a perspective camera?
        EPerspectiveCamera     = 0x200,

        /// Is the sensor an orthographic camera?
        EOrthographicCamera    = 0x400,

        /// Does the sample given to \ref sample_position() determine the pixel coordinates?
        EPositionSampleMapsToPixels  = 0x1000,

        /// Does the sample given to \ref sample_direction() determine the pixel coordinates?
        EDirectionSampleMapsToPixels = 0x2000
    };


    // =============================================================
    //! @{ \name Sensor-specific sampling functions
    // =============================================================

    /**
     * \brief Importance sample a ray according to the sensor response
     *
     * This function combines all three of the steps of sampling a time, ray
     * position, and direction value. It does not return any auxiliary sampling
     * information and is mainly meant to be used by unidirectional rendering
     * techniques.
     *
     * Note that this function potentially uses a different sampling strategy
     * compared to the sequence of running \ref sample_area() and \ref
     * sample_direction(). The reason for this is that it may be possible to
     * switch to a better technique when sampling both position and direction
     * at the same time.
     *
     * \param position_sample
     *    Denotes the desired sample position in fractional pixel coordinates
     *    relative to the crop window of the underlying film.
     *
     * \param aperture_sample
     *    A uniformly distributed 2D vector that is used to sample a position
     *    on the aperture of the sensor if necessary. (Any value is valid when
     *    \ref needs_aperture_sample() == \c false)
     *
     * \param time_sample
     *    A uniformly distributed 1D vector that is used to sample the temporal
     *    component of the emission profile. (Or any value when \ref
     *    needs_time_sample() == \c false)
     *
     * \return
     *    The sampled ray and an associated importance weight associated. This
     *    accounts for the difference between the sensor response and the
     *    sampling density function.
     */
    virtual std::pair<Ray3f, Spectrumf> sample_ray(
            const Point2f &position_sample, const Point2f &aperture_sample,
            Float time_sample) const = 0;
    std::pair<Ray3f, Spectrumf> sample_ray(
            const Point2f &position_sample, const Point2f &aperture_sample,
            Float time_sample, bool /*active*/) const {
        return sample_ray(position_sample, aperture_sample, time_sample);
    }

    /// Vectorized version of \ref sample_ray
    virtual std::pair<Ray3fP, SpectrumfP> sample_ray(
            const Point2fP &position_sample, const Point2fP &aperture_sample,
            FloatP time_sample, const mask_t<FloatP> &active = true) const = 0;

    /**
     * \brief Importance sample a ray differential according to the
     * sensor response
     *
     * This function combines all three of the steps of sampling a time,
     * ray position, and direction value. It does not return any auxiliary
     * sampling information and is mainly meant to be used by unidirectional
     * rendering techniques.
     *
     * Note that this function potentially uses a different sampling
     * strategy compared to the sequence of running \ref sample_area()
     * and \ref sample_direction(). The reason for this is that it may
     * be possible to switch to a better technique when sampling both
     * position and direction at the same time.
     *
     * The default implementation computes differentials using several
     * internal calls to \ref sample_ray(). Subclasses of the \ref Sensor
     * interface may optionally provide a more efficient approach.
     *
     * \param ray
     *    A ray data structure to be populated with a position
     *    and direction value.
     *
     * \param sample_position
     *    Denotes the desired sample position in fractional pixel
     *    coordinates relative to the crop window of the underlying
     *    film.
     *
     * \param aperture_sample
     *    A uniformly distributed 2D vector that is used to sample
     *    a position on the aperture of the sensor if necessary.
     *    (Any value is valid when \ref needs_aperture_sample() == \c false)

     * \param time_sample
     *    A uniformly distributed 1D vector that is used to sample
     *    the temporal component of the emission profile.
     *    (Or any value when \ref needs_time_sample() == \c false)
     *
     * \return
     *    An importance weight associated with the sampled ray.
     *    This accounts for the difference between the sensor response
     *    and the sampling density function.
     *
     * \remark
     *    In the Python API, the signature of this function is
     *    <tt>spectrumf, ray = sensor.sample_ray_rifferential(sample_position, aperture_sample)</tt>
     */
    virtual std::pair<RayDifferential3f, Spectrumf> sample_ray_differential(
            const Point2f &sample_position, const Point2f &aperture_sample,
            Float time_sample) const;
    std::pair<RayDifferential3f, Spectrumf> sample_ray_differential(
            const Point2f &sample_position, const Point2f &aperture_sample,
            Float time_sample, bool /*active*/) const {
        return sample_ray_differential(sample_position, aperture_sample,
                                       time_sample);
    }

    /// Vectorized version of \ref sample_ray_differential
    virtual std::pair<RayDifferential3fP, SpectrumfP> sample_ray_differential(
        const Point2fP &sample_position, const Point2fP &aperture_sample,
        FloatP time_sample, const mask_t<FloatP> &active = true) const;


    /// Importance sample the temporal part of the sensor response function
    template <typename Value, typename Mask = mask_t<Value>>
    Value sample_time(Value sample, const Mask &/*unused*/) const {
        return m_shutter_open + m_shutter_open_time * sample;
    }

    /**
     * \brief Evaluate the temporal component of the sampling density
     * implemented by the \ref sample_ray() method.
     */
    template <typename Value, typename Measure = like_t<Value, EMeasure>,
              typename Ray = Ray<Point<Value, 3>>,
              typename Mask = mask_t<Value>>
    Value pdf_time(const Ray &ray, Measure measure,
                   const Mask &active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Additional query functions
    // =============================================================

    /**
     * \brief Return the emitted importance for the given surface intersection
     *
     * This is function is used when a sensor has been hit by a
     * ray in a particle tracing-style integrator, and it subsequently needs to
     * be queried for the emitted importance along the negative ray direction.
     *
     * It efficiently computes the product of \ref eval_position()
     * and \ref eval_direction(), though note that it does not include the
     * cosine foreshortening factor of the latter method.
     *
     * This function is provided here as a fast convenience function for
     * unidirectional rendering techniques that support intersecting the
     * sensor. The default implementation throws an exception, which
     * states that the method is not implemented.
     *
     * \param its
     *    An intersect record that specfies the query position
     *
     * \param d
     *    A unit vector, which specifies the query direction
     *
     * \param result
     *    This argument is used to return the 2D sample position
     *    (i.e. the fractional pixel coordinates) associated
     *    with the intersection.
     *
     * \return
     *    The emitted importance
     */
    virtual std::pair<Spectrumf, Point2f> eval(const SurfaceInteraction3f &its,
                                               const Vector3f &d) const;
    std::pair<Spectrumf, Point2f> eval(const SurfaceInteraction3f &its,
                                       const Vector3f &d, bool /*active*/) const {
        return eval(its, d);
    }

    /// Vectorized version of \ref eval
    virtual std::pair<SpectrumfP, Point2fP> eval(
            const SurfaceInteraction3fP &its, const Vector3fP &d,
            const mask_t<FloatP> &active = true) const;


    /**
     * \brief Return the sample position associated with a given
     * position and direction sampling record
     *
     * \param d_rec
     *    A direction sampling record, which specifies the query direction
     *
     * \param p_rec
     *    A position sampling record, which specifies the query position
     *
     * \return \c true if the specified ray is visible by the camera
     */
    virtual std::pair<bool, Point2f> get_sample_position(
            const PositionSample3f &p_rec, const DirectionSample3f &d_rec) const;
    std::pair<bool, Point2f> get_sample_position(
            const PositionSample3f &p_rec, const DirectionSample3f &d_rec,
            bool /*active*/) const {
        return get_sample_position(p_rec, d_rec);
    }

    /// Vectorized version of \ref get_sample_position
    virtual std::pair<BoolP, Point2fP> get_sample_position(
            const PositionSample3fP &p_rec, const DirectionSample3fP &d_rec,
            const mask_t<FloatP> &active = true) const;

    /// Return the time value of the shutter opening event
    inline Float shutter_open() const { return m_shutter_open; }

    /// Set the time value of the shutter opening event
    void set_shutter_open(Float time) { m_shutter_open = time; }

    /// Return the length, for which the shutter remains open
    inline Float shutter_open_time() const { return m_shutter_open_time; }

    /// Set the length, for which the shutter remains open
    void set_shutter_open_time(Float time);

    /**
     * \brief Does the method \ref sample_ray() require a uniformly distributed
     * sample for the time-dependent component?
     */
    inline bool needs_time_sample() const { return !(m_type & EDeltaTime); }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Miscellaneous
    // =============================================================

    /**
     * \brief Does the method \ref sample_ray() require a uniformly
     * distributed sample for the aperture component?
     */
    inline bool needs_aperture_sample() const { return m_type & ENeedsApertureSample; }

    /// Return the \ref Film instance associated with this sensor
    inline Film *film() { return m_film; }

    /// Return the \ref Film instance associated with this sensor (const)
    inline const Film *film() const { return m_film.get(); }

    /// Return the aspect ratio of the sensor and its underlying film
    inline Float aspect() const { return m_aspect; }

    /**
     * \brief Return the sensor's sample generator
     *
     * This is the \a root sampler, which will later be cloned a
     * number of times to provide each participating worker thread
     * with its own instance (see \ref Scene::sampler()).
     * Therefore, this sampler should never be used for anything
     * except creating clones.
     */
    inline Sampler *sampler() { return m_sampler; }

    /**
     * \brief Return the sensor's sampler (const version).
     *
     * This is the \a root sampler, which will later be cloned a
     * number of times to provide each participating worker thread
     * with its own instance (see \ref Scene::sampler()).
     * Therefore, this sampler should never be used for anything
     * except creating clones.
     */
    inline const Sampler *sampler() const { return m_sampler.get(); }

    // /// Serialize this sensor to a binary data stream
    // virtual void serialize(Stream *stream, InstanceManager *manager) const;

    //! @}
    // =============================================================

    MTS_DECLARE_CLASS()

protected:
    Sensor(const Properties &props);

    /// Unserialize a sensor instance from a binary data stream
    // Sensor(Stream *stream, InstanceManager *manager);

    virtual ~Sensor();

    template <typename Value, typename Point2 = Point<Value, 2>,
              typename RayDifferential = RayDifferential<Point<Value, 3>>,
              typename Spectrum = Spectrum<Value>,
              typename Mask = mask_t<Value>>
    std::pair<RayDifferential, Spectrum> sample_ray_differential_impl(
        const Point2 &sample_position, const Point2 &aperture_sample,
        Value time_sample, const Mask &active = true) const;

private:
    void configure();

protected:
    ref<Film> m_film;
    ref<Sampler> m_sampler;
    Vector2f m_resolution;
    Vector2f m_inv_resolution;
    Float m_shutter_open;
    Float m_shutter_open_time;
    Float m_aspect;
};



/**
 * \brief Projective camera interface
 *
 * This class provides an abstract interface to several types of sensors that
 * are commonly used in computer graphics, such as perspective and orthographic
 * camera models.
 *
 * The interface is meant to be implemented by any kind of sensor, whose
 * world to clip space transformation can be explained using only linear
 * operations on homogeneous coordinates.
 *
 * A useful feature of \ref ProjectiveCamera sensors is that their view can be
 * rendered using the traditional OpenGL pipeline.
 *
 * \ingroup librender
 */
class MTS_EXPORT_RENDER ProjectiveCamera : public Sensor {
public:
    using Sensor::world_transform;

    /// Return the world-to-view (aka "view") transformation at time \c t
    inline const Transform4f view_transform_t(Float t,
                                              bool /*unused*/ = true) const {
        return world_transform()->lookup(t).inverse();
    }

    /// Return the view-to-world transformation at time \c t
    inline const Transform4f world_transform_t(Float t,
                                              bool /*unused*/ = true) const {
        return world_transform()->lookup(t);
    }

    /**
     * \brief Returns a projection matrix suitable for rendering the
     * scene using OpenGL.
     *
     * For scenes involving a narrow depth of field and antialiasing,
     * it is necessary to average many separately rendered images using
     * different pixel offsets and aperture positions.
     *
     * \param aperture_sample
     *     Sample for rendering with defocus blur. This should be a
     *     uniformly distributed random point in [0,1]^2 (or any value
     *     when \ref needs_aperture_sample() == \c false)
     *
     * \param aa_sample
     *     Sample for antialiasing. This should be a uniformly
     *     distributed random point in [0,1]^2.
     */
    virtual Transform4f projection_transform(const Point2f &aperture_sample,
            const Point2f &aa_sample) const = 0;

    /// Serialize this camera to a binary data stream
    // virtual void serialize(Stream *stream, InstanceManager *manager) const;

    /// Return the near clip plane distance
    inline Float near_clip() const { return m_near_clip; }

    /// Set the near clip plane distance
    void set_near_clip(Float near_clip);

    /// Return the far clip plane distance
    inline Float far_clip() const { return m_far_clip; }

    /// Set the far clip plane distance
    void set_far_clip(Float far_clip);

    /// Return the distance to the focal plane
    inline Float focus_distance() const { return m_focus_distance; }

    /// Set the distance to the focal plane
    void set_focus_distance(Float focus_distance);

    MTS_DECLARE_CLASS()

protected:
    ProjectiveCamera(const Properties &props);

    // /// Unserialize a camera instance from a binary data stream
    // ProjectiveCamera(Stream *stream, InstanceManager *manager);

    virtual ~ProjectiveCamera();

private:
    void configure() { }

protected:
    Float m_near_clip;
    Float m_far_clip;
    Float m_focus_distance;
};



/**
 * \brief Perspective camera interface.
 *
 * This class provides an abstract interface to several types of sensors that
 * are commonly used in computer graphics, such as perspective and orthographic
 * camera models.
 *
 * The interface is meant to be implemented by any kind of sensor, whose
 * world to clip space transformation can be explained using only linear
 * operations on homogeneous coordinates.
 *
 * \ingroup librender
 */
class MTS_EXPORT_RENDER PerspectiveCamera : public ProjectiveCamera {
public:
    // =============================================================
    //! @{ \name Field of view-related
    // =============================================================

    /// Return the horizontal field of view in degrees
    inline Float x_fov() const { return m_x_fov; }

    /// Set the horizontal field of view in degrees
    void set_x_fov(Float x_fov);

    /// Return the vertical field of view in degrees
    Float y_fov() const;

    /// Set the vertical field of view in degrees
    void set_y_fov(Float y_fov);

    /// Return the diagonal field of view in degrees
    Float diagonal_fov() const;

    /// Set the diagonal field of view in degrees
    void set_diagonal_fov(Float d_fov);

    //! @}
    // =============================================================
    //

    // /// Serialize this camera to a binary data stream
    // virtual void serialize(Stream *stream, InstanceManager *manager) const;

    MTS_DECLARE_CLASS()
protected:
    PerspectiveCamera(const Properties &props);

    // /// Unserialize a perspective camera instance from a binary data stream
    // PerspectiveCamera(Stream *stream, InstanceManager *manager);

    virtual ~PerspectiveCamera();

private:
    void configure();

protected:
    Float m_x_fov;
};


NAMESPACE_END(mitsuba)
