#pragma once

#include <mitsuba/render/interaction.h>
#include <mitsuba/render/common.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Structure holding the result of BSDF sampling operations.
 */
template <typename Point3_> struct BSDFSample {

    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Point3   = Point3_;

    using Value    = value_t<Point3>;
    using Index    = uint32_array_t<Value>;
    using Mask     = mask_t<Value>;
    using Spectrum = mitsuba::Spectrum<Value>;

    using Point2   = point2_t<Point3>;
    using Vector2  = vector2_t<Point3>;
    using Vector3  = vector3_t<Point3>;
    using Normal3  = normal3_t<Point3>;


    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Normalized incident direction in local coordinates
    Vector3 wi;

    /// Normalized outgoing direction in local coordinates
    Vector3 wo;

    /// Probability density at the sample
    Value pdf;

    /// Relative index of refraction in the sampled direction
    Value eta;

    /// Stores the component type that was sampled by \ref BSDF::sample()
    Index sampled_type = 0;

    /// Stores the component index that was sampled by \ref BSDF::sample()
    Index sampled_component = Index(-1);

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Methods
    // =============================================================

    /**
     * \brief Given a surface interaction an an incident/exitant direction
     * pair (wi, wo), create a query record to evaluate the BSDF or its
     * sampling density.
     *
     * By default, all components will be sampled irregardless of
     * what measure they live on. For convenience, this function
     * uses the local incident direction vector contained in the
     * supplied intersection record.
     *
     * For convenience, this function uses the local incident direction
     * vector contained in the supplied intersection record.
     *
     * \param wo
     *      An outgoing direction in local coordinates. This should
     *      be a normalized direction vector that points \a away from
     *      the scattering event.
     */
    BSDFSample(const Vector3 &wo)
        : wi(0.0f), wo(wo), pdf(0.0f), eta(1.0f), sampled_type(0),
          sampled_component(-1) { }

    /**
     * \brief Given a surface interaction and an incident/exitant direction
     * pair (wi, wo), create a query record to evaluate the BSDF or its
     * sampling density.
     *
     * \param wi
     *      An incident direction in local coordinates. This should
     *      be a normalized direction vector that points \a away from
     *      the scattering event.
     *
     * \param wo
     *      An outgoing direction in local coordinates. This should
     *      be a normalized direction vector that points \a away from
     *      the scattering event.
     */
    BSDFSample(const Vector3 &wi, const Vector3 &wo)
        : wi(wi), wo(wo), pdf(0.0f), eta(1.0f), sampled_type(0),
          sampled_component(-1) { }

    //! @}
    // =============================================================

    ENOKI_STRUCT(BSDFSample, wi, wo, pdf, eta, sampled_type,
                 sampled_component)
    ENOKI_ALIGNED_OPERATOR_NEW()
};


/**
 * \brief Abstract BSDF base-class.
 *
 * This class provides an abstract interface to all %BSDF plugins in Mitsuba.
 * It exposes functions for evaluating and sampling the model. Smooth
 * two-dimensional density functions, as well as degenerate one-dimensional and
 * discrete densities are all handled within the same framework.
 *
 * For improved flexibility with respect to the various rendering algorithms,
 * this class can sample and evaluate a complete BSDF, but it also allows to
 * pick and choose individual components of multi-lobed BSDFs based on their
 * properties and component indices. This selection is specified using a
 * context data structure that is provided along with every operation.
 *
 * \sa BSDFContext
 * \sa BSDFSample
 */
class MTS_EXPORT_RENDER BSDF : public Object {
public:
    /**
     * \brief This list of flags is used to classify the different
     * types of lobes that are implemented in a BSDF instance.
     *
     * They are also useful for picking out individual components
     * by setting combinations in \ref BSDFContext::type_mask.
     */
    enum EFlags : uint32_t {
        // =============================================================
        //                      BSDF lobe types
        // =============================================================

        /// 'null' scattering event, i.e. particles do not undergo deflection
        ENull                 = 0x00001,

        /// Ideally diffuse reflection
        EDiffuseReflection    = 0x00002,

        /// Ideally diffuse transmission
        EDiffuseTransmission  = 0x00004,

        /// Glossy reflection
        EGlossyReflection     = 0x00008,

        /// Glossy transmission
        EGlossyTransmission   = 0x00010,

        /// Reflection into a discrete set of directions
        EDeltaReflection      = 0x00020,

        /// Transmission into a discrete set of directions
        EDeltaTransmission    = 0x00040,

        /// Reflection into a 1D space of directions
        EDelta1DReflection    = 0x00080,

        /// Transmission into a 1D space of directions
        EDelta1DTransmission  = 0x00100,

        // =============================================================
        //!                   Other lobe attributes
        // =============================================================

        /// The lobe is not invariant to rotation around the normal
        EAnisotropic          = 0x01000,

        /// The BSDF depends on the UV coordinates
        ESpatiallyVarying     = 0x02000,

        /// Flags non-symmetry (e.g. transmission in dielectric materials)
        ENonSymmetric         = 0x04000,

        /// Supports interactions on the front-facing side
        EFrontSide            = 0x08000,

        /// Supports interactions on the back-facing side
        EBackSide             = 0x10000,
    };

    /// Convenient combinations of flags from \ref EBSDFType
    enum EFlagCombinations : uint32_t {
        /// Any reflection component (scattering into discrete, 1D, or 2D set of directions)
        EReflection   = EDiffuseReflection | EDeltaReflection |
                        EDelta1DReflection | EGlossyReflection,

        /// Any transmission component (scattering into discrete, 1D, or 2D set of directions)
        ETransmission = EDiffuseTransmission | EDeltaTransmission |
                        EDelta1DTransmission | EGlossyTransmission | ENull,

        /// Diffuse scattering into a 2D set of directions
        EDiffuse      = EDiffuseReflection | EDiffuseTransmission,

        /// Non-diffuse scattering into a 2D set of directions
        EGlossy       = EGlossyReflection | EGlossyTransmission,

        /// Scattering into a 2D set of directions
        ESmooth       = EDiffuse | EGlossy,

        /// Scattering into a discrete set of directions
        EDelta        = ENull | EDeltaReflection | EDeltaTransmission,

        /// Scattering into a 1D space of directions
        EDelta1D      = EDelta1DReflection | EDelta1DTransmission,

        /// Any kind of scattering
        EAll          = EDiffuse | EGlossy | EDelta | EDelta1D
    };

    /**
     * \brief Sample the BSDF and return the probability density \a and the
     * importance weight of the sample (i.e. the value of the BSDF divided
     * by the probability density)
     *
     * If a component mask or a specific component index is specified, the
     * sample is drawn from the matching component, if it exists. Depending
     * on the provided transport type, either the BSDF or its adjoint version
     * is used.
     *
     * When sampling a continuous/non-delta component, this method also
     * multiplies by the cosine foreshorening factor with respect to the
     * sampled direction.
     *
     * \param ctx     A BSDF query record
     * \param sample  A uniformly distributed sample on \f$[0,1]^2\f$
     * \param pdf     Will record the probability with respect to solid angles
     *                (or the discrete probability when a delta component is sampled)
     *
     * \return (bs, value)
     *     bs:    Sampling record, indicating the sampled direction, PDF values
     *            and other information. Lanes for which sampling failed have
     *            undefined values.
     *     value: The BSDF value (multiplied by the cosine foreshortening
     *            factor when a non-delta component is sampled). A zero spectrum
     *            means that sampling failed.
     *
     * \remark From Python, this function is is called using the syntax
     *         <tt>value, pdf = bsdf.sample(bs, sample)</tt>
     */
    virtual std::pair<BSDFSample3f, Spectrumf> sample(
            const SurfaceInteraction3f &si, const BSDFContext &ctx,
            Float sample1, const Point2f &sample2) const = 0;

    virtual std::pair<BSDFSample3fP, SpectrumfP> sample(
            const SurfaceInteraction3fP &si, const BSDFContext &ctx,
            FloatP sample1, const Point2fP &sample2, MaskP active = true) const = 0;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref sample()
    std::pair<BSDFSample3f, Spectrumf> sample(
            const SurfaceInteraction3f &si, const BSDFContext &ctx,
            Float sample1, const Point2f &sample2, bool /*unused*/) const {
        return sample(si, ctx, sample1, sample2);
    }


    /**
     * \brief Evaluate the BSDF f(wi, wo) or its adjoint version f^{*}(wi, wo)
     * and multiply by cosine foreshortening term.
     *
     * Based on the information in the supplied query context \c ctx, this
     * method will either evaluate the entire BSDF or query individual
     * components (e.g. the diffuse lobe). Only smooth (i.e. non Dirac-delta)
     * components are supported: calling \c eval() on a perfectly specular
     * material will return zero.
     *
     * Only the outgoing direction needs to be specified. The incident
     * direction is obtained from the surface interaction referenced by
     * the query context (<tt>ctx.si.wi</tt>).
     *
     * \param ctx
     *     A record with detailed information on the BSDF query, including a
     *     reference to a valid \ref SurfaceInteraction.
     *
     * \param wo
     *     The outgoing direction
     */
    virtual Spectrumf eval(const SurfaceInteraction3f &si, const BSDFContext &ctx,
                           const Vector3f &wo) const = 0;

    virtual SpectrumfP eval(const SurfaceInteraction3fP &si, const BSDFContext &ctx,
                            const Vector3fP &wo, MaskP active = true) const = 0;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref eval()
    Spectrumf eval(const SurfaceInteraction3f &si, const BSDFContext &ctx,
                   const Vector3f &wo, bool /*unused*/) const {
        return eval(si, ctx, wo);
    }

    /**
     * \brief Compute the probability of sampling \c wo (given
     * \c ctx.si.wi).
     *
     * This method provides access to the probability density that
     * would result when supplying the same BSDF context to the
     * \ref sample() method. It correctly handles changes in probability
     * when only a subset of the components is chosen for sampling
     * (this can be done using the \ref BSDFSample::component and
     * \ref BSDFSample::type_mask fields).
     *
     * Only the outgoing direction needs to be specified. The incident
     * direction is obtained from the surface interaction referenced by
     * the query context (<tt>ctx.si.wi</tt>).
     *
     * \param ctx
     *     A record with detailed information on the BSDF query, including a
     *     reference to a valid \ref SurfaceInteraction.
     *
     * \param wo
     *     The outgoing direction.
     */
    virtual Float pdf(const SurfaceInteraction3f &si, const BSDFContext &ctx,
                      const Vector3f &wo) const = 0;

    virtual FloatP pdf(const SurfaceInteraction3fP &si, const BSDFContext &ctx,
                       const Vector3fP &wo, MaskP active = true) const = 0;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref pdf()
    Float pdf(const SurfaceInteraction3f &si, const BSDFContext &ctx,
              const Vector3f &wo, bool /*unused*/) const {
        return pdf(si, ctx, wo);
    }


    // -----------------------------------------------------------------------
    //! @{ \name BSDF property accessors (components, flags, etc)
    // -----------------------------------------------------------------------

    /// Number of components this BSDF is comprised of.
    size_t component_count() const { return m_components.size(); }

    bool needs_differentials() const {
        return m_needs_differentials;
    }

    /// Flags for all components combined.
    uint32_t flags() const { return m_flags; }
    /// Flags for a specific component of this BSDF.
    uint32_t flags(size_t i) const { return m_components[i]; }

    std::string to_string() const override = 0;

    //! @}
    // -----------------------------------------------------------------------

    MTS_DECLARE_CLASS()

protected:
    virtual ~BSDF();

protected:
    bool m_needs_differentials = false;
    /// Combined flags for all components of this BSDF.
    uint32_t m_flags;
    /// Flags for each component of this BSDF.
    std::vector<uint32_t> m_components;
};


/**
 * \brief Record specifying a BSDF query and its context.
 * Note that since none of the fields are be wide, it is not an Enoki struct.
 */
struct MTS_EXPORT_RENDER BSDFContext {
    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Transported mode (radiance or importance)
    ETransportMode mode;

    /// Bit mask for requested BSDF component types to be sampled/evaluated
    uint32_t type_mask = BSDF::EAll;

    /**
     * Integer value of requested BSDF component index to be sampled/evaluated.
     * The default value of `(uint32_t) -1` enables all components.
     */
    uint32_t component = (uint32_t) -1;

    //! @}
    // =============================================================

    BSDFContext(ETransportMode mode = ERadiance)
        : mode(mode) { }

    BSDFContext(ETransportMode mode, uint32_t type_mask, uint32_t component)
        : mode(mode), type_mask(type_mask), component(component) { }

    BSDFContext(const BSDFContext &ctx)
        : mode(ctx.mode), type_mask(ctx.type_mask), component(ctx.component) { }

    /**
     * \brief Reverse the direction of light transport in the record
     *
     * This updates the transport mode (radiance to importance and vice versa).
     */
    void reverse() {
        mode = (ETransportMode) (1 - (int) mode);
    }

    /**
     * Checks whether a given BSDF component type and BSDF component index are
     * enabled in this context.
     */
    bool is_enabled(uint32_t type_, uint32_t component_ = 0) const {
        return (type_mask == (uint32_t) -1 || (type_mask & type_))
            && (component == (uint32_t) -1 || component == component_);
    }
};


namespace {
    template <typename Index>
    std::string type_mask_to_string(Index type_mask) {
        std::ostringstream oss;
        oss << "{ ";

#define is_set(mask) (type_mask & mask) == mask
        if (is_set(BSDF::EAll)) { oss << "all "; type_mask &= ~BSDF::EAll; }
        if (is_set(BSDF::EReflection)) { oss << "reflection "; type_mask &= ~BSDF::EReflection; }
        if (is_set(BSDF::ETransmission)) { oss << "transmission "; type_mask &= ~BSDF::ETransmission; }
        if (is_set(BSDF::ESmooth)) { oss << "smooth "; type_mask &= ~BSDF::ESmooth; }
        if (is_set(BSDF::EDiffuse)) { oss << "diffuse "; type_mask &= ~BSDF::EDiffuse; }
        if (is_set(BSDF::EGlossy)) { oss << "glossy "; type_mask &= ~BSDF::EGlossy; }
        if (is_set(BSDF::EDelta)) { oss << "delta"; type_mask &= ~BSDF::EDelta; }
        if (is_set(BSDF::EDelta1D)) { oss << "delta_1d "; type_mask &= ~BSDF::EDelta1D; }
        if (is_set(BSDF::EDiffuseReflection)) { oss << "diffuse_reflection "; type_mask &= ~BSDF::EDiffuseReflection; }
        if (is_set(BSDF::EDiffuseTransmission)) { oss << "diffuse_transmission "; type_mask &= ~BSDF::EDiffuseTransmission; }
        if (is_set(BSDF::EGlossyReflection)) { oss << "glossy_reflection "; type_mask &= ~BSDF::EGlossyReflection; }
        if (is_set(BSDF::EGlossyTransmission)) { oss << "glossy_transmission "; type_mask &= ~BSDF::EGlossyTransmission; }
        if (is_set(BSDF::EDeltaReflection)) { oss << "delta_reflection "; type_mask &= ~BSDF::EDeltaReflection; }
        if (is_set(BSDF::EDeltaTransmission)) { oss << "delta_transmission "; type_mask &= ~BSDF::EDeltaTransmission; }
        if (is_set(BSDF::EDelta1DReflection)) { oss << "delta_1d_reflection "; type_mask &= ~BSDF::EDelta1DReflection; }
        if (is_set(BSDF::EDelta1DTransmission)) { oss << "delta_1d_transmission "; type_mask &= ~BSDF::EDelta1DTransmission; }
        if (is_set(BSDF::ENull)) { oss << "null "; type_mask &= ~BSDF::ENull; }
        if (is_set(BSDF::EAnisotropic)) { oss << "anisotropic "; type_mask &= ~BSDF::EAnisotropic; }
        if (is_set(BSDF::EFrontSide)) { oss << "front_side "; type_mask &= ~BSDF::EFrontSide; }
        if (is_set(BSDF::EBackSide)) { oss << "back_side "; type_mask &= ~BSDF::EBackSide; }
        if (is_set(BSDF::ESpatiallyVarying)) { oss << "spatially_varying"; type_mask &= ~BSDF::ESpatiallyVarying; }
        if (is_set(BSDF::ENonSymmetric)) { oss << "non_symmetric"; type_mask &= ~BSDF::ENonSymmetric; }
#undef is_set

        Assert(type_mask == 0);
        oss << "}";
        return oss.str();
    }
}

// -----------------------------------------------------------------------
//! @{ \name Misc implementations
// -----------------------------------------------------------------------

extern MTS_EXPORT_RENDER std::ostream &operator<<(std::ostream &os,
                                                  const BSDFContext& ctx);

template <typename Point3>
std::ostream &operator<<(std::ostream &os, const BSDFSample<Point3>& bs) {
    os << "BSDFSample[" << std::endl
        << "  wi = " << bs.wi << "," << std::endl
        << "  wo = " << bs.wo << "," << std::endl
        << "  pdf = " << bs.pdf << "," << std::endl
        << "  eta = " << bs.eta << "," << std::endl
        << "  sampled_type = " << type_mask_to_string(bs.sampled_type) << "," << std::endl
        << "  sampled_component = " << bs.sampled_component << std::endl
        << "]";
    return os;
}

template <typename Point3>
typename SurfaceInteraction<Point3>::BSDFPtr
SurfaceInteraction<Point3>::bsdf(const RayDifferential3 &ray) {
    const BSDFPtr bsdf = shape->bsdf();

    if (!has_uv_partials && any(bsdf->needs_differentials()))
        compute_partials(ray);

    return bsdf;
}

//! @}
// -----------------------------------------------------------------------
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for dynamic vectorization
// -----------------------------------------------------------------------

ENOKI_STRUCT_DYNAMIC(mitsuba::BSDFSample, wi, wo, pdf, eta,
                     sampled_type, sampled_component)

//! @}
// -----------------------------------------------------------------------


// -----------------------------------------------------------------------
//! @{ \name Enoki support for packets of BSDF pointers
// -----------------------------------------------------------------------

ENOKI_CALL_SUPPORT_BEGIN(mitsuba::BSDFP)
ENOKI_CALL_SUPPORT(sample)
ENOKI_CALL_SUPPORT(eval)
ENOKI_CALL_SUPPORT(pdf)
ENOKI_CALL_SUPPORT_SCALAR(needs_differentials)
ENOKI_CALL_SUPPORT_SCALAR(flags)
ENOKI_CALL_SUPPORT_END(mitsuba::BSDFP)

//! @}
// -----------------------------------------------------------------------

/*
 * \brief This macro should be used in the definition of BSDF
 * plugins to instantiate concrete versions of the the \c sample,
 * \c eval and \c pdf functions.
 */
#define MTS_IMPLEMENT_BSDF()                                                   \
    std::pair<BSDFSample3f, Spectrumf> sample(                                 \
            const SurfaceInteraction3f &si, const BSDFContext &ctx,            \
            Float sample1, const Point2f &sample2) const override {            \
        return sample_impl(si, ctx, sample1, sample2, true);                   \
    }                                                                          \
    std::pair<BSDFSample3fP, SpectrumfP> sample(                               \
            const SurfaceInteraction3fP &si, const BSDFContext &ctx,           \
            FloatP sample1, const Point2fP &sample2,                           \
            MaskP active = true) const override {                              \
        return sample_impl(si, ctx, sample1, sample2, active);                 \
    }                                                                          \
    Spectrumf eval(const SurfaceInteraction3f &si, const BSDFContext &ctx,     \
                   const Vector3f &wo) const override {                        \
        return eval_impl(si, ctx, wo, true);                                   \
    }                                                                          \
    SpectrumfP eval(const SurfaceInteraction3fP &si, const BSDFContext &ctx,   \
                    const Vector3fP &wo, MaskP active) const override {        \
        return eval_impl(si, ctx, wo, active);                                 \
    }                                                                          \
    Float pdf(const SurfaceInteraction3f &si, const BSDFContext &ctx,          \
              const Vector3f &wo) const override {                             \
        return pdf_impl(si, ctx, wo, true);                                    \
    }                                                                          \
    FloatP pdf(const SurfaceInteraction3fP &si, const BSDFContext &ctx,        \
               const Vector3fP &wo, MaskP active) const override {             \
        return pdf_impl(si, ctx, wo, active);                                  \
    }
