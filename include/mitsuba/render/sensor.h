#pragma once

#include <mitsuba/core/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Sensor : public Object {
public:

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
     *    \ref needsApertureSample() == \c false)
     *
     * \param time_sample
     *    A uniformly distributed 1D vector that is used to sample the temporal
     *    component of the emission profile. (Or any value when \ref
     *    needsTimeSample() == \c false)
     *
     * \return
     *    The sampled ray and an associated importance weight associated. This
     *    accounts for the difference between the sensor response and the
     *    sampling density function.
     */
    virtual std::pair<Ray3f, Spectrumf> sample_ray(
        const Point2f &position_sample,
        const Point2f &aperture_sample,
        Float time_sample) const = 0;

    /// Vectorized version of \ref sample_ray
    virtual std::pair<Ray3fP, SpectrumfP> sample_ray(
        const Point2fP &position_sample,
        const Point2fP &aperture_sample,
        FloatP time_sample) const = 0;

    //! @}
    // =============================================================

    MTS_DECLARE_CLASS()

protected:
    Sensor(const Properties &props);
    virtual ~Sensor();

protected:
    Float m_shutter_open;
    Float m_shutter_open_time;
};

NAMESPACE_END(mitsuba)
