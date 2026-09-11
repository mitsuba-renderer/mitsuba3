#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/fwd.h>
#include <drjit/call.h>

NAMESPACE_BEGIN(mitsuba)


/**
 * This list of flags is used to classify the different types of emitters.
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
    //                    Other lobe attributes
    // =============================================================

    /// The emission depends on the UV coordinates
    SpatiallyVarying     = 0x00010,

    /// The emitter is hidden from directly visible (camera) rays.
    Invisible            = 0x00020,

    /// Light portal (see ``Scene::portals()``), does not emit anything itself
    Portal               = 0x00040,

    // =============================================================
    //                  Compound lobe attributes
    // =============================================================

    /// Delta function in either position or direction
    Delta        = DeltaPosition | DeltaDirection,
};

MI_DECLARE_ENUM_OPERATORS(EmitterFlags)

template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Emitter : public Endpoint<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Endpoint, m_shape)
    MI_IMPORT_TYPES()

    /// Is this an environment map light emitter?
    bool is_environment() const {
        return has_flag(m_flags, EmitterFlags::Infinite) &&
               !has_flag(m_flags, EmitterFlags::Delta);
    }

    /// Is this a light portal? (see ``Scene::portals()``)
    bool is_portal() const { return has_flag(m_flags, EmitterFlags::Portal); }

    /// Relative weight of sampling this emitter. Zero when hidden from secondary rays.
    ScalarFloat sampling_weight() const {
        ShapeVisibility v = visibility();
        return (v == ShapeVisibility::Secondary || v == ShapeVisibility::All)
                   ? m_sampling_weight : 0.f;
    }

    /// Ray categories that can see this emitter
    ShapeVisibility visibility() const {
        return m_shape ? m_shape->visibility() : m_visibility;
    }

    /// Return the complete set of emitter flags
    uint32_t flags() const {
        ShapeVisibility v = visibility();
        bool visible = v == ShapeVisibility::Primary || v == ShapeVisibility::All;
        return m_flags | (visible ? 0u : (uint32_t) EmitterFlags::Invisible);
    }

    void traverse(TraversalCallback *callback) override;

    void parameters_changed(const std::vector<std::string> &keys = {}) override;

    /// Return whether the emitter parameters have changed
    bool dirty() const { return m_dirty; }

    /// Modify the emitter's ``dirty`` flag
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

    /// Ray categories that can see the emitter
    ShapeVisibility m_visibility;

    /// True if the emitter's parameters have changed
    bool m_dirty = false;

private:
    friend class mitsuba::Scene<Float, Spectrum>;

    MI_TRAVERSE_CB(Base)
};

MI_EXTERN_CLASS(Emitter)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
// Enables vectorized method calls on Dr.Jit arrays of emitters
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

// -----------------------------------------------------------------------
