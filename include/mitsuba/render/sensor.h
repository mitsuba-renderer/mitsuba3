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
    // =============================================================
    //! @{ \name Sensor-specific sampling functions
    // =============================================================

    /**
     * \brief Importance sample a ray differential proportional to the sensor's
     * sensitivity profile.
     *
     * The sensor profile is a six-dimensional quantity that depends on time,
     * wavelength, surface position, and direction. This function takes a given
     * time value and five uniformly distributed samples on the interval [0, 1]
     * and warps them so that the returned ray the profile. Any
     * discrepancies between ideal and actual sampled profile are absorbed into
     * a spectral importance weight that is returned along with the ray.
     *
     * In contrast to \ref Endpoint::sample_ray(), this function returns
     * differentials with respect to the X and Y axis in screen space.
     *
     * \param time
     *    The scene time associated with the ray_differential to be sampled
     *
     * \param sample1
     *     A uniformly distributed 1D value that is used to sample the spectral
     *     dimension of the sensitivity profile.
     *
     * \param sample2
     *    This argument corresponds to the sample position in fractional pixel
     *    coordinates relative to the crop window of the underlying film.
     *
     * \param sample3
     *    A uniformly distributed sample on the domain <tt>[0,1]^2</tt>. This
     *    argument determines the position on the aperture of the sensor. This
     *    argument is ignored if <tt>needs_sample_3() == false</tt>.
     *
     * \return
     *    The sampled ray differential and (potentially spectrally varying)
     *    importance weights. The latter account for the difference between the
     *    sensor profile and the actual used sampling density function.
     */
    virtual std::pair<RayDifferential3f, Spectrumf>
    sample_ray_differential(Float time,
                            Float sample1,
                            const Point2f &sample2,
                            const Point2f &sample3) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref sample_ray_differential()
    std::pair<RayDifferential3f, Spectrumf>
    sample_ray_differential(Float time, Float sample1, const Point2f &sample2,
                            const Point2f &sample3, bool /* unused */) const {
        return sample_ray_differential(time, sample1, sample2, sample3);
    }

    /// Vectorized version of \ref sample_ray_differential
    virtual std::pair<RayDifferential3fP, SpectrumfP>
    sample_ray_differential(FloatP time,
                            FloatP sample1,
                            const Point2fP &sample2,
                            const Point2fP &sample3,
                            MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref sample_ray_differential
    virtual std::pair<RayDifferential3fD, SpectrumfD>
    sample_ray_differential(FloatD time,
                            FloatD sample1,
                            const Point2fD &sample2,
                            const Point2fD &sample3,
                            MaskD active = true) const;
#endif

    /**
     * \brief Polarized version of \ref sample_ray()
     *
     * Since there is no special polarized importance sampling
     * this method behaves very similar to the standard one.
     *
     * \return
     *    The sampled ray differential and the Mueller matrix of importance
     *    weights (in standard world space for the sensor profile).
     */
    virtual std::pair<RayDifferential3f, MuellerMatrixSf>
    sample_ray_differential_pol(Float time,
                                Float sample1,
                                const Point2f &sample2,
                                const Point2f &sample3) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref sample_ray_differential_pos()
    std::pair<RayDifferential3f, MuellerMatrixSf>
    sample_ray_differential_pol(Float time,
                                Float sample1,
                                const Point2f &sample2,
                                const Point2f &sample3,
                                bool /* unused */) const {
        return sample_ray_differential_pol(time, sample1, sample2, sample3);
    }

    /// Vectorized version of \ref sample_ray_differential_pol()
    virtual std::pair<RayDifferential3fP, MuellerMatrixSfP>
    sample_ray_differential_pol(FloatP time,
                                FloatP sample1,
                                const Point2fP &sample2,
                                const Point2fP &sample3,
                                MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref sample_ray_differential_pol()
    virtual std::pair<RayDifferential3fD, MuellerMatrixSfD>
    sample_ray_differential_pol(FloatD time,
                                FloatD sample1,
                                const Point2fD &sample2,
                                const Point2fD &sample3,
                                MaskD active = true) const;
#endif

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Additional query functions
    // =============================================================

    /// Return the time value of the shutter opening event
    Float shutter_open() const { return m_shutter_open; }

    /// Return the length, for which the shutter remains open
    Float shutter_open_time() const { return m_shutter_open_time; }

    /// Does the sampling technique require a sample for the aperture position?
    bool needs_aperture_sample() const { return m_needs_sample_3; }

    /// Return the \ref Film instance associated with this sensor
    Film *film() { return m_film; }

    /// Return the \ref Film instance associated with this sensor (const)
    const Film *film() const { return m_film.get(); }

    /**
     * \brief Return the sensor's sample generator
     *
     * This is the \a root sampler, which will later be cloned a
     * number of times to provide each participating worker thread
     * with its own instance (see \ref Scene::sampler()).
     * Therefore, this sampler should never be used for anything
     * except creating clones.
     */
    Sampler *sampler() { return m_sampler; }

    /**
     * \brief Return the sensor's sampler (const version).
     *
     * This is the \a root sampler, which will later be cloned a
     * number of times to provide each participating worker thread
     * with its own instance (see \ref Scene::sampler()).
     * Therefore, this sampler should never be used for anything
     * except creating clones.
     */
    const Sampler *sampler() const { return m_sampler.get(); }

    //! @}
    // =============================================================

    MTS_DECLARE_CLASS()

protected:
    Sensor(const Properties &props);

    virtual ~Sensor();

    template <typename Value, typename Point2 = Point<Value, 2>,
              typename RayDifferential = RayDifferential<Point<Value, 3>>,
              typename Spectrum        = Spectrum<Value>,
              typename Mask            = mask_t<Value>>
    std::pair<RayDifferential, Spectrum>
    sample_ray_differential_impl(Value time, Value sample1,
                                 const Point2 &sample2,
                                 const Point2 &sample3,
                                 Mask active) const;

    template <typename Value, typename Point2 = Point<Value, 2>,
              typename RayDifferential = RayDifferential<Point<Value, 3>>,
              typename Spectrum = Spectrum<Value>,
              typename Mask = mask_t<Value>>
    std::pair<RayDifferential, MuellerMatrix<Spectrum>>
    sample_ray_differential_pol_impl(Value time, Value sample1,
                                     const Point2 &sample2,
                                     const Point2 &sample3,
                                     Mask active) const;

protected:
    ref<Film> m_film;
    ref<Sampler> m_sampler;
    Vector2f m_resolution;
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

    /// Return the near clip plane distance
    Float near_clip() const { return m_near_clip; }

    /// Return the far clip plane distance
    Float far_clip() const { return m_far_clip; }

    /// Return the distance to the focal plane
    Float focus_distance() const { return m_focus_distance; }

    MTS_DECLARE_CLASS()

protected:
    ProjectiveCamera(const Properties &props);

    virtual ~ProjectiveCamera();

protected:
    Float m_near_clip;
    Float m_far_clip;
    Float m_focus_distance;
};

NAMESPACE_END(mitsuba)

#include "detail/sensor.inl"
