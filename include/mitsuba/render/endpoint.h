#pragma once

#include <mitsuba/core/field.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/medium.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Abstract interface subsuming emitters and sensors in Mitsuba.
 *
 * This class provides an abstract interface to emitters and sensors in
 * Mitsuba, which are named *endpoints* since they represent the first and
 * last vertices of a light path. Thanks to symmetries underlying the equations
 * of light transport and scattering, sensors and emitters can be treated as
 * essentially the same thing, their main difference being type of emitted
 * radiation: light sources emit *radiance*, while sensors emit a conceptual
 * radiation named *importance*. This class casts these symmetries into a
 * unified API that enables access to both types of endpoints using the same
 * set of functions.
 *
 * Subclasses of this interface must implement functions to evaluate and sample
 * the emission/response profile, and to compute probability densities
 * associated with the provided sampling techniques.
 *
 * In addition to :py:meth:`Endpoint.sample_ray`, which generates a
 * sample from the profile, subclasses also provide a specialized *direction
 * sampling* method in :py:meth:`Endpoint.sample_direction`. This is
 * a generalization of direct illumination techniques to both emitters *and*
 * sensors. A direction sampling method is given an arbitrary reference position
 * in the scene and samples a direction from the reference point towards the
 * endpoint (ideally proportional to the emission/sensitivity profile). This
 * reduces the sampling domain from 4D to 2D, which often enables the
 * construction of smarter specialized sampling techniques.
 *
 * When rendering scenes involving participating media, it is important to know
 * what medium surrounds the sensors and emitters. For this reason, every
 * endpoint instance keeps a reference to a medium (which may be set to
 * ``nullptr`` when the endpoint is surrounded by vacuum).
 *
 * In the context of polarized simulation, the perfect symmetry between
 * emitters and sensors technically breaks down: the former emit 4D *Stokes
 * vectors* encoding the polarization state of light, while sensors are
 * characterized by 4x4 *Mueller matrices* that transform the incident
 * polarization prior to measurement. We sidestep this non-symmetry by simply
 * using Mueller matrices everywhere: in the case of emitters, only the first
 * column will be used (the remainder being filled with zeros). This API
 * simplification comes at a small extra cost in terms of register usage and
 * arithmetic. The JIT (LLVM, CUDA) variants of Mitsuba can recognize
 * these redundancies and remove them retroactively.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Endpoint : public JitObject<Endpoint<Float, Spectrum>> {
public:
    MI_IMPORT_TYPES(Medium, Scene, Shape)
    static constexpr const char *Variant = detail::variant<Float, Spectrum>::name;
    static constexpr const char *Domain = "Endpoint";
    static constexpr ObjectType Type = ObjectType::Unknown; // Endpoint is not a concrete type

    // =============================================================
    // Wavelength sampling interface
    // =============================================================

    /// Destructor
    ~Endpoint();

    /**
     * Importance sample a set of wavelengths according to the
     * endpoint's sensitivity/emission spectrum.
     *
     * This function takes a uniformly distributed 1D sample and generates a
     * sample that is approximately distributed according to the endpoint's
     * spectral sensitivity/emission profile.
     *
     * For this, the input 1D sample is first replicated into
     * ``Spectrum::Size`` separate samples using simple arithmetic
     * transformations (see ``math.sample_shifted()``), which can be interpreted
     * as a type of Quasi-Monte-Carlo integration scheme. Following this, a
     * standard technique (e.g. inverse transform sampling) is used to find the
     * corresponding wavelengths. Any discrepancies between ideal and actual
     * sampled profile are absorbed into a spectral importance weight that is
     * returned along with the wavelengths.
     *
     * This function should not be called in RGB or monochromatic modes.
     *
     * Args:
     *     si: In the case of a spatially-varying spectral sensitivity/emission
     *         profile, this parameter conditions sampling on a specific spatial
     *         position. The ``si.uv`` field must be specified in this case.
     *
     *     sample: A 1D uniformly distributed random variate
     *
     * Returns:
     *     The set of sampled wavelengths and (potentially spectrally varying)
     *     importance weights. The latter account for the difference between the
     *     profile and the actual used sampling density function. In the case of
     *     emitters, the weight will include the emitted radiance.
     */
    virtual std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active = true) const;

    /**
     * Evaluate the probability density of the wavelength sampling
     * method implemented by `sample_wavelengths()`.
     *
     * Args:
     *     wavelengths: The sampled wavelengths.
     *
     * Returns:
     *     The corresponding sampling density per wavelength (units of 1/nm).
     */
    virtual Spectrum pdf_wavelengths(const Spectrum &wavelengths,
                                     Mask active = true) const;

    // =============================================================

    // =============================================================
    // Ray sampling interface
    // =============================================================

    /**
     * Importance sample a ray proportional to the endpoint's
     * sensitivity/emission profile.
     *
     * The endpoint profile is a six-dimensional quantity that depends on time,
     * wavelength, surface position, and direction. This function takes a given
     * time value and five uniformly distributed samples on the interval [0, 1]
     * and warps them so that the returned ray follows the profile. Any
     * discrepancies between ideal and actual sampled profile are absorbed into
     * a spectral importance weight that is returned along with the ray.
     *
     * Args:
     *     time: The scene time associated with the ray to be sampled
     *
     *     sample1: A uniformly distributed 1D value that is used to sample the spectral
     *         dimension of the emission profile.
     *
     *     sample2: A uniformly distributed sample on the domain ``[0,1]^2``. For
     *         sensor endpoints, this argument corresponds to the sample position in
     *         fractional pixel coordinates relative to the crop window of the
     *         underlying film.
     *         This argument is ignored if ``needs_sample_2() == false``.
     *
     *     sample3: A uniformly distributed sample on the domain ``[0,1]^2``. For
     *         sensor endpoints, this argument determines the position on the
     *         aperture of the sensor.
     *         This argument is ignored if ``needs_sample_3() == false``.
     *
     * Returns:
     *     The sampled ray and (potentially spectrally varying) importance
     *     weights. The latter account for the difference between the profile
     *     and the actual used sampling density function.
     */
    virtual std::pair<Ray3f, Spectrum>
    sample_ray(Float time, Float sample1, const Point2f &sample2,
               const Point2f &sample3, Mask active = true) const;

    // =============================================================

    // =============================================================
    // Direction sampling interface
    // =============================================================

    /**
     * Given a reference point in the scene, sample a direction from the
     * reference point towards the endpoint (ideally proportional to the
     * emission/sensitivity profile)
     *
     * This operation is a generalization of direct illumination techniques to
     * both emitters *and* sensors. A direction sampling method is given an
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
     * Args:
     *     it: A reference position somewhere within the scene.
     *
     *     sample: A uniformly distributed 2D point on the domain ``[0,1]^2``.
     *
     * Returns:
     *     A `DirectionSample3f` instance describing the generated sample
     *     along with a spectral importance weight.
     */
    virtual std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it,
                     const Point2f &sample,
                     Mask active = true) const;

    /**
     * Evaluate the probability density of the *direct* sampling
     * method implemented by the `sample_direction()` method.
     *
     * The returned probability will always be zero when the
     * emission/sensitivity profile contains a Dirac delta term (e.g. point or
     * directional emitters/sensors).
     *
     * Args:
     *     it: A 3D reference location within the scene, which may influence the
     *         sampling process.
     *
     *     ds: A direct sampling record, which specifies the query
     *         location.
     */
    virtual Float pdf_direction(const Interaction3f &it,
                                const DirectionSample3f &ds,
                                Mask active = true) const;

    /**
     * Re-evaluate the incident direct radiance/importance of the
     * `sample_direction()` method.
     *
     * This function re-evaluates the incident direct radiance or importance
     * and sample probability due to the endpoint so that division by
     * ``ds.pdf`` equals the sampling weight returned by
     * `sample_direction()`. This may appear redundant, and indeed such a
     * function would not find use in "normal" rendering algorithms.
     *
     * However, the ability to re-evaluate the contribution of a generated
     * sample is important for differentiable rendering. For example, we might
     * want to track derivatives in the sampled direction (``ds.d``)
     * without also differentiating the sampling technique.
     *
     * In contrast to `pdf_direction()`, evaluating this function can yield
     * a nonzero result in the case of emission profiles containing a Dirac
     * delta term (e.g. point or directional lights).
     *
     * Args:
     *     it: A 3D reference location within the scene, which may influence the
     *         sampling process.
     *
     *     ds: A direction sampling record, which specifies the query location.
     *
     * Returns:
     *     The incident direct radiance/importance associated with the sample.
     */
    virtual Spectrum
    eval_direction(const Interaction3f &it,
                   const DirectionSample3f &ds,
                   Mask active = true) const;

    // =============================================================
    // Position sampling interface
    // =============================================================

    /**
     * Importance sample the spatial component of the
     * emission or importance profile of the endpoint.
     *
     * The default implementation throws an exception.
     *
     * Args:
     *     time: The scene time associated with the position to be sampled.
     *
     *     sample: A uniformly distributed 2D point on the domain ``[0,1]^2``.
     *
     * Returns:
     *     A `PositionSample3f` instance describing the generated sample
     *     along with an importance weight.
     */
    virtual std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f &sample,
                    Mask active = true) const;

    /**
     * Evaluate the probability density of the position sampling
     * method implemented by `sample_position()`.
     *
     * In simple cases, this will be the reciprocal of the endpoint's
     * surface area.
     *
     * Args:
     *     ps: The sampled position record.
     *
     * Returns:
     *     The corresponding sampling density.
     */
    virtual Float pdf_position(const PositionSample3f &ps,
                               Mask active = true) const;

    // =============================================================
    // Other query functions
    // =============================================================

    /**
     * Given a ray-surface intersection, return the emitted
     * radiance or importance traveling along the reverse direction
     *
     * This function is e.g. used when an area light source has been hit by a
     * ray in a path tracing-style integrator, and it subsequently needs to be
     * queried for the emitted radiance along the negative ray direction. The
     * default implementation throws an exception, which states that the method
     * is not implemented.
     *
     * Args:
     *     si: An intersect record that specifies both the query position
     *         and direction (using the ``si.wi`` field)
     *
     * Returns:
     *     The emitted radiance or importance
     */
    virtual Spectrum eval(const SurfaceInteraction3f &si, Mask active = true) const;


    /// Return the local space to world space transformation
    AffineTransform4f world_transform() const {
        return m_to_world.value();
    }

    /**
     * Does the method `sample_ray()` require a uniformly distributed
     * 2D sample for the ``sample2`` parameter?
     */
    bool needs_sample_2() const { return m_needs_sample_2; }

    /**
     * Does the method `sample_ray()` require a uniformly distributed
     * 2D sample for the ``sample3`` parameter?
     */
    bool needs_sample_3() const { return m_needs_sample_3; }


    // =============================================================


    // =============================================================
    // Miscellaneous
    // =============================================================

    /// Return the `Shape` to which the emitter is currently attached
    Shape *shape() { return m_shape; }

    /// Return the `Shape` to which the emitter is currently attached (const version)
    const Shape *shape() const { return m_shape; }

    /// Return a pointer to the `Medium` that surrounds the emitter
    Medium *medium() { return m_medium; }

    /// Return a pointer to the `Medium` that surrounds the emitter (const version)
    const Medium *medium() const { return m_medium.get(); }

    /**
     * Return an axis-aligned box bounding the spatial
     * extents of the emitter
     */
    virtual ScalarBoundingBox3f bbox() const = 0;

    /// Set the shape associated with this endpoint.
    virtual void set_shape(Shape *shape);

    /// Set the medium that surrounds the emitter.
    virtual void set_medium(Medium *medium);

    /**
     * Inform the emitter about the properties of the scene
     *
     * Various emitters that surround the scene (e.g. environment emitters)
     * must be informed about the scene dimensions to operate correctly.
     * This function is invoked by the `Scene` constructor.
     */
    virtual void set_scene(const Scene *scene);

    // =============================================================

    void traverse(TraversalCallback *callback) override;

    void parameters_changed(const std::vector<std::string> &keys = {}) override;

    MI_DECLARE_CLASS(Endpoint)

protected:
    Endpoint(const Properties &props);
    Endpoint(const Properties &props, ObjectType type);

protected:
    field<AffineTransform4f, ScalarAffineTransform4f> m_to_world;
    ref<Medium> m_medium;
    Shape *m_shape = nullptr;
    bool m_needs_sample_2 = true;
    bool m_needs_sample_3 = true;

    MI_DECLARE_TRAVERSE_CB(m_to_world, m_medium)
};

MI_EXTERN_CLASS(Endpoint)
NAMESPACE_END(mitsuba)
