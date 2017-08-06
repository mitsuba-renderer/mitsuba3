#pragma once

#include <mitsuba/render/fwd.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/simd.h>

NAMESPACE_BEGIN(mitsuba)

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
     * by setting combinations in \ref BSDFSamplingRecord::typeMask.
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

    bool needs_differentials() const { return m_needs_differentials; }

    uint32_t flags() const { return m_flags; }

    MTS_DECLARE_CLASS()

protected:
    virtual ~BSDF() { }

protected:
    bool m_needs_differentials = false;
    uint32_t m_flags;
};

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for packets of BSDF pointers
// -----------------------------------------------------------------------

ENOKI_CALL_SUPPORT_BEGIN(mitsuba::BSDFP)
ENOKI_CALL_SUPPORT_SCALAR(needs_differentials)
ENOKI_CALL_SUPPORT_SCALAR(flags)
ENOKI_CALL_SUPPORT_END(mitsuba::BSDFP)

//! @}
// -----------------------------------------------------------------------
