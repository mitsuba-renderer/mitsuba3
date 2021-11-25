#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/fwd.h>
#include <enoki/vcall.h>

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

    /// The emitter is attached to a volume (e.g. volume emitters)
    Volume               = 0x00010,

    // =============================================================
    //!                   Other lobe attributes
    // =============================================================

    /// The emission depends on the UV coordinates
    SpatiallyVarying     = 0x00020,

    // =============================================================
    //!                 Compound lobe attributes
    // =============================================================

    /// Delta function in either position or direction
    Delta        = DeltaPosition | DeltaDirection,
};

MTS_DECLARE_ENUM_OPERATORS(EmitterFlags)

template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Emitter : public Endpoint<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Endpoint, m_shape)

    /// Is this an environment map light emitter?
    bool is_environment() const {
        return has_flag(m_flags, EmitterFlags::Infinite) &&
               !has_flag(m_flags, EmitterFlags::Delta);
    }

    /// Flags for all components combined.
    uint32_t flags(ek::mask_t<Float> /*active*/ = true) const { return m_flags; }


    ENOKI_VCALL_REGISTER(Float, mitsuba::Emitter)

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

ENOKI_VCALL_TEMPLATE_BEGIN(mitsuba::Emitter)
    ENOKI_VCALL_METHOD(sample_ray)
    ENOKI_VCALL_METHOD(eval)
    ENOKI_VCALL_METHOD(sample_direction)
    ENOKI_VCALL_METHOD(pdf_direction)
    ENOKI_VCALL_METHOD(sample_position)
    ENOKI_VCALL_METHOD(sample_wavelengths)
    ENOKI_VCALL_METHOD(is_environment)
    ENOKI_VCALL_GETTER(flags, uint32_t)
    ENOKI_VCALL_GETTER(shape, const typename Class::Shape *)
    ENOKI_VCALL_GETTER(medium, const typename Class::Medium *)
ENOKI_VCALL_TEMPLATE_END(mitsuba::Emitter)

//! @}
// -----------------------------------------------------------------------
