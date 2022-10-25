#pragma once

#include <mitsuba/core/field.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Abstract interface subsuming emitters and sensors in Mitsuba.
 *
 * This class provides an abstract interface to emitters and sensors in
 * Mitsuba, which are named \a endpoints since they represent the first and
 * last vertices of a light path. Thanks to symmetries underlying the equations
 * of light transport and scattering, sensors and emitters can be treated as
 * essentially the same thing, their main difference being type of emitted
 * radiation: light sources emit \a radiance, while sensors emit a conceptual
 * radiation named \a importance. This class casts these symmetries into a
 * unified API that enables access to both types of endpoints using the same
 * set of functions.
 *
 * Subclasses of this interface must implement functions to evaluate and sample
 * the emission/response profile, and to compute probability densities
 * associated with the provided sampling techniques.
 *
 * In addition to :py:meth:`mitsuba.Endpoint.sample_ray`, which generates a
 * sample from the profile, subclasses also provide a specialized <em>direction
 * sampling</em> method in :py:meth:`mitsuba.Endpoint.sample_direction`. This is
 * a generalization of direct illumination techniques to both emitters \a and
 * sensors. A direction sampling method is given an arbitrary reference position
 * in the scene and samples a direction from the reference point towards the
 * endpoint (ideally proportional to the emission/sensitivity profile). This
 * reduces the sampling domain from 4D to 2D, which often enables the
 * construction of smarter specialized sampling techniques.
 *
 * When rendering scenes involving participating media, it is important to know
 * what medium surrounds the sensors and emitters. For this reason, every
 * endpoint instance keeps a reference to a medium (which may be set to
 * \c nullptr when the endpoint is surrounded by vacuum).
 *
 * In the context of polarized simulation, the perfect symmetry between
 * emitters and sensors technically breaks down: the former emit 4D <em>Stokes
 * vectors</em> encoding the polarization state of light, while sensors are
 * characterized by 4x4 <em>Mueller matrices</em> that transform the incident
 * polarization prior to measurement. We sidestep this non-symmetry by simply
 * using Mueller matrices everywhere: in the case of emitters, only the first
 * column will be used (the remainder being filled with zeros). This API
 * simplification comes at a small extra cost in terms of register usage and
 * arithmetic. The JIT (LLVM, CUDA) variants of Mitsuba can recognize
 * these redundancies and remove them retroactively.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Endpoint : public Object {
public:
    MI_IMPORT_TYPES(Medium, Scene, Shape)

    // =============================================================
    //! @{ \name Wavelength sampling interface
    // =============================================================

    /**
     * \brief Importance sample a set of wavelengths according to the
     * endpoint's sensitivity/emission spectrum.
     *
     * This function takes a uniformly distributed 1D sample and generates a
     * sample that is approximately distributed according to the endpoint's
     * spectral sensitivity/emission profile.
     *
     * For this, the input 1D sample is first replicated into
     * <tt>Spectrum::Size</tt> separate samples using simple arithmetic
     * transformations (see \ref math::sample_shifted()), which can be interpreted
     * as a type of Quasi-Monte-Carlo integration scheme. Following this, a
     * standard technique (e.g. inverse transform sampling) is used to find the
     * corresponding wavelengths. Any discrepancies between ideal and actual
     * sampled profile are absorbed into a spectral importance weight that is
     * returned along with the wavelengths.
     *
     * This function should not be called in RGB or monochromatic modes.
     *
     * \param si
     *     In the case of a spatially-varying spectral sensitivity/emission
     *     profile, this parameter conditions sampling on a specific spatial
     *     position. The <tt>si.uv</tt> field must be specified in this case.
     *
     * \param sample
     *     A 1D uniformly distributed random variate
     *
     * \return
     *    The set of sampled wavelengths and (potentially spectrally varying)
     *    importance weights. The latter account for the difference between the
     *    profile and the actual used sampling density function. In the case of
     *    emitters, the weight will include the emitted radiance.
     */
    virtual std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active = true) const;

    /**
     * \brief Evaluate the probability density of the wavelength sampling
     * method implemented by \ref sample_wavelengths().
     *
     * \param wavelengths
     *    The sampled wavelengths.
     *
     * \return
     *    The corresponding sampling density per wavelength (units of 1/nm).
     */
    virtual Spectrum pdf_wavelengths(const Spectrum &wavelengths,
                                     Mask active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray sampling interface
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

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Direction sampling interface
    // =============================================================

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
     *     A uniformly distributed 2D point on the domain <tt>[0,1]^2</tt>.
     *
     * \return
     *     A \ref DirectionSample instance describing the generated sample
     *     along with a spectral importance weight.
     */
    virtual std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &ref,
                     const Point2f &sample,
                     Mask active = true) const;

    /**
     * \brief Evaluate the probability density of the \a direct sampling
     * method implemented by the \ref sample_direction() method.
     *
     * The returned probability will always be zero when the
     * emission/sensitivity profile contains a Dirac delta term (e.g. point or
     * directional emitters/sensors).
     *
     * \param ds
     *    A direct sampling record, which specifies the query
     *    location.
     */
    virtual Float pdf_direction(const Interaction3f &ref,
                                const DirectionSample3f &ds,
                                Mask active = true) const;

    /**
     * \brief Re-evaluate the incident direct radiance/importance of the \ref
     * sample_direction() method.
     *
     * This function re-evaluates the incident direct radiance or importance
     * and sample probability due to the endpoint so that division by
     * <tt>ds.pdf</tt> equals the sampling weight returned by \ref
     * sample_direction(). This may appear redundant, and indeed such a
     * function would not find use in "normal" rendering algorithms.
     *
     * However, the ability to re-evaluate the contribution of a generated
     * sample is important for differentiable rendering. For example, we might
     * want to track derivatives in the sampled direction (<tt>ds.d</tt>)
     * without also differentiating the sampling technique. Alternatively (or
     * additionally), it may be necessary to apply a spherical
     * reparameterization to <tt>ds.d</tt>  to handle visibility-induced
     * discontinuities during differentiation. Both steps require re-evaluating
     * the contribution of the emitter while tracking derivative information
     * through the calculation.
     *
     * In contrast to \ref pdf_direction(), evaluating this function can yield
     * a nonzero result in the case of emission profiles containing a Dirac
     * delta term (e.g. point or directional lights).
     *
     * \param ref
     *    A 3D reference location within the scene, which may influence the
     *    sampling process.
     *
     * \param ds
     *    A direction sampling record, which specifies the query location.
     *
     * \return
     *    The incident direct radiance/importance associated with the sample.
     */
    virtual Spectrum
    eval_direction(const Interaction3f &ref,
                   const DirectionSample3f &ds,
                   Mask active = true) const;

    // =============================================================
    //! @{ \name Position sampling interface
    // =============================================================

    /**
     * \brief Importance sample the spatial component of the
     * emission or importance profile of the endpoint.
     *
     * The default implementation throws an exception.
     *
     * \param time
     *    The scene time associated with the position to be sampled.
     *
     * \param sample
     *     A uniformly distributed 2D point on the domain <tt>[0,1]^2</tt>.
     *
     * \return
     *     A \ref PositionSample instance describing the generated sample
     *     along with an importance weight.
     */
    virtual std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f &sample,
                    Mask active = true) const;

    /**
     * \brief Evaluate the probability density of the position sampling
     * method implemented by \ref sample_position().
     *
     * In simple cases, this will be the reciprocal of the endpoint's
     * surface area.
     *
     * \param ps
     *    The sampled position record.
     * \return
     *    The corresponding sampling density.
     */
    virtual Float pdf_position(const PositionSample3f &ps,
                               Mask active = true) const;

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
     *    An intersect record that specifies both the query position
     *    and direction (using the <tt>si.wi</tt> field)
     * \return
     *    The emitted radiance or importance
     */
    virtual Spectrum eval(const SurfaceInteraction3f &si, Mask active = true) const;


    /// Return the local space to world space transformation
    Transform4f world_transform() const {
        return m_to_world.value();
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
    std::string id() const override { return m_id; }

    /// Set a string identifier
    void set_id(const std::string& id) override { m_id = id; };

    //! @}
    // =============================================================

    void traverse(TraversalCallback *callback) override;

    DRJIT_VCALL_REGISTER(Float, mitsuba::Endpoint)
    MI_DECLARE_CLASS()
protected:
    Endpoint(const Properties &props);

    virtual ~Endpoint();

protected:
    field<Transform4f, ScalarTransform4f> m_to_world;
    ref<Medium> m_medium;
    Shape *m_shape = nullptr;
    bool m_needs_sample_2 = true;
    bool m_needs_sample_3 = true;
    std::string m_id;
};

MI_EXTERN_CLASS(Endpoint)
NAMESPACE_END(mitsuba)
