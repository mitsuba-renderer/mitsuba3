#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/sampler.h>
#include <enoki/vcall.h>

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
    TransportMode mode = TransportMode::Radiance;

    /// Sampler object
    Sampler *sampler = nullptr;

    /*
     * Bit mask for requested phase function component types to be
     * sampled/evaluated.
     * The default value (equal to \ref PhaseFunctionFlags::All) enables all
     * components.
     */
    uint32_t type_mask = (uint32_t) 0x7u;

    /*
     * Integer value of requested phase function component index to be
     * sampled/evaluated.
     */
    uint32_t component = (uint32_t) -1;

    //! @}
    // =============================================================

    PhaseFunctionContext() = default;

    PhaseFunctionContext(Sampler *sampler,
                         TransportMode mode = TransportMode::Radiance)
        : mode(mode), sampler(sampler) { }

    PhaseFunctionContext(Sampler *sampler, TransportMode mode,
                         uint32_t type_mask, uint32_t component)
        : mode(mode), sampler(sampler), type_mask(type_mask),
          component(component) { }

    /**
     * \brief Reverse the direction of light transport in the record
     *
     * This updates the transport mode (radiance to importance and vice versa).
     */
    void reverse() { mode = (TransportMode)(1 - (int) mode); }

    /**
     * Checks whether a given phase function component type and phase function
     * component index are enabled in this context.
     */
    bool is_enabled(PhaseFunctionFlags type_, uint32_t component_ = 0) const {
        uint32_t type = (uint32_t) type_;
        return (type_mask == (uint32_t) -1 || (type_mask & type) == type) &&
               (component == (uint32_t) -1 || component == component_);
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
     * \param sample1
     *     A uniformly distributed sample on \f$[0,1]\f$. It is used
     *     to select the phase function component in multi-component models.
     *
     * \param sample2
     *     A uniformly distributed sample on \f$[0,1]^2\f$. It is
     *     used to generate the sampled direction.
     *
     * \return A sampled direction wo
     */
    virtual std::pair<Vector3f, Float> sample(const PhaseFunctionContext &ctx,
                                              const MediumInteraction3f &mi,
                                              Float sample1, const Point2f &sample2,
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

    /// Flags for this phase function.
    uint32_t flags(Mask /*active*/ = true) const { return m_flags; }

    /// Flags for a specific component of this phase function.
    uint32_t flags(size_t i, Mask /*active*/ = true) const {
        Assert(i < m_components.size());
        return m_components[i];
    }

    /// Number of components this phase function is comprised of.
    size_t component_count(Mask /*active*/ = true) const {
        return m_components.size();
    }

    /// Return a string identifier
    std::string id() const override { return m_id; }

    /// Set a string identifier
    void set_id(const std::string& id) override { m_id = id; };

    /// Return a human-readable representation of the phase function
    std::string to_string() const override = 0;

    //! @}
    // -----------------------------------------------------------------------

    ENOKI_VCALL_REGISTER(Float, mitsuba::PhaseFunction)

    MTS_DECLARE_CLASS()
protected:
    PhaseFunction(const Properties &props);
    virtual ~PhaseFunction();

protected:
    /// Type of phase function (e.g. anisotropic)
    uint32_t m_flags;

    /// Flags for each component of this phase function.
    std::vector<uint32_t> m_components;

    /// Identifier (if available)
    std::string m_id;
};

MTS_VARIANT
std::ostream &operator<<(std::ostream &os, const PhaseFunctionContext<Float, Spectrum>& ctx) {
    os << "PhaseFunctionContext[" << std::endl
       << "  mode = " << ctx.mode << "," << std::endl
       << "  sampler = " << ctx.sampler << "," << std::endl
       << "  component = ";
    if (ctx.component == (uint32_t) -1)
        os << "all";
    else
        os << ctx.component;
    os << std::endl << "]";
    return os;
}

//! @}
// -----------------------------------------------------------------------
MTS_EXTERN_CLASS_RENDER(PhaseFunction)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for vectorized function calls
// -----------------------------------------------------------------------

ENOKI_VCALL_TEMPLATE_BEGIN(mitsuba::PhaseFunction)
    ENOKI_VCALL_METHOD(sample)
    ENOKI_VCALL_METHOD(eval)
    ENOKI_VCALL_METHOD(projected_area)
    ENOKI_VCALL_METHOD(max_projected_area)
    ENOKI_VCALL_GETTER(flags, uint32_t)
    ENOKI_VCALL_GETTER(component_count, size_t)
ENOKI_VCALL_TEMPLATE_END(mitsuba::PhaseFunction)

//! @}
// -----------------------------------------------------------------------
