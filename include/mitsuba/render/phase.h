#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/sampler.h>
#include <drjit/call.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief This enumeration is used to classify phase functions into different types,
 * i.e. into isotropic, anisotropic and microflake phase functions.
 *
 * This can be used to optimize implementations to for example have less overhead
 * if the phase function is not a microflake phase function.
 */
enum class PhaseFunctionFlags : uint32_t {
    Empty       = 0x00,
    Isotropic   = 0x01,
    Anisotropic = 0x02,
    Microflake  = 0x04
};

MI_DECLARE_ENUM_OPERATORS(PhaseFunctionFlags)

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
MI_VARIANT
struct MI_EXPORT_LIB PhaseFunctionContext {
    MI_IMPORT_TYPES(Sampler);

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

MI_VARIANT
class MI_EXPORT_LIB PhaseFunction : public Object {
public:
    MI_IMPORT_TYPES(PhaseFunctionContext);

    /// Destructor
    ~PhaseFunction();

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
     * \return A sampled direction wo and its corresponding weight and PDF
     */
    virtual std::tuple<Vector3f, Spectrum, Float> sample(const PhaseFunctionContext &ctx,
                                                         const MediumInteraction3f &mi,
                                                         Float sample1, const Point2f &sample2,
                                                         Mask active = true) const = 0;
    /**
     * \brief Evaluates the phase function model value and PDF
     *
     * The function returns the value (which often equals the PDF) of the phase
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
     * \return The value and the sampling PDF of the phase function in direction wo
     */
    virtual std::pair<Spectrum, Float> eval_pdf(const PhaseFunctionContext &ctx,
                                                const MediumInteraction3f &mi,
                                                const Vector3f &wo,
                                                Mask active = true) const = 0;

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

    /// Return type of phase function
    uint32_t get_flags() const { return m_flags; }

    /// Set type of phase function
    void set_flags(uint32_t flags) { m_flags = flags; }

    //! @}
    // -----------------------------------------------------------------------

    MI_DECLARE_CLASS()

protected:
    PhaseFunction(const Properties &props);

protected:
    /// Type of phase function (e.g. anisotropic)
    uint32_t m_flags;

    /// Flags for each component of this phase function.
    std::vector<uint32_t> m_components;

    /// Identifier (if available)
    std::string m_id;
};

MI_VARIANT
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
MI_EXTERN_CLASS(PhaseFunction)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Dr.Jit support for vectorized function calls
// -----------------------------------------------------------------------

DRJIT_CALL_TEMPLATE_BEGIN(mitsuba::PhaseFunction)
    DRJIT_CALL_METHOD(sample)
    DRJIT_CALL_METHOD(eval_pdf)
    DRJIT_CALL_METHOD(projected_area)
    DRJIT_CALL_METHOD(max_projected_area)
    DRJIT_CALL_GETTER(flags)
    DRJIT_CALL_GETTER(component_count)
DRJIT_CALL_END(mitsuba::PhaseFunction)

//! @}
// -----------------------------------------------------------------------
