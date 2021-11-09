#pragma once

#include <enoki/vcall.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/traits.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/volume.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Medium : public Object {
public:
    MTS_IMPORT_TYPES(PhaseFunction, MediumPtr, Sampler, Scene, Volume);

    /// Intersets a ray with the medium's bounding box
    virtual std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f &ray) const = 0;

    /// Returns the medium's majorant used for delta tracking
    virtual UnpolarizedSpectrum
    get_combined_extinction(const MediumInteraction3f &mi, Mask active = true,
                            bool global_only = false) const = 0;

    /**
     * Returns the medium's albedo, independently of other quantities.
     * May not be supported by all media.
     *
     * Becomes necessary when we need to evaluate the albedo at a
     * location where sigma_t = 0.
     */
    virtual UnpolarizedSpectrum get_albedo(const MediumInteraction3f &mi,
                                           Mask active = true) const = 0;

    /// Returns the medium's emission at the queried location.
    virtual UnpolarizedSpectrum get_emission(const MediumInteraction3f &mi,
                                             Mask active = true) const = 0;

    /// Returns the medium coefficients Sigma_s, Sigma_n and Sigma_t evaluated
    /// at a given MediumInteraction mi
    virtual std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum,
                       UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi,
                                Mask active = true) const = 0;

    /**
     * \brief Sample a free-flight distance in the medium.
     *
     * This function samples a (tentative) free-flight distance according to an
     * exponential transmittance. It is then up to the integrator to then decide
     * whether the MediumInteraction corresponds to a real or null scattering
     * event.
     *
     * \param ray      Ray, along which a distance should be sampled
     * \param sample   A uniformly distributed random sample
     * \param channel  The channel according to which we will sample the
     * free-flight distance. This argument is only used when rendering in RGB
     * modes.
     *
     * \return         This method returns a MediumInteraction.
     *                 The MediumInteraction will always be valid,
     *                 except if the ray missed the Medium's bounding box.
     */
    MediumInteraction3f sample_interaction(const Ray3f &ray, Float sample,
                                           UInt32 channel, Mask active) const;

    /**
     * Similar to \ref sample_interaction, but ensures that a real interaction
     * is sampled.
     */
    std::pair<MediumInteraction3f, Spectrum>
    sample_interaction_real(const Ray3f &ray, Sampler *sampler, UInt32 channel,
                            Mask active) const;

    /**
     * Sample an interaction with Differential Ratio Tracking.
     * Intended for adjoint integration.
     *
     * Returns the interaction record and a sampling weight.
     */
    std::pair<MediumInteraction3f, Spectrum>
    sample_interaction_drt(const Ray3f &ray, Sampler *sampler, UInt32 channel,
                           Mask active) const;

    /**
     * Sample an interaction with Differential Residual Ratio Tracking.
     * Intended for adjoint integration.
     *
     * Returns the interaction record and a sampling weight.
     */
    std::pair<MediumInteraction3f, Spectrum>
    sample_interaction_drrt(const Ray3f &ray, Sampler *sampler, UInt32 channel,
                            Mask active) const;

    /**
     * \brief Compute the transmittance and PDF
     *
     * This function evaluates the transmittance and PDF of sampling a certain
     * free-flight distance The returned PDF takes into account if a medium
     * interaction occured (mi.t <= si.t) or the ray left the medium (mi.t >
     * si.t)
     *
     * The evaluated PDF is spectrally varying. This allows to account for the
     * fact that the free-flight distance sampling distribution can depend on
     * the wavelength.
     *
     * \return   This method returns a pair of (Transmittance, PDF).
     *
     */
    std::pair<UnpolarizedSpectrum, UnpolarizedSpectrum>
    eval_tr_and_pdf(const MediumInteraction3f &mi,
                    const SurfaceInteraction3f &si, Mask active) const;

    /**
     * Compute the ray-medium overlap range and prepare a
     * medium interaction to be filled by a sampling routine.
     * Exposed as part of the API to enable testing.
     */
    std::tuple<MediumInteraction3f, Float, Float, Mask>
    prepare_interaction_sampling(const Ray3f &ray, Mask active) const;

    /**
     * Pre-computes quantities needed for a DDA traversal of the given grid.
     *
     * Returns (initial t, tmax, tdelta).
     */
    std::tuple<Float, Vector3f, Vector3f>
    prepare_dda_traversal(const Volume *majorant_grid, const Ray3f &ray,
                          Float mint, Float maxt, Mask active) const;

    /// Return the phase function of this medium
    MTS_INLINE const PhaseFunction *phase_function() const {
        return m_phase_function.get();
    }

    /// Returns a reference to the majorant supegrid, if any
    MTS_INLINE ref<Volume> majorant_grid() const {
        return m_majorant_grid;
    }

    /// Return true if a majorant supergrid is available.
    MTS_INLINE bool has_majorant_grid() const {
        return (bool) m_majorant_grid;
    }

    /// Return the size of a voxel in the majorant grid, if any
    MTS_INLINE Vector3f majorant_grid_voxel_size() const {
        if (m_majorant_grid)
            return m_majorant_grid->voxel_size();
        else
            return ek::zero<Vector3f>();
    }

    /// Returns whether this specific medium instance uses emitter sampling
    MTS_INLINE bool use_emitter_sampling() const { return m_sample_emitters; }

    /// Returns whether this medium is homogeneous
    MTS_INLINE bool is_homogeneous() const { return m_is_homogeneous; }

    /// Returns whether this medium has a spectrally varying extinction
    MTS_INLINE bool has_spectral_extinction() const {
        return m_has_spectral_extinction;
    }

    /// Return a string identifier
    std::string id() const override { return m_id; }

    /// Set a string identifier
    void set_id(const std::string& id) override { m_id = id; };

    /// Return a human-readable representation of the Medium
    std::string to_string() const override = 0;

    ENOKI_VCALL_REGISTER(Float, mitsuba::Medium)

    MTS_DECLARE_CLASS()
protected:
    Medium(const Properties &props);
    virtual ~Medium();

    static Float extract_channel(Spectrum value, UInt32 channel);

protected:
    ref<PhaseFunction> m_phase_function;
    bool m_sample_emitters, m_is_homogeneous, m_has_spectral_extinction;

    size_t m_majorant_resolution_factor;
    ref<Volume> m_majorant_grid;
    /* Factor to apply to the majorant, helps ensure that we are not using
     * a majorant that is exactly equal to the max density, which can be
     * problematic for Path Replay. */
    ScalarFloat m_majorant_factor;

    /// Identifier (if available)
    std::string m_id;
};

MTS_EXTERN_CLASS_RENDER(Medium)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for packets of Medium pointers
// -----------------------------------------------------------------------

ENOKI_VCALL_TEMPLATE_BEGIN(mitsuba::Medium)
    ENOKI_VCALL_GETTER(phase_function, const typename Class::PhaseFunction*)
    ENOKI_VCALL_GETTER(use_emitter_sampling, bool)
    ENOKI_VCALL_GETTER(is_homogeneous, bool)
    ENOKI_VCALL_GETTER(has_spectral_extinction, bool)
    ENOKI_VCALL_METHOD(get_combined_extinction)
    ENOKI_VCALL_METHOD(get_albedo)
    ENOKI_VCALL_METHOD(get_emission)
    ENOKI_VCALL_METHOD(get_scattering_coefficients)
    ENOKI_VCALL_METHOD(intersect_aabb)
    ENOKI_VCALL_METHOD(sample_interaction)
    ENOKI_VCALL_METHOD(sample_interaction_real)
    ENOKI_VCALL_METHOD(sample_interaction_drt)
    ENOKI_VCALL_METHOD(sample_interaction_drrt)
    ENOKI_VCALL_METHOD(eval_tr_and_pdf)
    ENOKI_VCALL_METHOD(prepare_interaction_sampling)
ENOKI_VCALL_TEMPLATE_END(mitsuba::Medium)

//! @}
// -----------------------------------------------------------------------
