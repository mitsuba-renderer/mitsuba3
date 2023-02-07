#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/fwd.h>
#include <drjit/vcall.h>

NAMESPACE_BEGIN(mitsuba)


/**
 * \brief This list of flags is used to classify the different types of emitters.
 */
enum class EmitterFlags : uint32_t {
    // =============================================================
    //                      Emitter types
    // =============================================================

    /// No flags set (default value)
    Empty                = 0x00000,

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

MI_DECLARE_ENUM_OPERATORS(EmitterFlags)

template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Emitter : public Endpoint<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Endpoint, m_shape)

    /// Is this an environment map light emitter?
    bool is_environment() const {
        return has_flag(m_flags, EmitterFlags::Infinite) &&
               !has_flag(m_flags, EmitterFlags::Delta);
    }

    /// Flags for all components combined.
    uint32_t flags(dr::mask_t<Float> /*active*/ = true) const { return m_flags; }

    DRJIT_VCALL_REGISTER(Float, mitsuba::Emitter)

    MI_DECLARE_CLASS()
protected:
    Emitter(const Properties &props);

    virtual ~Emitter();

protected:
    /// Combined flags for all properties of this emitter.
    uint32_t m_flags;
};

MI_EXTERN_CLASS(Emitter)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Dr.Jit support for vectorized function calls
// -----------------------------------------------------------------------

DRJIT_VCALL_TEMPLATE_BEGIN(mitsuba::Emitter)
    DRJIT_VCALL_METHOD(sample_ray)
    DRJIT_VCALL_METHOD(sample_direction)
    DRJIT_VCALL_METHOD(pdf_direction)
    DRJIT_VCALL_METHOD(eval_direction)
    DRJIT_VCALL_METHOD(sample_position)
    DRJIT_VCALL_METHOD(pdf_position)
    DRJIT_VCALL_METHOD(eval)
    DRJIT_VCALL_METHOD(sample_wavelengths)
    DRJIT_VCALL_METHOD(is_environment)
    DRJIT_VCALL_GETTER(flags, uint32_t)
    DRJIT_VCALL_GETTER(shape, const typename Class::Shape *)
    DRJIT_VCALL_GETTER(medium, const typename Class::Medium *)
DRJIT_VCALL_TEMPLATE_END(mitsuba::Emitter)

//! @}
// -----------------------------------------------------------------------
