#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)


/**
 * \brief This list of flags is used to classify the different types of emitters.
 */
enum class EmitterFlags : uint32_t {
    // =============================================================
    //                      Emitter types
    // =============================================================

    /// No flags set (default value)
    None                 = 0x00000,

    /// The emitter lies at a single point in space
    DeltaPosition        = 0x00001,

    /// The emitter emits light in a single direction
    DeltaDirection       = 0x00002,

    /// The emitter is placed at infinity (e.g. environment maps)
    Infinite             = 0x00004,

    /// The emitter is attached to a surface (e.g. area emitters)
    Surface              = 0x00008,

    // =============================================================
    //!                   Other lobe attributes
    // =============================================================

    /// The emission depends on the UV coordinates
    SpatiallyVarying     = 0x00010,

    // =============================================================
    //!                 Compound lobe attributes
    // =============================================================

    /// Delta function in either position or direction
    Delta        = DeltaPosition | DeltaDirection,
};

constexpr uint32_t operator |(EmitterFlags f1, EmitterFlags f2)  { return (uint32_t) f1 | (uint32_t) f2; }
constexpr uint32_t operator |(uint32_t f1, EmitterFlags f2)      { return f1 | (uint32_t) f2; }
constexpr uint32_t operator &(EmitterFlags f1, EmitterFlags f2)  { return (uint32_t) f1 & (uint32_t) f2; }
constexpr uint32_t operator &(uint32_t f1, EmitterFlags f2)      { return f1 & (uint32_t) f2; }
constexpr uint32_t operator ~(EmitterFlags f1)                   { return ~(uint32_t) f1; }
constexpr uint32_t operator +(EmitterFlags e)                    { return (uint32_t) e; }
template <typename UInt32>
constexpr auto has_flag(UInt32 flags, EmitterFlags f)            { return neq(flags & (uint32_t) f, 0u); }



template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Emitter : public Endpoint<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Endpoint)

    /// Is this an environment map light emitter?
    bool is_environment() const {
        return has_flag(m_flags, EmitterFlags::Infinite) && !has_flag(m_flags, EmitterFlags::Delta);
    }

    /// Flags for all components combined.
    uint32_t flags(mask_t<Float> /*active*/ = true) const { return m_flags; }


    ENOKI_CALL_SUPPORT_FRIEND()
    MTS_DECLARE_CLASS()
protected:
    Emitter(const Properties &props);

    virtual ~Emitter();

protected:
    /// Combined flags for all properties of this emitter.
    uint32_t m_flags;
};

MTS_EXTERN_CLASS_RENDER(Emitter)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for vectorized function calls
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
ENOKI_CALL_SUPPORT_TEMPLATE_BEGIN(mitsuba::Emitter)
    ENOKI_CALL_SUPPORT_METHOD(sample_ray)
    ENOKI_CALL_SUPPORT_METHOD(eval)
    ENOKI_CALL_SUPPORT_METHOD(sample_direction)
    ENOKI_CALL_SUPPORT_METHOD(pdf_direction)
    ENOKI_CALL_SUPPORT_METHOD(is_environment)
    ENOKI_CALL_SUPPORT_GETTER(flags, m_flags)
ENOKI_CALL_SUPPORT_TEMPLATE_END(mitsuba::Emitter)

//! @}
// -----------------------------------------------------------------------
