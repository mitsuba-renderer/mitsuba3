#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/render/interaction.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Abstract phase function base-class.
 *
 * This class provides an abstract interface to all Phase function plugins in
 * Mitsuba. It exposes functions for evaluating and sampling the model.
 */
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER PhaseFunction : public Object {
public:
    MTS_IMPORT_TYPES();

    /**
     * \brief Importance sample the phase function model
     *
     * The function returns a sampled direction.
     *
     *
     * \param mi
     *     A medium interaction data structure describing the underlying
     *     medium position. The incident direction is obtained from
     *     the field <tt>mi.wi</tt>.
     *
     * \param sample1
     *     A uniformly distributed sample on \f$[0,1]^2\f$. It is
     *     used to generate the sampled direction.
     *
     * \return A sampled direction wo
     */
    virtual Vector3f sample(const MediumInteraction3f &mi, const Point2f &sample1,
                            Mask active = true) const = 0;
    /**
     * \brief Evaluates the phase function model
     *
     * The function returns the value (which equals the PDF) of the phase
     * function in the query direction.
     *
     * \param wo
     *      An outgoing direction to evaluate.
     *
     * \return The value of the phase function in direction wo
     */
    virtual Float eval(const Vector3f &wo, Mask active = true) const = 0;

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
