#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief This enumeration is used to classify phase functions into different types,
 * i.e. into isotropic, anisotropic and microflake phase functions.
 *
 * This can be used to optimize implementatons to for example have less overhead
 * if the phase function is not a microflake phase function.
 */
enum class PhaseFunctionFlags : uint32_t {
    None        = 0x00,
    Isotropic   = 0x01,
    Anisotropic = 0x02,
    Microflake  = 0x04
};

constexpr uint32_t operator|(PhaseFunctionFlags f1, PhaseFunctionFlags f2) {
    return (uint32_t) f1 | (uint32_t) f2;
}
constexpr uint32_t operator|(uint32_t f1, PhaseFunctionFlags f2) { return f1 | (uint32_t) f2; }
constexpr uint32_t operator&(PhaseFunctionFlags f1, PhaseFunctionFlags f2) {
    return (uint32_t) f1 & (uint32_t) f2;
}
constexpr uint32_t operator&(uint32_t f1, PhaseFunctionFlags f2) { return f1 & (uint32_t) f2; }
constexpr uint32_t operator~(PhaseFunctionFlags f1) { return ~(uint32_t) f1; }
constexpr uint32_t operator+(PhaseFunctionFlags e) { return (uint32_t) e; }
template <typename UInt32> constexpr auto has_flag(UInt32 flags, PhaseFunctionFlags f) {
    return (flags & (uint32_t) f) != 0;
}

/**
 * \brief Context data structure for phase function evaluation and sampling
 *
 * Phase function models in Mitsuba can be queried and sampled using a variety of
 * different modes. Using this data structure, a rendering algorithm can indicate whether
 * radiance or importance is being transported.
 *
 * The context further holds a pointer to a sampler object, in case
 * the evaluation or sampling functions need additional random numbers.
 *
 */
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
        : mode(mode), sampler(sampler) {}

    /**
     * \brief Reverse the direction of light transport in the record
     *
     * This updates the transport mode (radiance to importance and vice versa).
     */
    void reverse() { mode = (TransportMode)(1 - (int) mode); }
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

    /**
     * \brief Returns the microflake projected area
     *
     * The function returns the projected area of the microflake distribution defining the phase
     * function. For non-microflake phase functions, e.g. isotropic or Henyey-Greenstein, this
     * should return a value of 1.
     *
     * \param mi
     *     A medium interaction data structure describing the underlying
     *     medium position. The incident direction is obtained from
     *     the field <tt>mi.wi</tt>.
     *
     * \return The projected area in direction <tt>mi.wi</tt> at position <tt>mi.p</tt>
     */
    virtual Float projected_area(const MediumInteraction3f & /* mi */, Mask /* active */ = true) const {
        return 1.f;
    }

    /// Return the maximum projected area of the microflake distribution
    virtual Float max_projected_area() const { return 1.f; }

    /// Flags for this phase function
    uint32_t flags(Mask /*active*/) const { return m_flags; }

    /// Return a string identifier
    std::string id() const override { return m_id; }

    /// Return a human-readable representation of the phase function
    std::string to_string() const override = 0;

    //! @}
    // -----------------------------------------------------------------------

    ENOKI_CALL_SUPPORT_FRIEND()
    ENOKI_PINNED_OPERATOR_NEW(Float)
    MTS_DECLARE_CLASS()
protected:
    PhaseFunction(const Properties &props);
    virtual ~PhaseFunction();

protected:
    /// Type of phase function (e.g. anisotropic)
    uint32_t m_flags;

    /// Identifier (if available)
    std::string m_id;
};

MTS_VARIANT
std::ostream &operator<<(std::ostream &os, const PhaseFunctionContext<Float, Spectrum>& ctx) {
    os << "PhaseFunctionContext[" << std::endl
        << "  mode = " << ctx.mode << "," << std::endl
        << "  sampler = " << ctx.sampler << std::endl << "]";
    return os;
}

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
    ENOKI_CALL_SUPPORT_METHOD(projected_area)
    ENOKI_CALL_SUPPORT_METHOD(max_projected_area)
    ENOKI_CALL_SUPPORT_GETTER(flags, m_flags)
ENOKI_CALL_SUPPORT_TEMPLATE_END(mitsuba::PhaseFunction)

//! @}
// -----------------------------------------------------------------------
