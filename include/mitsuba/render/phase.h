#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT
struct MTS_EXPORT_RENDER PhaseFunctionContext {
    MTS_IMPORT_TYPES(Sampler);

      // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Transported mode (radiance or importance)
    TransportMode mode;

    /// Sampler object
    Sampler *sampler;

    //! @}
    // =============================================================

    PhaseFunctionContext(Sampler *sampler, TransportMode mode = TransportMode::Radiance)
        : mode(mode), sampler(sampler) { }

    /**
     * \brief Reverse the direction of light transport in the record
     *
     * This updates the transport mode (radiance to importance and vice versa).
     */
    void reverse() {
        mode = (TransportMode) (1 - (int) mode);
    }

};

/**
 * \brief Abstract phase function base-class.
 *
 * This class provides an abstract interface to all Phase function plugins in
 * Mitsuba. It exposes functions for evaluating and sampling the model.
 */

MTS_VARIANT
class MTS_EXPORT_RENDER PhaseFunction : public Object {
public:
    MTS_IMPORT_TYPES(PhaseFunctionContext);

    /**
     * \brief Importance sample the phase function model
     *
     * The function returns a sampled direction.
     *
     * \param ctx
     *     A phase function sampling context, contains information
     *     about the transport mode
     *
     * \param mi
     *     A medium interaction data structure describing the underlying
     *     medium position. The incident direction is obtained from
     *     the field <tt>mi.wi</tt>.
     *
     * \param sample
     *     A uniformly distributed sample on \f$[0,1]^2\f$. It is
     *     used to generate the sampled direction.
     *
     * \return A sampled direction wo
     */
    virtual std::pair<Vector3f, Float> sample(const PhaseFunctionContext &ctx,
                                              const MediumInteraction3f &mi, const Point2f &sample,
                                              Mask active = true) const = 0;
    /**
     * \brief Evaluates the phase function model
     *
     * The function returns the value (which equals the PDF) of the phase
     * function in the query direction.
     *
     * \param ctx
     *     A phase function sampling context, contains information
     *     about the transport mode
     *
     * \param mi
     *     A medium interaction data structure describing the underlying
     *     medium position. The incident direction is obtained from
     *     the field <tt>mi.wi</tt>.
     *
     * \param wo
     *     An outgoing direction to evaluate.
     *
     * \return The value of the phase function in direction wo
     */
    virtual Float eval(const PhaseFunctionContext &ctx, const MediumInteraction3f &mi,
                       const Vector3f &wo, Mask active = true) const = 0;

    /// Return a string identifier
    std::string id() const override { return m_id; }

    /// Return a human-readable representation of the phase function
    std::string to_string() const override = 0;

    //! @}
    // -----------------------------------------------------------------------

    ENOKI_PINNED_OPERATOR_NEW(Float)
    MTS_DECLARE_CLASS()
protected:
    PhaseFunction();
    virtual ~PhaseFunction();

protected:
    /// Identifier (if available)
    std::string m_id;

};

//! @}
// -----------------------------------------------------------------------
MTS_EXTERN_CLASS_RENDER(PhaseFunction)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for vectorized function calls
// -----------------------------------------------------------------------

ENOKI_CALL_SUPPORT_TEMPLATE_BEGIN(mitsuba::PhaseFunction)
    ENOKI_CALL_SUPPORT_METHOD(sample)
    ENOKI_CALL_SUPPORT_METHOD(eval)
ENOKI_CALL_SUPPORT_TEMPLATE_END(mitsuba::PhaseFunction)

//! @}
// -----------------------------------------------------------------------
