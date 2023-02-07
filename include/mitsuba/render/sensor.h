#pragma once

#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Sensor : public Endpoint<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Film, Sampler, Texture)
    MI_IMPORT_BASE(Endpoint, sample_ray, m_needs_sample_3)

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
    virtual std::pair<RayDifferential3f, Spectrum>
    sample_ray_differential(Float time, Float sample1,
                            const Point2f &sample2, const Point2f &sample3,
                            Mask active = true) const;

    /**
     * \brief Importance sample a set of wavelengths proportional to the
     * sensitivity spectrum.
     *
     * Any discrepancies between ideal and actual sampled profile are absorbed
     * into a spectral importance weight that is returned along with the wavelengths.
     *
     * In RGB and monochromatic modes, since no wavelengths need to be sampled,
     * this simply returns an empty vector and the value 1.
     *
     * \param sample
     *     A uniformly distributed 1D value that is used to sample the spectral
     *     dimension of the sensitivity profile.
     *
     * \return
     *    The set of sampled wavelengths and importance weights.
     *    The latter account for the difference between the
     *    profile and the actual used sampling density function.
     */
    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active = true) const override;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Additional query functions
    // =============================================================

    /// Return the time value of the shutter opening event
    ScalarFloat shutter_open() const { return m_shutter_open; }

    /// Return the length, for which the shutter remains open
    ScalarFloat shutter_open_time() const { return m_shutter_open_time; }

    /// Does the sampling technique require a sample for the aperture position?
    bool needs_aperture_sample() const { return m_needs_sample_3; }

    /// Return the \ref Film instance associated with this sensor
    Film *film() { return m_film; }

    /// Return the \ref Film instance associated with this sensor (const)
    const Film *film() const { return m_film.get(); }

    /**
     * \brief Return the sensor's sample generator
     *
     * This is the \a root sampler, which will later be forked a
     * number of times to provide each participating worker thread
     * with its own instance (see \ref Scene::sampler()).
     * Therefore, this sampler should never be used for anything
     * except creating forks.
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

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("shutter_open", m_shutter_open,           +ParamFlags::NonDifferentiable);
        callback->put_parameter("shutter_open_time", m_shutter_open_time, +ParamFlags::NonDifferentiable);
        callback->put_object("film", m_film.get(),                        +ParamFlags::NonDifferentiable);
        callback->put_object("sampler", m_sampler.get(),                  +ParamFlags::NonDifferentiable);
        Base::traverse(callback);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        m_resolution = ScalarVector2f(m_film->crop_size());
    }

    DRJIT_VCALL_REGISTER(Float, mitsuba::Sensor)
    MI_DECLARE_CLASS()
protected:
    Sensor(const Properties &props);

    virtual ~Sensor();

protected:
    ref<Film> m_film;
    ref<Sampler> m_sampler;
    ScalarVector2f m_resolution;
    ScalarFloat m_shutter_open;
    ScalarFloat m_shutter_open_time;
    ref<const Texture> m_srf;
    bool m_alpha;
};

//! @}
// -----------------------------------------------------------------------

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
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB ProjectiveCamera : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor)
    MI_IMPORT_TYPES()

    /// Return the near clip plane distance
    ScalarFloat near_clip() const { return m_near_clip; }

    /// Return the far clip plane distance
    ScalarFloat far_clip() const { return m_far_clip; }

    /// Return the distance to the focal plane
    Float focus_distance() const { return m_focus_distance; }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("near_clip",      m_near_clip,      +ParamFlags::NonDifferentiable);
        callback->put_parameter("far_clip",       m_far_clip,       +ParamFlags::NonDifferentiable);
        Base::traverse(callback);
    }

    MI_DECLARE_CLASS()
protected:
    ProjectiveCamera(const Properties &props);

    virtual ~ProjectiveCamera();

protected:
    ScalarFloat m_near_clip;
    ScalarFloat m_far_clip;
    Float m_focus_distance;
};

// ========================================================================
//! @{ \name Functionality common to perspective cameras, projectors, etc.
// ========================================================================

/// Helper function to parse the field of view field of a camera
extern MI_EXPORT_LIB double parse_fov(const Properties &props, double aspect);

/// Helper function to create a perspective projection transformation matrix
template <typename Float> Transform<Point<Float, 4>>
perspective_projection(const Vector<int, 2> &film_size,
                       const Vector<int, 2> &crop_size,
                       const Vector<int, 2> &crop_offset,
                       Float fov_x,
                       Float near_clip, Float far_clip) {

    using Vector2f = Vector<Float, 2>;
    using Vector3f = Vector<Float, 3>;
    using Transform4f = Transform<Point<Float, 4>>;

    Vector2f film_size_f = Vector2f(film_size),
             rel_size    = Vector2f(crop_size) / film_size_f,
             rel_offset  = Vector2f(crop_offset) / film_size_f;

    Float aspect = film_size_f.x() / film_size_f.y();

    /**
     * These do the following (in reverse order):
     *
     * 1. Create transform from camera space to [-1,1]x[-1,1]x[0,1] clip
     *    coordinates (not taking account of the aspect ratio yet)
     *
     * 2+3. Translate and scale to shift the clip coordinates into the
     *    range from zero to one, and take the aspect ratio into account.
     *
     * 4+5. Translate and scale the coordinates once more to account
     *     for a cropping window (if there is any)
     */
    return Transform4f::scale(
               Vector3f(1.f / rel_size.x(), 1.f / rel_size.y(), 1.f)) *
           Transform4f::translate(
               Vector3f(-rel_offset.x(), -rel_offset.y(), 0.f)) *
           Transform4f::scale(Vector3f(-0.5f, -0.5f * aspect, 1.f)) *
           Transform4f::translate(Vector3f(-1.f, -1.f / aspect, 0.f)) *
           Transform4f::perspective(fov_x, near_clip, far_clip);
}

/// Helper function to create a orthographic projection transformation matrix
template <typename Float> Transform<Point<Float, 4>>
orthographic_projection(const Vector<int, 2> &film_size,
                        const Vector<int, 2> &crop_size,
                        const Vector<int, 2> &crop_offset,
                        Float near_clip, Float far_clip) {

    using Vector2f = Vector<Float, 2>;
    using Vector3f = Vector<Float, 3>;
    using Transform4f = Transform<Point<Float, 4>>;

    Vector2f film_size_f = Vector2f(film_size),
             rel_size    = Vector2f(crop_size) / film_size_f,
             rel_offset  = Vector2f(crop_offset) / film_size_f;

    Float aspect = film_size_f.x() / film_size_f.y();

    /**
     * These do the following (in reverse order):
     *
     * 1. Create transform from camera space to [-1,1]x[-1,1]x[0,1] clip
     *    coordinates (not taking account of the aspect ratio yet)
     *
     * 2+3. Translate and scale to shift the clip coordinates into the
     *    range from zero to one, and take the aspect ratio into account.
     *
     * 4+5. Translate and scale the coordinates once more to account
     *     for a cropping window (if there is any)
     */
    return Transform4f::scale(
               Vector3f(1.f / rel_size.x(), 1.f / rel_size.y(), 1.f)) *
           Transform4f::translate(
               Vector3f(-rel_offset.x(), -rel_offset.y(), 0.f)) *
           Transform4f::scale(Vector3f(-0.5f, -0.5f * aspect, 1.f)) *
           Transform4f::translate(Vector3f(-1.f, -1.f / aspect, 0.f)) *
           Transform4f::orthographic(near_clip, far_clip);
}

//! @}
// ========================================================================

MI_EXTERN_CLASS(Sensor)
MI_EXTERN_CLASS(ProjectiveCamera)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Dr.Jit support for vectorized function calls
// -----------------------------------------------------------------------

DRJIT_VCALL_TEMPLATE_BEGIN(mitsuba::Sensor)
    DRJIT_VCALL_METHOD(sample_ray)
    DRJIT_VCALL_METHOD(sample_ray_differential)
    DRJIT_VCALL_METHOD(sample_direction)
    DRJIT_VCALL_METHOD(pdf_direction)
    DRJIT_VCALL_METHOD(eval_direction)
    DRJIT_VCALL_METHOD(sample_position)
    DRJIT_VCALL_METHOD(pdf_position)
    DRJIT_VCALL_METHOD(eval)
    DRJIT_VCALL_METHOD(sample_wavelengths)
    DRJIT_VCALL_GETTER(flags, uint32_t)
    DRJIT_VCALL_GETTER(shape, const typename Class::Shape *)
    DRJIT_VCALL_GETTER(medium, const typename Class::Medium *)
DRJIT_VCALL_TEMPLATE_END(mitsuba::Sensor)
