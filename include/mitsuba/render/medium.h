#pragma once

#include <drjit/vcall.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/traits.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/volume.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Medium : public Object {
public:
    MI_IMPORT_TYPES(PhaseFunction, MediumPtr, Sampler, Scene, Texture, Volume);

    /// Intersects a ray with the medium's bounding box
    virtual std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f &ray) const = 0;

    /// Returns the medium's majorant used for delta tracking
    virtual UnpolarizedSpectrum
    get_majorant(const MediumInteraction3f &mi,
                 Mask active = true) const = 0;

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
     *
     * Warning: the returned medium interaction's quantities (sigma_t,
     * majorant, etc) will *not* be filled in. This is to allow the caller
     * to decide whether to perform attached or detached lookups for
     * these quantities.
     */
    std::pair<MediumInteraction3f, Spectrum>
    sample_interaction_drt(const Ray3f &ray, Sampler *sampler, UInt32 channel,
                           Mask active) const;

    /**
     * Sample an interaction with Differential Residual Ratio Tracking.
     * Intended for adjoint integration.
     *
     * Returns the interaction record and a sampling weight.
     *
     * Warning: the returned medium interaction's quantities (sigma_t,
     * majorant, etc) will *not* be filled in. This is to allow the caller
     * to decide whether to perform attached or detached lookups for
     * these quantities.
     */
    std::pair<MediumInteraction3f, Spectrum>
    sample_interaction_drrt(const Ray3f &ray, Sampler *sampler, UInt32 channel,
                            Mask active) const;

    /**
     * \brief Compute the transmittance and PDF
     *
     * This function evaluates the transmittance and PDF of sampling a certain
     * free-flight distance The returned PDF takes into account if a medium
     * interaction occurred (mi.t <= si.t) or the ray left the medium (mi.t >
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
    MI_INLINE const PhaseFunction *phase_function() const {
        return m_phase_function.get();
    }

    /**
     * Returns the current majorant supergrid resolution factor
     * w.r.t. the sigma_t grid resolution.
     */
    MI_INLINE size_t majorant_resolution_factor() const {
        return m_majorant_resolution_factor;
    }
    /**
     * Set the majorant supergrid resolution factor
     * w.r.t. the sigma_t grid resolution.
     * One should call \ref parameters_changed() to ensure
     * that the supergrid is regenerated.
     */
    MI_INLINE void set_majorant_resolution_factor(size_t factor) {
        m_majorant_resolution_factor = factor;
    }

    /// Returns a reference to the majorant supegrid, if any
    MI_INLINE ref<Volume> majorant_grid() const {
        return m_majorant_grid;
    }
    /// Return true if a majorant supergrid is available.
    MI_INLINE bool has_majorant_grid() const {
        return (bool) m_majorant_grid;
    }

    /// Return the size of a voxel in the majorant grid, if any
    MI_INLINE Vector3f majorant_grid_voxel_size() const {
        if (m_majorant_grid)
            return m_majorant_grid->voxel_size();
        else
            return dr::zeros<Vector3f>();
    }

    /// Returns whether this specific medium instance uses emitter sampling
    MI_INLINE bool use_emitter_sampling() const { return m_sample_emitters; }

    /// Returns whether this medium is homogeneous
    MI_INLINE bool is_homogeneous() const { return m_is_homogeneous; }

    /// Returns whether this medium has a spectrally varying extinction
    MI_INLINE bool has_spectral_extinction() const {
        return m_has_spectral_extinction;
    }

    /// Return a string identifier
    std::string id() const override { return m_id; }

    /// Set a string identifier
    void set_id(const std::string& id) override { m_id = id; };

    /// Return a human-readable representation of the Medium
    std::string to_string() const override = 0;

    DRJIT_VCALL_REGISTER(Float, mitsuba::Medium)

    MI_DECLARE_CLASS()
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

    /// Used by differential residual ratio tracking (DRRT)
    ScalarFloat m_control_density;

    /// Identifier (if available)
    std::string m_id;
};

MI_EXTERN_CLASS(Medium)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Dr.Jit support for packets of Medium pointers
// -----------------------------------------------------------------------

DRJIT_VCALL_TEMPLATE_BEGIN(mitsuba::Medium)
    DRJIT_VCALL_GETTER(phase_function, const typename Class::PhaseFunction*)
    DRJIT_VCALL_GETTER(use_emitter_sampling, bool)
    DRJIT_VCALL_GETTER(is_homogeneous, bool)
    DRJIT_VCALL_GETTER(has_spectral_extinction, bool)
    DRJIT_VCALL_METHOD(get_majorant)
    DRJIT_VCALL_METHOD(get_albedo)
    DRJIT_VCALL_METHOD(get_emission)
    DRJIT_VCALL_METHOD(get_scattering_coefficients)
    DRJIT_VCALL_METHOD(intersect_aabb)
    DRJIT_VCALL_METHOD(sample_interaction)
    DRJIT_VCALL_METHOD(sample_interaction_real)
    DRJIT_VCALL_METHOD(sample_interaction_drt)
    DRJIT_VCALL_METHOD(sample_interaction_drrt)
    DRJIT_VCALL_METHOD(eval_tr_and_pdf)
    DRJIT_VCALL_METHOD(prepare_interaction_sampling)
DRJIT_VCALL_TEMPLATE_END(mitsuba::Medium)

//! @}
// -----------------------------------------------------------------------
