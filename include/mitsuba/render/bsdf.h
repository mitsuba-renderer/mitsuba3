#pragma once

#include <mitsuba/core/profiler.h>
#include <mitsuba/render/interaction.h>
#include <drjit/vcall.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Specifies the transport mode when sampling or
 * evaluating a scattering function
 */
enum class TransportMode : uint32_t {
    /// Radiance transport
    Radiance,

    /// Importance transport
    Importance,

    /// Specifies the number of supported transport modes
    TransportModes = 2
};

/**
 * \brief This list of flags is used to classify the different types of lobes
 * that are implemented in a BSDF instance.
 *
 * They are also useful for picking out individual components, e.g., by setting
 * combinations in \ref BSDFContext::type_mask.
 */
enum class BSDFFlags : uint32_t {
    // =============================================================
    //                      BSDF lobe types
    // =============================================================

    /// No flags set (default value)
    Empty                = 0x00000,

    /// 'null' scattering event, i.e. particles do not undergo deflection
    Null                 = 0x00001,

    /// Ideally diffuse reflection
    DiffuseReflection    = 0x00002,

    /// Ideally diffuse transmission
    DiffuseTransmission  = 0x00004,

    /// Glossy reflection
    GlossyReflection     = 0x00008,

    /// Glossy transmission
    GlossyTransmission   = 0x00010,

    /// Reflection into a discrete set of directions
    DeltaReflection      = 0x00020,

    /// Transmission into a discrete set of directions
    DeltaTransmission    = 0x00040,

    /// Reflection into a 1D space of directions
    Delta1DReflection    = 0x00080,

    /// Transmission into a 1D space of directions
    Delta1DTransmission  = 0x00100,

    // =============================================================
    //!                   Other lobe attributes
    // =============================================================

    /// The lobe is not invariant to rotation around the normal
    Anisotropic          = 0x01000,

    /// The BSDF depends on the UV coordinates
    SpatiallyVarying     = 0x02000,

    /// Flags non-symmetry (e.g. transmission in dielectric materials)
    NonSymmetric         = 0x04000,

    /// Supports interactions on the front-facing side
    FrontSide            = 0x08000,

    /// Supports interactions on the back-facing side
    BackSide             = 0x10000,

    /// Does the implementation require access to texture-space differentials
    NeedsDifferentials   = 0x20000,

    // =============================================================
    //!                 Compound lobe attributes
    // =============================================================

    /// Any reflection component (scattering into discrete, 1D, or 2D set of directions)
    Reflection   = DiffuseReflection | DeltaReflection |
                   Delta1DReflection | GlossyReflection,

    /// Any transmission component (scattering into discrete, 1D, or 2D set of directions)
    Transmission = DiffuseTransmission | DeltaTransmission |
                   Delta1DTransmission | GlossyTransmission | Null,

    /// Diffuse scattering into a 2D set of directions
    Diffuse      = DiffuseReflection | DiffuseTransmission,

    /// Non-diffuse scattering into a 2D set of directions
    Glossy       = GlossyReflection | GlossyTransmission,

    /// Scattering into a 2D set of directions
    Smooth       = Diffuse | Glossy,

    /// Scattering into a discrete set of directions
    Delta        = Null | DeltaReflection | DeltaTransmission,

    /// Scattering into a 1D space of directions
    Delta1D      = Delta1DReflection | Delta1DTransmission,

    /// Any kind of scattering
    All          = Diffuse | Glossy | Delta | Delta1D
};

MI_DECLARE_ENUM_OPERATORS(BSDFFlags)

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
struct MI_EXPORT_LIB BSDFContext {
    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Transported mode (radiance or importance)
    TransportMode mode = TransportMode::Radiance;

    /*
     * Bit mask for requested BSDF component types to be sampled/evaluated
     * The default value (equal to \ref BSDFFlags::All) enables all components.
     */
    uint32_t type_mask = (uint32_t) 0x1FFu;

    /// Integer value of requested BSDF component index to be sampled/evaluated.
    uint32_t component = (uint32_t) -1;

    //! @}
    // =============================================================

    BSDFContext() = default;

    BSDFContext(TransportMode mode, uint32_t type_mask = (uint32_t) 0x1FFu,
                uint32_t component = (uint32_t) -1)
        : mode(mode), type_mask(type_mask), component(component) { }

    /**
     * \brief Reverse the direction of light transport in the record
     *
     * This updates the transport mode (radiance to importance and vice versa).
     */
    void reverse() {
        mode = (TransportMode) (1 - (int) mode);
    }

    /**
     * Checks whether a given BSDF component type and BSDF component index are
     * enabled in this context.
     */
    bool is_enabled(BSDFFlags type_, uint32_t component_ = 0) const {
        uint32_t type = (uint32_t) type_;
        return (type_mask == (uint32_t) -1 || (type_mask & type) == type)
            && (component == (uint32_t) -1 || component == component_);
    }
};

/// Data structure holding the result of BSDF sampling operations.
template <typename Float, typename Spectrum> struct BSDFSample3 {
    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Vector3f = Vector<Float, 3>;
    using UInt32   = dr::uint32_array_t<Float>;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Normalized outgoing direction in local coordinates
    Vector3f wo;

    /// Probability density at the sample
    Float pdf;

    /// Relative index of refraction in the sampled direction
    Float eta;

    /// Stores the component type that was sampled by \ref BSDF::sample()
    UInt32 sampled_type;

    /// Stores the component index that was sampled by \ref BSDF::sample()
    UInt32 sampled_component;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Methods
    // =============================================================

    /**
     * \brief Given a surface interaction and an incident/exitant direction
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
    BSDFSample3(const Vector3f &wo)
        : wo(wo), pdf(0.f), eta(1.f), sampled_type(0),
          sampled_component(uint32_t(-1)) { }


    //! @}
    // =============================================================

    DRJIT_STRUCT(BSDFSample3, wo, pdf, eta, sampled_type, sampled_component);
};


/**
 * \brief Bidirectional Scattering Distribution Function (BSDF) interface
 *
 * This class provides an abstract interface to all %BSDF plugins in Mitsuba.
 * It exposes functions for evaluating and sampling the model, and for querying
 * associated probability densities.
 *
 * By default, functions in class sample and evaluate the complete BSDF, but it
 * also allows to pick and choose individual components of multi-lobed BSDFs
 * based on their properties and component indices. This selection is specified
 * using a context data structure that is provided along with every operation.
 *
 * When polarization is enabled, BSDF sampling and evaluation returns 4x4
 * Mueller matrices that describe how scattering changes the polarization state
 * of incident light. Mueller matrices (e.g. for mirrors) are expressed with
 * respect to a reference coordinate system for the incident and outgoing
 * direction. The convention used here is that these coordinate systems are
 * given by <tt>coordinate_system(wi)</tt> and <tt>coordinate_system(wo)</tt>,
 * where 'wi' and 'wo' are the incident and outgoing direction in local
 * coordinates.
 *
 * \sa mitsuba.BSDFContext
 * \sa mitsuba.BSDFSample3f
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB BSDF : public Object {
public:
    MI_IMPORT_TYPES()

    /**
     * \brief Importance sample the BSDF model
     *
     * The function returns a sample data structure along with the importance
     * weight, which is the value of the BSDF divided by the probability
     * density, and multiplied by the cosine foreshortening factor (if needed
     * --- it is omitted for degenerate BSDFs like smooth mirrors/dielectrics).
     *
     * If the supplied context data structures selects subset of components in
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
     *     value: The BSDF value divided by the probability (multiplied by the
     *            cosine foreshortening factor when a non-delta component is
     *            sampled). A zero spectrum indicates that sampling failed.
     */
    virtual std::pair<BSDFSample3f, Spectrum>
    sample(const BSDFContext &ctx,
           const SurfaceInteraction3f &si,
           Float sample1,
           const Point2f &sample2,
           Mask active = true) const = 0;

    /**
     * \brief Evaluate the BSDF f(wi, wo) or its adjoint version f^{*}(wi, wo)
     * and multiply by the cosine foreshortening term.
     *
     * Based on the information in the supplied query context \c ctx, this
     * method will either evaluate the entire BSDF or query individual
     * components (e.g. the diffuse lobe). Only smooth (i.e. non Dirac-delta)
     * components are supported: calling ``eval()`` on a perfectly specular
     * material will return zero.
     *
     * Note that the incident direction does not need to be explicitly
     * specified. It is obtained from the field <tt>si.wi</tt>.
     *
     * \param ctx
     *     A context data structure describing which lobes to evaluate,
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
    virtual Spectrum eval(const BSDFContext &ctx,
                          const SurfaceInteraction3f &si,
                          const Vector3f &wo,
                          Mask active = true) const = 0;

    /**
     * \brief Compute the probability per unit solid angle of sampling a
     * given direction
     *
     * This method provides access to the probability density that would result
     * when supplying the same BSDF context and surface interaction data
     * structures to the \ref sample() method. It correctly handles changes in
     * probability when only a subset of the components is chosen for sampling
     * (this can be done using the \ref BSDFContext::component and \ref
     * BSDFContext::type_mask fields).
     *
     * Note that the incident direction does not need to be explicitly
     * specified. It is obtained from the field <tt>si.wi</tt>.
     *
     * \param ctx
     *     A context data structure describing which lobes to evaluate,
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
    virtual Float pdf(const BSDFContext &ctx,
                      const SurfaceInteraction3f &si,
                      const Vector3f &wo,
                      Mask active = true) const = 0;

    /**
     * \brief Jointly evaluate the BSDF f(wi, wo) and the probability per unit
     * solid angle of sampling the given direction. The result from the evaluated
     * BSDF is multiplied by the cosine foreshortening term.
     *
     * Based on the information in the supplied query context \c ctx, this
     * method will either evaluate the entire BSDF or query individual
     * components (e.g. the diffuse lobe). Only smooth (i.e. non Dirac-delta)
     * components are supported: calling ``eval()`` on a perfectly specular
     * material will return zero.
     *
     * This method provides access to the probability density that would result
     * when supplying the same BSDF context and surface interaction data
     * structures to the \ref sample() method. It correctly handles changes in
     * probability when only a subset of the components is chosen for sampling
     * (this can be done using the \ref BSDFContext::component and \ref
     * BSDFContext::type_mask fields).
     *
     * Note that the incident direction does not need to be explicitly
     * specified. It is obtained from the field <tt>si.wi</tt>.
     *
     * \param ctx
     *     A context data structure describing which lobes to evaluate,
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
    virtual std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
                                                const SurfaceInteraction3f &si,
                                                const Vector3f &wo,
                                                Mask active = true) const;

    /**
     * \brief Jointly evaluate the BSDF f(wi, wo), the probability per unit
     * solid angle of sampling the given direction \c wo and importance sample
     * the BSDF model.
     *
     * This is simply a wrapper around two separate function calls to eval_pdf()
     * and sample(). The function exists to perform a smaller number of virtual
     * function calls, which has some performance benefits on highly vectorized
     * JIT variants of the renderer. (A ~20% performance improvement for the
     * basic path tracer on CUDA)
     *
     * \param ctx
     *     A context data structure describing which lobes to evaluate,
     *     and whether radiance or importance are being transported.
     *
     * \param si
     *     A surface interaction data structure describing the underlying
     *     surface position. The incident direction is obtained from
     *     the field <tt>si.wi</tt>.
     *
     * \param wo
     *     The outgoing direction
     *
     * \param sample1
     *     A uniformly distributed sample on \f$[0,1]\f$. It is used
     *     to select the BSDF lobe in multi-lobe models.
     *
     * \param sample2
     *     A uniformly distributed sample on \f$[0,1]^2\f$. It is
     *     used to generate the sampled direction.
     */
    virtual std::tuple<Spectrum, Float, BSDFSample3f, Spectrum>
    eval_pdf_sample(const BSDFContext &ctx,
                    const SurfaceInteraction3f &si,
                    const Vector3f &wo,
                    Float sample1,
                    const Point2f &sample2,
                    Mask active = true) const;


    /**
     * \brief Evaluate un-scattered transmission component of the BSDF
     *
     * This method will evaluate the un-scattered transmission (\ref
     * BSDFFlags::Null) of the BSDF for light arriving from direction \c w.
     * The default implementation returns zero.
     *
     * \param si
     *     A surface interaction data structure describing the underlying
     *     surface position. The incident direction is obtained from
     *     the field <tt>si.wi</tt>.
     */
    virtual Spectrum eval_null_transmission(const SurfaceInteraction3f &si,
                                            Mask active = true) const;


    // -----------------------------------------------------------------------
    //! @{ \name BSDF property accessors (components, flags, etc)
    // -----------------------------------------------------------------------

    /// Flags for all components combined.
    uint32_t flags(Mask /*active*/ = true) const { return m_flags; }

    /// Flags for a specific component of this BSDF.
    uint32_t flags(size_t i, Mask /*active*/ = true) const {
        Assert(i < m_components.size());
        return m_components[i];
    }

    /// Does the implementation require access to texture-space differentials?
    bool needs_differentials(Mask /*active*/ = true) const {
        return has_flag(m_flags, BSDFFlags::NeedsDifferentials);
    }

    /// Number of components this BSDF is comprised of.
    size_t component_count(Mask /*active*/ = true) const {
        return m_components.size();
    }

    /// Return a string identifier
    std::string id() const override { return m_id; }

    /// Set a string identifier
    void set_id(const std::string& id) override { m_id = id; };

    /**
     * \brief Evaluate the diffuse reflectance
     *
     * This method approximates the total diffuse reflectance for a given
     * direction. For some materials, an exact value can be computed
     * inexpensively.
     * When this is not possible, the value is approximated by
     * evaluating the BSDF for a normal outgoing direction and returning this
     * value multiplied by pi. This is the default behaviour of this method.
     *
     * \param si
     *     A surface interaction data structure describing the underlying
     *     surface position.
     */
    virtual Spectrum eval_diffuse_reflectance(const SurfaceInteraction3f &si,
                                              Mask active = true) const;

    /// Return a human-readable representation of the BSDF
    std::string to_string() const override = 0;

    //! @}
    // -----------------------------------------------------------------------

    DRJIT_VCALL_REGISTER(Float, mitsuba::BSDF)

    MI_DECLARE_CLASS()
protected:
    BSDF(const Properties &props);
    virtual ~BSDF();

protected:
    /// Combined flags for all components of this BSDF.
    uint32_t m_flags;

    /// Flags for each component of this BSDF.
    std::vector<uint32_t> m_components;

    /// Identifier (if available)
    std::string m_id;
};

// -----------------------------------------------------------------------
//! @{ \name Misc implementations
// -----------------------------------------------------------------------

extern MI_EXPORT_LIB std::ostream &operator<<(std::ostream &os,
                                              const TransportMode &mode);

extern MI_EXPORT_LIB std::ostream &operator<<(std::ostream &os,
                                              const BSDFContext& ctx);

template <typename Float, typename Spectrum>
std::ostream &operator<<(std::ostream &os, const BSDFSample3<Float, Spectrum>& bs) {
    os << "BSDFSample[" << std::endl
        << "  wo = " << bs.wo << "," << std::endl
        << "  pdf = " << bs.pdf << "," << std::endl
        << "  eta = " << bs.eta << "," << std::endl
        << "  sampled_type = " << "TODO" /*type_mask_to_string(bs.sampled_type)*/ << "," << std::endl
        << "  sampled_component = " << bs.sampled_component << std::endl
        << "]";
    return os;
}

template <typename Float, typename Spectrum>
typename SurfaceInteraction<Float, Spectrum>::BSDFPtr SurfaceInteraction<Float, Spectrum>::bsdf(
    const typename SurfaceInteraction<Float, Spectrum>::RayDifferential3f &ray) {
    const BSDFPtr bsdf = shape->bsdf();

    /// TODO: revisit the 'false' default for autodiff mode once there are actually BRDFs using
    /// differentials
    if constexpr (!dr::is_diff_v<Float>) {
        if (!has_uv_partials() && dr::any(bsdf->needs_differentials()))
            compute_uv_partials(ray);
    } else {
        DRJIT_MARK_USED(ray);
    }

    return bsdf;
}

//! @}
// -----------------------------------------------------------------------

MI_EXTERN_CLASS(BSDF)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Dr.Jit support for vectorized function calls
// -----------------------------------------------------------------------

DRJIT_VCALL_TEMPLATE_BEGIN(mitsuba::BSDF)
    DRJIT_VCALL_METHOD(sample)
    DRJIT_VCALL_METHOD(eval)
    DRJIT_VCALL_METHOD(eval_null_transmission)
    DRJIT_VCALL_METHOD(pdf)
    DRJIT_VCALL_METHOD(eval_pdf)
    DRJIT_VCALL_METHOD(eval_pdf_sample)
    DRJIT_VCALL_METHOD(eval_diffuse_reflectance)
    DRJIT_VCALL_GETTER(flags, uint32_t)
    auto needs_differentials() const {
        return has_flag(flags(), mitsuba::BSDFFlags::NeedsDifferentials);
    }
DRJIT_VCALL_TEMPLATE_END(mitsuba::BSDF)

//! @}
// -----------------------------------------------------------------------
