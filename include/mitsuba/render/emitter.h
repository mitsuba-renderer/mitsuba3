#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/fwd.h>
#include <drjit/call.h>

NAMESPACE_BEGIN(mitsuba)


template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Emitter : public Endpoint<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Endpoint, m_shape)
    MI_IMPORT_TYPES()

    /// Is this an environment map light emitter?
    bool is_environment() const {
        return has_flag(m_flags, EndpointFlags::Infinite) &&
               !has_flag(m_flags, EndpointFlags::Delta);
    }

    /// The emitter's sampling weight.
    ScalarFloat sampling_weight() const { return m_sampling_weight; }

    /// Flags for all components combined.
    uint32_t flags(dr::mask_t<Float> /*active*/ = true) const { return m_flags; }

    void traverse(TraversalCallback *callback) override;

    void parameters_changed(const std::vector<std::string> &keys = {}) override;

    /// Return whether the emitter parameters have changed
    bool dirty() const { return m_dirty; }

    /// Modify the emitter's "dirty" flag
    void set_dirty(bool dirty) { m_dirty = dirty; }

    /// This is both a class and the base of various Mitsuba plugins
    MI_DECLARE_PLUGIN_BASE_CLASS(Emitter)

protected:
    Emitter(const Properties &props);

protected:
    /// Combined flags for all properties of this emitter.
    uint32_t m_flags;

    /// Sampling weight
    ScalarFloat m_sampling_weight;

    /// True if the emitter's parameters have changed
    bool m_dirty = false;

    MI_TRAVERSE_CB(Base)
};

MI_EXTERN_CLASS(Emitter)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enables vectorized method calls on Dr.Jit arrays of emitters
// -----------------------------------------------------------------------

DRJIT_CALL_TEMPLATE_BEGIN(mitsuba::Emitter)
    DRJIT_CALL_METHOD(sample_ray)
    DRJIT_CALL_METHOD(sample_direction)
    DRJIT_CALL_METHOD(pdf_direction)
    DRJIT_CALL_METHOD(eval_direction)
    DRJIT_CALL_METHOD(sample_position)
    DRJIT_CALL_METHOD(pdf_position)
    DRJIT_CALL_METHOD(eval)
    DRJIT_CALL_METHOD(sample_wavelengths)
    DRJIT_CALL_GETTER(is_environment)
    DRJIT_CALL_GETTER(flags)
    DRJIT_CALL_GETTER(shape)
    DRJIT_CALL_GETTER(medium)
    DRJIT_CALL_GETTER(sampling_weight)
DRJIT_CALL_END()

//! @}
// -----------------------------------------------------------------------
