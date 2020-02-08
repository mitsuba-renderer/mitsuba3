#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/traits.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Medium : public Object {
public:
    MTS_IMPORT_TYPES(PhaseFunction, Sampler, Scene, Texture);

    // =============================================================
    //! @{ \name Medium sampling strategy
    // =============================================================

    /**
     * \brief Sample a free-flight distance in the medium.
     *
     * Should ideally importance sample with respect to the transmittance.
     *
     * \param scene    A pointer to the current scene
     * \param ray      Ray, along which a distance should be sampled
     * \param sampler  Sampler to produce random numbers. Note that some media
     * (e.g. heterogenous media) may require an arbitrary number of random
     * numbers to sample a distance.
     *
     * \return         This method returns both a medium and a surface
     *                 interaction. If a medium interaction was sampled, a valid
     *                 MediumInteraction is returned. If the sampled distance
     * results in an intersection with a surface, the corresponding
     * SurfaceInteraction is returned. Further, a spectrum with the sampled
     * throughput value is returned.
     */
    virtual std::tuple<SurfaceInteraction3f, MediumInteraction3f, Spectrum>
    sample_distance(const Scene *scene, const Ray3f &ray, const Point2f &sample, Sampler *sampler,
                    Mask active = true) const = 0;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Functions for querying the medium
    // =============================================================

    /**
     * \brief Compute the transmittance along a ray segment
     *
     * Computes the transmittance along a ray segment
     * [mint, maxt] associated with the ray. It is assumed
     * that the ray has a normalized direction value.
     */
    virtual Spectrum eval_transmittance(const Ray3f &ray, Sampler *sampler,
                                        Mask active = true) const = 0;

    // NEW INTERFACE
    virtual std::tuple<Mask, Float, Float> intersect_aabb(const Ray3f &ray) const = 0;

    virtual UnpolarizedSpectrum get_combined_extinction(const MediumInteraction3f &mi, Mask active = true) const = 0;
    virtual std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi, Mask active = true) const = 0;

    MediumInteraction3f sample_interaction(const Ray3f &ray, Float sample,
                                           UInt32 channel, Mask active) const;
    std::pair<UnpolarizedSpectrum, UnpolarizedSpectrum>
    eval_tr_and_pdf(const MediumInteraction3f &mi, const SurfaceInteraction3f &si, Mask active) const;

    // END NEW INTERFACE


    /// Return the phase function of this medium
    MTS_INLINE const PhaseFunction *phase_function() const { return m_phase_function.get(); }

    /// Returns whether this specific medium instance uses emitter sampling
    MTS_INLINE bool use_emitter_sampling() const { return m_sample_emitters; }

    /// Return a string identifier
    std::string id() const override { return m_id; }

    /// Return a human-readable representation of the Medium
    std::string to_string() const override = 0;

    /**
     * Extracts the following medium parameters from the given properties:
     * - sigma_a (absoption)
     * - sigma_s (scattering)
     * - sigma_t (transmission)
     * - albedo
     */
    // static std::tuple<ref<Texture>, ref<Texture>,
    //                   ref<Texture>, ref<Texture>>
    // extract_medium_parameters(const Properties &props);

    ENOKI_PINNED_OPERATOR_NEW(Float)
    MTS_DECLARE_CLASS()
protected:
    Medium();
    Medium(const Properties &props);
    virtual ~Medium();

protected:
    ref<PhaseFunction> m_phase_function;
    bool m_sample_emitters;

    /// Identifier (if available)
    std::string m_id;
};

MTS_EXTERN_CLASS_RENDER(Medium)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for packets of Medium pointers
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
ENOKI_CALL_SUPPORT_TEMPLATE_BEGIN(mitsuba::Medium)
    ENOKI_CALL_SUPPORT_METHOD(sample_distance)
    ENOKI_CALL_SUPPORT_METHOD(eval_transmittance)
    ENOKI_CALL_SUPPORT_METHOD(phase_function)
    ENOKI_CALL_SUPPORT_METHOD(use_emitter_sampling)
    ENOKI_CALL_SUPPORT_METHOD(get_combined_extinction)
    ENOKI_CALL_SUPPORT_METHOD(intersect_aabb)
    ENOKI_CALL_SUPPORT_METHOD(sample_interaction)
    ENOKI_CALL_SUPPORT_METHOD(eval_tr_and_pdf)
    ENOKI_CALL_SUPPORT_METHOD(get_scattering_coefficients)
ENOKI_CALL_SUPPORT_TEMPLATE_END(mitsuba::Medium)

//! @}
// -----------------------------------------------------------------------
