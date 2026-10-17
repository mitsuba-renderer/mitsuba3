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

    /// The emitter is placed at infinity (i.e., it is an environment map)
    Infinite             = 0x00004,

    /// The emitter is attached to a surface (i.e., it is an area emitter)
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

    /**
     * The emitter importance samples a texture and maps the result onto
     * the shape through ``Shape::eval_parameterization()``.
     */
    SamplesTexture       = 0x00080,

    // =============================================================
    //                  Compound lobe attributes
    // =============================================================

    /// Delta function in either position or direction
    Delta        = DeltaPosition | DeltaDirection,
};

MI_DECLARE_ENUM_OPERATORS(EmitterFlags)

/**
 * Summary of an emitter for emitter selection
 *
 * Emitter sampling data structures need information about the position and
 * energy of emitters. `Emitter.bounds()` returns this record for emitters of
 * finite extent. The orientation cone follows Conty Estevez and Kulla (2018).
 */
template <typename Float>
struct EmitterBounds {
    MI_IMPORT_CORE_TYPES()

    /// World-space bounding box of the emitting geometry
    ScalarBoundingBox3f bbox;

    /// Axis of the orientation cone
    ScalarVector3f axis;

    /**
     * Bound on the angle between any emitter normal and ``axis``, in radians
     *
     * A point emitter reports :math:`\pi` and a spot emitter :math:`0`.
     */
    ScalarFloat theta_o;

    /**
     * Angular spread of the emission around each normal, in radians
     *
     * A point emitter reports :math:`\pi/2` and a spot emitter its cutoff angle.
     */
    ScalarFloat theta_e;

    /**
     * Estimate of the radiant intensity (power per steradian) that the
     * emitter radiates into the directions of its cone
     *
     * For an area emitter, this is the radiance times the surface area. For
     * a point or spot emitter, it is the intensity.
     */
    ScalarFloat energy;
};

template <typename Float>
std::ostream &operator<<(std::ostream &os, const EmitterBounds<Float> &lb) {
    os << "EmitterBounds[" << std::endl
       << "  bbox = " << string::indent(lb.bbox) << "," << std::endl
       << "  axis = " << lb.axis << "," << std::endl
       << "  theta_o = " << lb.theta_o << "," << std::endl
       << "  theta_e = " << lb.theta_e << "," << std::endl
       << "  energy = " << lb.energy << std::endl
       << "]";
    return os;
}

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

    /**
     * Return a summary of the position, orientation and energy of the emitter
     *
     * The summary assumes a constant ``to_world`` transformation. Infinite
     * emitters, portals and the default implementation raise an exception.
     * See `EmitterBounds`.
     */
    virtual EmitterBounds bounds() const;

    /**
     * Estimate the irradiance that an infinite emitter delivers
     *
     * Returns the direction toward the emitter and an estimate of the
     * irradiance at a surface that faces it. Emitter selection uses this
     * estimate to compare infinite emitters with emitters of finite extent.
     * Emitters that cover the whole sphere of directions return a zero
     * direction. An environment map reports :math:`\pi` times its mean
     * radiance, and a directional emitter reports its irradiance. The
     * estimate does not include `sampling_weight()`. Only emitters with the
     * `EmitterFlags.Infinite` flag implement this method. The default
     * implementation raises an exception.
     */
    virtual std::pair<ScalarVector3f, ScalarFloat> irradiance_estimate() const;

    /**
     * Path code of the emitter within the scene's emitter tree
     *
     * Each level of the path from the root to the emitter's leaf stores the
     * index of the chosen child (one of 8) in 3 bits, starting from the
     * lowest bits. An additional set bit above the last level marks the
     * length of the path. The code is zero when the emitter is not part of
     * the tree. The scene assigns the code when it builds the tree.
     */
    uint32_t emitter_tree_code() const { return m_emitter_tree_code; }

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

    /// Path code within the scene's emitter tree (see `emitter_tree_code()`)
    uint32_t m_emitter_tree_code = 0;

private:
    friend class mitsuba::Scene<Float, Spectrum>;
    friend class mitsuba::EmitterTree<Float, Spectrum>;

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
    DRJIT_CALL_GETTER(emitter_tree_code)
DRJIT_CALL_END()

// -----------------------------------------------------------------------
