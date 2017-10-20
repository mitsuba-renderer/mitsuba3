#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/simd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/common.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)


/**
* \brief This macro must be used in the definition of bsdfs
* plugins for template instantiations of the sample, eval and
* pdf functions.
*/
#define MTS_IMPLEMENT_BSDF()                                    \
    std::pair<Spectrumf, Float> sample(BSDFSample3f &bs,        \
        const Point2f &sample) const override {                 \
        return sample_impl(bs, sample, true);                   \
    }                                                           \
    std::pair<SpectrumfP, FloatP> sample(BSDFSample3fP &bs,     \
        const Point2fP &sample,                                 \
        const mask_t<FloatP> &active = true) const override {   \
        return sample_impl(bs, sample, active);                 \
    }                                                           \
    Spectrumf eval(const BSDFSample3f &bs,                      \
        EMeasure measure) const override {                      \
        return eval_impl(bs, measure, true);                    \
    }                                                           \
    SpectrumfP eval(const BSDFSample3fP &bs,                    \
        EMeasure measure,                                       \
        const mask_t<FloatP> &active) const override {          \
        return eval_impl(bs, measure, active);                  \
    }                                                           \
    Float pdf(const BSDFSample3f &bs,                           \
        EMeasure measure) const override {                      \
        return pdf_impl(bs, measure, true);                     \
    }                                                           \
    FloatP pdf(const BSDFSample3fP &bs,                         \
        EMeasure measure,                                       \
        const mask_t<FloatP> &active) const override {          \
        return pdf_impl(bs, measure, active);                   \
    }                                                           \




/**
 * \brief Abstract %BSDF base-class.
 *
 * This class implements an abstract interface to all BSDF plugins in Mitsuba.
 * It exposes functions for evaluating and sampling the model, and it allows
 * querying the probability density of the sampling method. Smooth
 * two-dimensional density functions, as well as degenerate one-dimensional
 * and discrete densities are all handled within the same framework.
 *
 * For improved flexibility with respect to the various rendering algorithms,
 * this class can sample and evaluate a complete BSDF, but it also allows to
 * pick and choose individual components of multi-lobed BSDFs based on their
 * properties and component indices. This selection is specified using a
 * special record that is provided along with every query.
 *
 * \ref BSDFSample
 */
class MTS_EXPORT_RENDER BSDF : public Object {
public:
    /**
     * \brief This list of flags is used to classify the different
     * types of lobes that are implemented in a BSDF instance.
     *
     * They are also useful for picking out individual components
     * by setting combinations in \ref BSDFSample::type_mask.
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

        /// Uses extra random numbers from the supplied sampler instance
        EUsesSampler          = 0x20000
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
    * \param bs    A BSDF query record
    * \param sample  A uniformly distributed sample on \f$[0,1]^2\f$
    * \param pdf     Will record the probability with respect to solid angles
    *                (or the discrete probability when a delta component is sampled)
    *
    * \return The BSDF value (multiplied by the cosine foreshortening
    *         factor when a non-delta component is sampled). A zero spectrum
    *         means that sampling failed.
    *
    * \remark From Python, this function is is called using the syntax
    *         <tt>value, pdf = bsdf.sample(bs, sample)</tt>
    */
    virtual std::pair<Spectrumf, Float> sample(BSDFSample3f &bs,
                                               const Point2f &sample) const = 0;
    std::pair<Spectrumf, Float> sample(BSDFSample3f &bs,
                                       const Point2f &s,
                                       bool /*unused*/) const {
        return sample(bs, s);
    }
    virtual std::pair<SpectrumfP, FloatP> sample(BSDFSample3fP &bs,
                                                 const Point2fP &sample,
                                                 const mask_t<FloatP> &active = true) const = 0;

    /**
    * \brief Evaluate the BSDF f(wi, wo) or its adjoint version f^{*}(wi, wo)
    *
    * This method allows to query the BSDF as a whole or pick out
    * individual components. When querying a smooth (i.e. non-degenerate)
    * component, it already multiplies the result by the cosine
    * foreshortening factor with respect to the outgoing direction.
    *
    * \param bs
    *     A record with detailed information on the BSDF query
    *
    * \param measure
    *     Specifies the measure of the component. This is necessary
    *     to handle BSDFs, whose components live on spaces with
    *     different measures. (E.g. a diffuse material with an
    *     ideally smooth dielectric coating).
    */
    virtual Spectrumf eval(const BSDFSample3f &bs,
                           EMeasure measure = ESolidAngle) const = 0;
    Spectrumf eval(const BSDFSample3f &bs,
                   EMeasure measure,
                   bool /*unused*/) const {
        return eval(bs, measure);
    }
    virtual SpectrumfP eval(const BSDFSample3fP &bs,
                            EMeasure measure = ESolidAngle,
                            const mask_t<FloatP> &active = true) const = 0;

    /**
    * \brief Compute the probability of sampling \c bs.wo (given
    * \c bs.wi).
    *
    * This method provides access to the probability density that
    * would result when supplying the same BSDF query record to the
    * \ref sample() method. It correctly handles changes in probability
    * when only a subset of the components is chosen for sampling
    * (this can be done using the \ref BSDFSample::component and
    * \ref BSDFSample::type_mask fields).
    *
    * \param bs
    *     A record with detailed information on the BSDF query
    *
    * \param measure
    *     Specifies the measure of the component. This is necessary
    *     to handle BSDFs, whose components live on spaces with
    *     different measures. (E.g. a diffuse material with an
    *     ideally smooth dielectric coating).
    */
    virtual Float pdf(const BSDFSample3f &bs,
                      EMeasure measure = ESolidAngle) const = 0;
    Float pdf(const BSDFSample3f &bs,
              EMeasure measure,
              bool /*unused*/) const {
        return pdf(bs, measure);
    }
    virtual FloatP pdf(const BSDFSample3fP &bs,
                       EMeasure measure = ESolidAngle,
                       const mask_t<FloatP> &active = true) const = 0;

    bool needs_differentials() const { return m_needs_differentials; }

    uint32_t flags() const { return m_flags; }

    std::string to_string() const override = 0;

    MTS_DECLARE_CLASS()

protected:
    virtual ~BSDF();

protected:
    bool m_needs_differentials = false;
    uint32_t m_flags;
};


/** \brief Container for all information
* that is required to sample or query a BSDF.
*/
template <typename Point3_> struct BSDFSample {

    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Point3 = Point3_;

    using Value = value_t<Point3>;
    using Index = uint32_array_t<Value>;
    using Mask = mask_t<Value>;

    using Point2 = point2_t<Point3>;
    using Vector2 = vector2_t<Point3>;
    using Vector3 = vector3_t<Point3>;
    using Normal3 = normal3_t<Point3>;

    using SurfaceInteraction = SurfaceInteraction<Point3>;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Reference to the underlying surface interaction
    const SurfaceInteraction &its;

    /// Pointer to a \ref Sampler instance (optional)
    Sampler *sampler = nullptr;

    /// Normalized incident direction in local coordinates
    Vector3 wi;

    /// Normalized outgoing direction in local coordinates
    Vector3 wo;

    /// Relative index of refraction in the sampled direction
    Value eta;

    /// Transported mode (radiance or importance)
    ETransportMode mode;

    /// Bit mask for requested BSDF component types to be sampled/evaluated
    uint32_t type_mask;

    /// Integer value of requested BSDF component index to be sampled/evaluated
    uint32_t component;

    /// Stores the component type that was sampled by \ref BSDF::sample()
    Index sampled_type;

    /// Stores the component index that was sampled by \ref BSDF::sample()
    Index sampled_component;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Methods
    // =============================================================

    /**
    * \brief Given a surface interaction and an incident direction,
    * construct a query record which can be used to sample an outgoing
    * direction.
    *
    * By default, all components will be sampled irregardless of
    * what measure they live on. For convenience, this function
    * uses the local incident direction vector contained in the
    * supplied intersection record.
    *
    * \param its
    *      An reference to the underlying intersection record
    *
    * \param sampler
    *      A source of (pseudo-) random numbers. Note that this sampler
    *      is only used when the scattering model for some reason needs
    *      more than the two unformly distributed numbers supplied in
    *      the \ref BSDF::sample() methods.
    *
    * \param mode
    *      The transported mode (\ref ERadiance or \ref EImportance)
    */
    inline BSDFSample(
        const SurfaceInteraction &its,
        Sampler *sampler,
        ETransportMode mode = ERadiance)
        : its(its), sampler(sampler), wi(its.wi), mode(mode),
        type_mask(BSDF::EAll), component(-1), sampled_type(0), sampled_component(-1) {
    }

    /**
    * \brief Given a surface interaction an an incident/exitant direction
    * pair (wi, wo), create a query record to evaluate the BSDF or its
    * sampling density.
    *
    * For convenience, this function uses the local incident direction
    * vector contained in the supplied intersection record.
    *
    * \param its
    *      A reference to the underlying intersection record
    * \param wo
    *      An outgoing direction in local coordinates. This should
    *      be a normalized direction vector that points \a away from
    *      the scattering event.
    * \param mode
    *      The transported mode (\ref ERadiance or \ref EImportance)
    */
    inline BSDFSample(
        const SurfaceInteraction &its,
        const Vector3 &wo,
        ETransportMode mode = ERadiance)
        : its(its), sampler(nullptr), wi(its.wi), wo(wo), mode(mode),
        type_mask(BSDF::EAll), component(-1), sampled_type(0), sampled_component(-1) {
    }

    /**
    * \brief Given a surface interaction and an incident/exitant direction
    * pair (wi, wo), create a query record to evaluate the BSDF or its
    * sampling density.
    *
    * \param its
    *      An reference to the underlying intersection record
    * \param wi
    *      An incident direction in local coordinates. This should
    *      be a normalized direction vector that points \a away from
    *      the scattering event.
    * \param wo
    *      An outgoing direction in local coordinates. This should
    *      be a normalized direction vector that points \a away from
    *      the scattering event.
    * \param mode
    *      The transported mode (\ref ERadiance or \ref EImportance)
    *
    */
    inline BSDFSample(
        const SurfaceInteraction &its,
        const Vector3 &wi,
        const Vector3 &wo,
        ETransportMode mode = ERadiance)
        : its(its), sampler(nullptr), wi(wi), wo(wo), mode(mode),
        type_mask(BSDF::EAll), component(-1), sampled_type(0), sampled_component(-1) {
    }

    /**
    * \brief Reverse the direction of light transport in the record
    *
    * This function essentially swaps \c wi and \c wo and adjusts
    * \c mode appropriately, so that non-symmetric scattering
    * models can be queried in the reverse direction.
    */
    void reverse() {
        std::swap(wo, wi);
        mode = (ETransportMode)(1 - mode);
    }

    //! @}
    // =============================================================

    ENOKI_STRUCT(BSDFSample, its, sampler, wi, wo, eta, mode,
                 type_mask, component, sampled_type, sampled_component)

    ENOKI_ALIGNED_OPERATOR_NEW()
};

namespace {
    template<typename Index>
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
        if (is_set(BSDF::EDelta1D)) { oss << "delta1D "; type_mask &= ~BSDF::EDelta1D; }
        if (is_set(BSDF::EDiffuseReflection)) { oss << "diffuse_reflection "; type_mask &= ~BSDF::EDiffuseReflection; }
        if (is_set(BSDF::EDiffuseTransmission)) { oss << "diffuse_transmission "; type_mask &= ~BSDF::EDiffuseTransmission; }
        if (is_set(BSDF::EGlossyReflection)) { oss << "glossy_reflection "; type_mask &= ~BSDF::EGlossyReflection; }
        if (is_set(BSDF::EGlossyTransmission)) { oss << "glossy_transmission "; type_mask &= ~BSDF::EGlossyTransmission; }
        if (is_set(BSDF::EDeltaReflection)) { oss << "delta_reflection "; type_mask &= ~BSDF::EDeltaReflection; }
        if (is_set(BSDF::EDeltaTransmission)) { oss << "delta_transmission "; type_mask &= ~BSDF::EDeltaTransmission; }
        if (is_set(BSDF::EDelta1DReflection)) { oss << "delta1D_reflection "; type_mask &= ~BSDF::EDelta1DReflection; }
        if (is_set(BSDF::EDelta1DTransmission)) { oss << "delta1D_transmission "; type_mask &= ~BSDF::EDelta1DTransmission; }
        if (is_set(BSDF::ENull)) { oss << "null "; type_mask &= ~BSDF::ENull; }
        if (is_set(BSDF::EAnisotropic)) { oss << "anisotropic "; type_mask &= ~BSDF::EAnisotropic; }
        if (is_set(BSDF::EFrontSide)) { oss << "front_side "; type_mask &= ~BSDF::EFrontSide; }
        if (is_set(BSDF::EBackSide)) { oss << "back_side "; type_mask &= ~BSDF::EBackSide; }
        if (is_set(BSDF::EUsesSampler)) { oss << "uses_sampler "; type_mask &= ~BSDF::EUsesSampler; }
        if (is_set(BSDF::ESpatiallyVarying)) { oss << "spatially_varying"; type_mask &= ~BSDF::ESpatiallyVarying; }
        if (is_set(BSDF::ENonSymmetric)) { oss << "non_symmetric"; type_mask &= ~BSDF::ENonSymmetric; }
#undef is_set

        Assert(type_mask == 0);
        oss << "}";
        return oss.str();
    }
}

template<typename Point3>
std::ostream &operator<<(std::ostream &os, const BSDFSample<Point3>& bs) {
    os << "BSDFSample[" << std::endl
        << "  its = " << bs.its << "," << std::endl
        << "  sampler = " << bs.sampler << "," << std::endl
        << "  wi = " << bs.wi << "," << std::endl
        << "  wo = " << bs.wo << "," << std::endl
        << "  eta = " << bs.eta << "," << std::endl
        << "  mode = " << bs.mode << "," << std::endl
        << "  type_mask = " << type_mask_to_string(bs.type_mask) << "," << std::endl
        << "  component = " << bs.component << "," << std::endl
        << "  sampled_type = " << type_mask_to_string(bs.sampled_type) << "," << std::endl
        << "  sampled_component = " << bs.sampled_component << std::endl
        << "]";
    return os;
}


NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for dynamic vectorization
// -----------------------------------------------------------------------

ENOKI_STRUCT_DYNAMIC(mitsuba::BSDFSample, its, sampler, wi, wo, eta,
                     mode, type_mask, component, sampled_type, sampled_component)

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
