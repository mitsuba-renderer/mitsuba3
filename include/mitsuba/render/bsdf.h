#pragma once

#include <mitsuba/render/interaction.h>
#include <mitsuba/render/common.h>
#include <mitsuba/render/mueller.h>
#include <mitsuba/core/profiler.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Context data structure for BSDF evaluation and sampling
 *
 * BSDF models in Mitsuba can be queried and sampled using a variety of
 * different modes -- for instance, a rendering algorithm can indicate whether
 * radiance or importance is being transported, and it can also restrict
 * evaluation and sampling to a subset of lobes in a a multi-lobe BSDF model.
 *
 * The \ref BSDFContext data structure encodes these preferences and is
 * supplied to most \ref BSDF methods.
 */
struct MTS_EXPORT_RENDER BSDFContext {
    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Transported mode (radiance or importance)
    ETransportMode mode;

    /*
     * Bit mask for requested BSDF component types to be sampled/evaluated
     * The default value (equal to \ref BSDF::EAll) enables all components.
     */
    uint32_t type_mask = (uint32_t) 0x1FFu;

    /// Integer value of requested BSDF component index to be sampled/evaluated.
    uint32_t component = (uint32_t) -1;

    //! @}
    // =============================================================

    BSDFContext(ETransportMode mode = ERadiance)
        : mode(mode) { }

    BSDFContext(ETransportMode mode, uint32_t type_mask, uint32_t component)
        : mode(mode), type_mask(type_mask), component(component) { }

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
        return (type_mask == (uint32_t) -1 || (type_mask & type_) == type_)
            && (component == (uint32_t) -1 || component == component_);
    }
};

/// Data structure holding the result of BSDF sampling operations.
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

    /// Normalized outgoing direction in local coordinates
    Vector3 wo;

    /// Probability density at the sample
    Value pdf;

    /// Relative index of refraction in the sampled direction
    Value eta;

    /// Stores the component type that was sampled by \ref BSDF::sample()
    Index sampled_type = 0;

    /// Stores the component index that was sampled by \ref BSDF::sample()
    Index sampled_component = uint32_t(-1);

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
     * By default, all components will be sampled regardless of what measure
     * they live on.
     *
     * \param wo
     *      An outgoing direction in local coordinates. This should
     *      be a normalized direction vector that points \a away from
     *      the scattering event.
     */
    BSDFSample(const Vector3 &wo)
        : wo(wo), pdf(0.f), eta(1.f), sampled_type(0),
          sampled_component(uint32_t(-1)) { }


    //! @}
    // =============================================================

    ENOKI_STRUCT(BSDFSample, wo, pdf, eta, sampled_type, sampled_component)
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

        /// Does the implementation require access to texture-space differentials
        ENeedsDifferentials   = 0x20000
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
     * \brief Importance sample the BSDF model
     *
     * The function returns a sample data structure along with the importance
     * weight, which is the value of the BSDF divided by the probability
     * density, and multiplied by the cosine foreshortening factor (if needed
     * --- it is omitted for degenerate BSDFs like smooth mirrors/dielectrics).
     *
     * If the supplied context data strutcures selects subset of components in
     * a multi-lobe BRDF model, the sampling is restricted to this subset.
     * Depending on the provided transport type, either the BSDF or its adjoint
     * version is sampled.
     *
     * When sampling a continuous/non-delta component, this method also
     * multiplies by the cosine foreshorening factor with respect to the
     * sampled direction.
     *
     * \param ctx
     *     A context data structure describing which lobes to sample,
     *     and whether radiance or importance are being transported.
     *
     * \param si
     *     A surface interaction data structure describing the underlying
     *     surface position. The incident direction is obtained from
     *     the field <tt>si.wi</tt>.
     *
     * \param sample1
     *     A uniformly distributed sample on \f$[0,1]\f$. It is used
     *     to select the BSDF lobe in multi-lobe models.
     *
     * \param sample2
     *     A uniformly distributed sample on \f$[0,1]^2\f$. It is
     *     used to generate the sampled direction.
     *
     * \return A pair (bs, value) consisting of
     *
     *     bs:    Sampling record, indicating the sampled direction, PDF values
     *            and other information. The contents are undefined if sampling
     *            failed.
     *
     *     value: The BSDF value (multiplied by the cosine foreshortening
     *            factor when a non-delta component is sampled). A zero
     *            spectrum indicates that sampling failed.
     */
    virtual std::pair<BSDFSample3f, Spectrumf>
    sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
           Float sample1, const Point2f &sample2) const = 0;

    /// Vectorized version of \ref sample()
    virtual std::pair<BSDFSample3fP, SpectrumfP>
    sample(const BSDFContext &ctx, const SurfaceInteraction3fP &si,
           FloatP sample1, const Point2fP &sample2,
           MaskP active = true) const = 0;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref sample()
    std::pair<BSDFSample3f, Spectrumf>
    sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
           Float sample1, const Point2f &sample2, bool /*unused*/) const {
        return sample(ctx, si, sample1, sample2);
    }

    /**
     * \brief Polarized version of \ref sample()
     *
     * Since there is no special polarized importance sampling
     * this method behaves very similar to the standard one.
     *
     * \return A pair (bs, value) consisting of
     *
     *     bs:    Sampling record, indicating the sampled direction, PDF values
     *            and other information. The contents are undefined if sampling
     *            failed.
     *
     *     value: Mueller matrix of BSDF values (multiplied by the cosine
     *            foreshortening factor when a non-delta component is sampled).
     *            A zero spectrum indicates that sampling failed.
     *
     *            The Mueller matrix has the following coordinate system:
     *            CoordinateSystem(w_i_local) -> CoordinateSystem(w_o_local).
     */
    virtual std::pair<BSDFSample3f, MuellerMatrixSf>
    sample_pol(const BSDFContext &ctx, const SurfaceInteraction3f &si,
               Float sample1, const Point2f &sample2) const;

    /// Vectorized version of \ref sample_pol()
    virtual std::pair<BSDFSample3fP, MuellerMatrixSfP>
    sample_pol(const BSDFContext &ctx, const SurfaceInteraction3fP &si,
               FloatP sample1, const Point2fP &sample2,
               MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref sample_pol()
    std::pair<BSDFSample3f, MuellerMatrixSf>
    sample_pol(const BSDFContext &ctx, const SurfaceInteraction3f &si,
               Float sample1, const Point2f &sample2, bool /*unused*/) const {
        return sample_pol(ctx, si, sample1, sample2);
    }

    /**
     * \brief Evaluate the BSDF f(wi, wo) or its adjoint version f^{*}(wi, wo)
     * and multiply by the cosine foreshortening term.
     *
     * Based on the information in the supplied query context \c ctx, this
     * method will either evaluate the entire BSDF or query individual
     * components (e.g. the diffuse lobe). Only smooth (i.e. non Dirac-delta)
     * components are supported: calling \c eval() on a perfectly specular
     * material will return zero.
     *
     * Note that the incident direction does not need to be explicitly
     * specified. It is obtained from the field <tt>si.wi</tt>.
     *
     * \param ctx
     *     A context data structure describing which lobes to evalute,
     *     and whether radiance or importance are being transported.
     *
     * \param si
     *     A surface interaction data structure describing the underlying
     *     surface position. The incident direction is obtained from
     *     the field <tt>si.wi</tt>.
     *
     * \param wo
     *     The outgoing direction
     */
    virtual Spectrumf eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                           const Vector3f &wo) const = 0;

    /// Vectorized version of \ref eval()
    virtual SpectrumfP eval(const BSDFContext &ctx, const SurfaceInteraction3fP &si,
                            const Vector3fP &wo, MaskP active = true) const = 0;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref eval()
    Spectrumf eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                   const Vector3f &wo, bool /*unused*/) const {
        return eval(ctx, si, wo);
    }

    /**
     * \brief Polarized version of \ref eval()
     *
     * The resulting MuellerMatrix has the following coordinate system:
     * CoordinateSystem(w_i_local) -> CoordinateSystem(w_o_local)
     * SurfaceInteraction::to_world_mueller can be used to transform this matrix
     * into world space.
     * This function behaves in a very similar way to the unpolarized function
     * so the documentation there applies.
     *
     * \return Mueller matrix of BSDF values (multiplied by the cosine
     *         foreshortening factor when a non-delta component is sampled).
     *         A zero spectrum indicates that sampling failed.
     *
     *         The Mueller matrix has the following coordinate system:
     *         CoordinateSystem(w_i_local) -> CoordinateSystem(w_o_local).
     */
    virtual MuellerMatrixSf eval_pol(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                     const Vector3f &wo) const;

    /// Vectorized version of \ref eval_pol()
    virtual MuellerMatrixSfP eval_pol(const BSDFContext &ctx, const SurfaceInteraction3fP &si,
                                      const Vector3fP &wo, MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref eval_pol()
    MuellerMatrixSf eval_pol(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                             const Vector3f &wo, bool /*unused*/) const {
        return eval_pol(ctx, si, wo);
    }

    /**
     * \brief Compute the probability per unit solid angle of sampling a
     * given direction
     *
     * This method provides access to the probability density that would result
     * when supplying the same BSDF context and surface interaction data
     * structures to the \ref sample() method. It correctly handles changes in
     * probability when only a subset of the components is chosen for sampling
     * (this can be done using the \ref BSDFContext::component and \ref
     * BSDFCOntext::type_mask fields).
     *
     * Note that the incident direction does not need to be explicitly
     * specified. It is obtained from the field <tt>si.wi</tt>.
     *
     * \param ctx
     *     A context data structure describing which lobes to evalute,
     *     and whether radiance or importance are being transported.
     *
     * \param si
     *     A surface interaction data structure describing the underlying
     *     surface position. The incident direction is obtained from
     *     the field <tt>si.wi</tt>.
     *
     * \param wo
     *     The outgoing direction
     */
    virtual Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                      const Vector3f &wo) const = 0;

    /// Vectorized version of \ref pdf()
    virtual FloatP pdf(const BSDFContext &ctx, const SurfaceInteraction3fP &si,
                       const Vector3fP &wo, MaskP active = true) const = 0;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref pdf()
    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, bool /*unused*/) const {
        return pdf(ctx, si, wo);
    }

    // -----------------------------------------------------------------------
    //! @{ \name BSDF property accessors (components, flags, etc)
    // -----------------------------------------------------------------------

    /// Number of components this BSDF is comprised of.
    size_t component_count() const { return m_components.size(); }

    /// Does the implementation require access to texture-space differentials?
    bool needs_differentials() const { return m_flags & ENeedsDifferentials; }

    /// Flags for all components combined.
    uint32_t flags() const { return m_flags; }

    /// Flags for a specific component of this BSDF.
    uint32_t flags(size_t i) const { Assert(i < m_components.size()); return m_components[i]; }

    /// Return a human-readable representation of the BSDF
    std::string to_string() const override = 0;

    //! @}
    // -----------------------------------------------------------------------

    MTS_DECLARE_CLASS()

protected:
    BSDF();
    virtual ~BSDF();

protected:
    /// Combined flags for all components of this BSDF.
    uint32_t m_flags;

    /// Flags for each component of this BSDF.
    std::vector<uint32_t> m_components;
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
        if (is_set(BSDF::ESpatiallyVarying)) { oss << "spatially_varying "; type_mask &= ~BSDF::ESpatiallyVarying; }
        if (is_set(BSDF::ENonSymmetric)) { oss << "non_symmetric "; type_mask &= ~BSDF::ENonSymmetric; }
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

ENOKI_STRUCT_SUPPORT(mitsuba::BSDFSample, wo, pdf, eta,
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
 * plugins to instantiate concrete versions of the \c sample,
 * \c eval and \c pdf functions.
 */
#define MTS_IMPLEMENT_BSDF()                                                   \
    using BSDF::sample;                                                        \
    using BSDF::eval;                                                          \
    using BSDF::pdf;                                                           \
    std::pair<BSDFSample3f, Spectrumf> sample(                                 \
            const BSDFContext &ctx, const SurfaceInteraction3f &si,            \
            Float sample1, const Point2f &sample2) const override {            \
        ScopedPhase p(EProfilerPhase::EBSDFSample);                            \
        return sample_impl(ctx, si, sample1, sample2, true);                   \
    }                                                                          \
    std::pair<BSDFSample3fP, SpectrumfP> sample(                               \
            const BSDFContext &ctx, const SurfaceInteraction3fP &si,           \
            FloatP sample1, const Point2fP &sample2,                           \
            MaskP active = true) const override {                              \
        ScopedPhase p(EProfilerPhase::EBSDFSampleP);                           \
        return sample_impl(ctx, si, sample1, sample2, active);                 \
    }                                                                          \
    Spectrumf eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,     \
                   const Vector3f &wo) const override {                        \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluate);                          \
        return eval_impl(ctx, si, wo, true);                                   \
    }                                                                          \
    SpectrumfP eval(const BSDFContext &ctx, const SurfaceInteraction3fP &si,   \
                    const Vector3fP &wo, MaskP active) const override {        \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluateP);                         \
        return eval_impl(ctx, si, wo, active);                                 \
    }                                                                          \
    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,          \
              const Vector3f &wo) const override {                             \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluate);                          \
        return pdf_impl(ctx, si, wo, true);                                    \
    }                                                                          \
    FloatP pdf(const BSDFContext &ctx, const SurfaceInteraction3fP &si,        \
               const Vector3fP &wo, MaskP active) const override {             \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluateP);                         \
        return pdf_impl(ctx, si, wo, active);                                  \
    }

/// Instantiates concrete scalar and packet versions of the polarized BSDF plugin API
#define MTS_IMPLEMENT_BSDF_POLARIZED()                                                          \
    std::pair<BSDFSample3f, MuellerMatrixSf> sample_pol(                                         \
            const BSDFContext &ctx, const SurfaceInteraction3f &si,                             \
            Float sample1, const Point2f &sample2) const override {                             \
        return sample_pol_impl(ctx, si, sample1, sample2, true);                                \
    }                                                                                           \
    std::pair<BSDFSample3fP, MuellerMatrixSfP> sample_pol(                                       \
            const BSDFContext &ctx, const SurfaceInteraction3fP &si,                            \
            FloatP sample1, const Point2fP &sample2,                                            \
            MaskP active = true) const override {                                               \
        return sample_pol_impl(ctx, si, sample1, sample2, active);                              \
    }                                                                                           \
    MuellerMatrixSf eval_pol(const BSDFContext &ctx, const SurfaceInteraction3f &si,             \
                                  const Vector3f &wo) const override {                          \
        return eval_pol_impl(ctx, si, wo, true);                                                \
    }                                                                                           \
    MuellerMatrixSfP eval_pol(const BSDFContext &ctx, const SurfaceInteraction3fP &si,           \
                                   const Vector3fP &wo, MaskP active) const override {          \
        return eval_pol_impl(ctx, si, wo, active);                                              \
    }