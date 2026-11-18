#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/traits.h>
#include <mitsuba/render/fwd.h>
#include <drjit/call.h>

NAMESPACE_BEGIN(mitsuba)

/// Data structure holding the result of real interaction sampling.
template <typename Float, typename Spectrum> struct MediumSample3 {
    MI_IMPORT_TYPES()

    /// Real collision, \c mei.t is infinite if none occurred
    MediumInteraction3f mei;

    /// Sampling densities of all channels relative to the hero channel
    UnpolarizedSpectrum pdf_ratio;

    DRJIT_STRUCT(MediumSample3, mei, pdf_ratio)
};

template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Medium : public JitObject<Medium<Float, Spectrum>> {
public:
    MI_IMPORT_TYPES(PhaseFunction, Sampler, Scene, Texture, Extremum);

    /// Destructor
    ~Medium();

    /// Intersects a ray with the medium's bounding box
    virtual std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f &ray) const = 0;

    /// Returns the medium's majorant used for delta tracking
    virtual UnpolarizedSpectrum
    get_majorant(const MediumInteraction3f &mi,
                 Mask active = true) const = 0;

    /// Returns the medium coefficients :math:`\sigma_s`, :math:`\sigma_n` and
    /// :math:`\sigma_t` evaluated at a given `MediumInteraction3f` ``mi``
    virtual std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum,
                       UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi,
                                Mask active = true) const = 0;

    /**
     * Sample a free-flight distance in the medium.
     *
     * This function samples a (tentative) free-flight distance according to an
     * exponential transmittance. It is then up to the integrator to then decide
     * whether the `MediumInteraction3f` corresponds to a real or null scattering
     * event.
     *
     * Args:
     *     ray: Ray, along which a distance should be sampled
     *
     *     sample: A uniformly distributed random sample
     *
     *     channel: The channel according to which we will sample the
     *         free-flight distance. This argument is only used when rendering in RGB
     *         modes.
     *
     * Returns:
     *     This method returns a `MediumInteraction3f`.
     *     The MediumInteraction will always be valid,
     *     except if the ray missed the Medium's bounding box.
     */
    MediumInteraction3f sample_interaction(const Ray3f &ray, Float sample,
                                           UInt32 channel, Mask active) const;

    /**
     * Compute the transmittance and PDF
     *
     * This function evaluates the transmittance and PDF of sampling a certain
     * free-flight distance The returned PDF takes into account if a medium
     * interaction occurred (``mi.t <= si.t``) or the ray left the medium
     * (``mi.t > si.t``)
     *
     * The evaluated PDF is spectrally varying. This allows to account for the
     * fact that the free-flight distance sampling distribution can depend on
     * the wavelength.
     *
     * Returns:
     *     This method returns a pair of ``(Transmittance, PDF)``.
     */
    std::pair<UnpolarizedSpectrum, UnpolarizedSpectrum>
    transmittance_eval_pdf(const MediumInteraction3f &mi,
                           const SurfaceInteraction3f &si,
                           Mask active) const;

    /**
     * \brief Sample a real collision along \c ray, over its part inside the
     * medium's bounding box and <tt>[0, ray.maxt]</tt>.
     *
     * The default implementation uses delta tracking. Free flights are
     * sampled at the hero channel's majorant, given by the medium's extremum
     * structure. At a tentative collision, the event type (null or real) is
     * sampled using the medium's coefficients. Tracking stops at the first
     * real collision. A ray that misses that range returns no collision.
     *
     * \param sample
     *      Uniform sample in <tt>[0, 1)</tt> that draws the first free flight
     *      and seeds the generator of the later free flights and of the
     *      collision choices.
     * \param channel
     *      Hero channel, along which free flights are sampled.
     *
     * \return A pair (sample, weight): the collision and path throughput weight.
     */
    virtual std::pair<MediumSample3f, UnpolarizedSpectrum>
    sample_real_interaction(const Ray3f &ray, Float sample,
                            const UInt32 &channel, Mask active = true) const;

    /**
     * \brief Estimate the transmittance along \c ray.
     *
     * The default implementation uses (residual) ratio tracking: as
     * \ref sample_real_interaction, except that every collision is null and
     * the traversal runs to the end of the range. The property \c use_rrt
     * specifies whether the minorant is used as control extinction tracking,
     * the minorants are a control extinction, accounted for analytically,
     * and free flights are sampled at the residual majorants.
     *
     * Traversal stops as soon as the transmittance estimate reaches zero.
     * If the medium's \c rr_threshold is positive, estimates falling below
     * it undergo Russian roulette.
     */
    virtual UnpolarizedSpectrum
    sample_transmittance(const Ray3f &ray, Float sample, const UInt32 &channel,
                         Mask active = true) const;

    /**
     * \brief Estimate the transmittance, path throughput, and pdf ratio
     * along \c ray.
     *
     * As \ref sample_transmittance, but also returns the path throughput
     * weight and the sampling densities of all channels relative to the hero
     * channel. Cannot use residual ratio tracking.
     *
     * \return A tuple (transmittance, throughput, pdf_ratio).
     */
    virtual
    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    sample_transmittance_pdf(const Ray3f &ray, Float sample,
                             const UInt32 &channel, Mask active = true) const;

    /// Return the phase function of this medium
    MI_INLINE const PhaseFunction *phase_function() const {
        return m_phase_function.get();
    }

    /// Returns whether this specific medium instance uses emitter sampling
    MI_INLINE bool use_emitter_sampling() const { return m_sample_emitters; }

    /// Returns whether this medium is homogeneous
    MI_INLINE bool is_homogeneous() const { return m_is_homogeneous; }

    /// Returns whether this medium has a spectrally varying extinction
    MI_INLINE bool has_spectral_extinction() const {
        return m_has_spectral_extinction;
    }

    /// Return the extremum structure of this medium
    MI_INLINE const Extremum *extremum() const { return m_extremum.get(); }

    void traverse(TraversalCallback *callback) override;

    /// Return a human-readable representation of the Medium
    std::string to_string() const override = 0;

    MI_DECLARE_PLUGIN_BASE_CLASS(Medium)

protected:
    Medium();
    Medium(const Properties &props);

    /// Loop state of \ref track, defined in medium.cpp
    struct TrackingRecord;

    /**
     * \brief Tracking loop shared by \ref sample_real_interaction,
     * \ref sample_transmittance and \ref sample_transmittance_pdf:
     * \c Ratio makes every collision null.
     */
    template <bool Ratio>
    TrackingRecord track(const Ray3f &ray, Float sample, const UInt32 &channel,
                         Mask rrt, Mask active) const;

protected:
    ref<PhaseFunction> m_phase_function;
    bool m_sample_emitters;
    bool m_is_homogeneous;
    bool m_has_spectral_extinction;

    ref<Extremum> m_extremum;
    bool m_use_rrt;
    /// Ratio tracking Russian roulette threshold, disabled if not positive.
    ScalarFloat m_rr_threshold = 0.f;

    MI_DECLARE_TRAVERSE_CB(m_phase_function, m_extremum)
};

MI_EXTERN_CLASS(Medium)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
// Enables vectorized method calls on Dr.Jit medium arrays
// -----------------------------------------------------------------------

DRJIT_CALL_TEMPLATE_BEGIN(mitsuba::Medium)
    DRJIT_CALL_GETTER(phase_function)
    DRJIT_CALL_GETTER(use_emitter_sampling)
    DRJIT_CALL_GETTER(is_homogeneous)
    DRJIT_CALL_GETTER(has_spectral_extinction)
    DRJIT_CALL_METHOD(get_majorant)
    DRJIT_CALL_METHOD(intersect_aabb)
    DRJIT_CALL_METHOD(sample_interaction)
    DRJIT_CALL_METHOD(transmittance_eval_pdf)
    DRJIT_CALL_METHOD(get_scattering_coefficients)
    DRJIT_CALL_METHOD(sample_real_interaction)
    DRJIT_CALL_METHOD(sample_transmittance)
    DRJIT_CALL_METHOD(sample_transmittance_pdf)
DRJIT_CALL_END()

// -----------------------------------------------------------------------
