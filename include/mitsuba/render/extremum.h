#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/volume.h>
#include <mitsuba/render/extremum_segment.h>
#include <mitsuba/render/tracking.h>
#include <drjit/call.h>

#include <optional>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Abstract base class for extremum structures
 *
 * This class provides an interface for spatial data structures that store
 * coarse volumetric local extrema (majorant/minorant). This enables efficient
 * use of tracking algorithms with locally-adaptive majorants and minorants.
 *
 * The extremum structure needs to be built using the ``update_extremum``
 * function, it is **not** called automatically in the constructor. It is the
 * caller's responsability to pass the ``Volume`` plugin the extremum is
 * derived from.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Extremum : public JitObject<Extremum<Float, Spectrum>> {
public:
    MI_IMPORT_TYPES(Medium, Sampler, Volume)

    using TrackingStateType    = TrackingState<Float, Spectrum>;
    using TrackingFunctionType = TrackingFunction<Float, Spectrum>;

    /// Destructor
    ~Extremum();

    /// Setter for the bbox over which the structure must be valid.
    MI_INLINE void set_bbox(ScalarBoundingBox3f bbox) { m_bbox = bbox; };

    /// Setter for the scale by which to multiply the extremum values.
    MI_INLINE void set_scale(ScalarFloat scale) { m_scale = scale; }

    /**
     * \brief Update the bbox and scale, and rebuild the structure.
     *
     * The \c bbox parameters indicates the domain over which the extremum
     * can be queried. It can be larger or smaller than the underlying
     * volume bbox. It is the extremum's responsibility to be valid over this
     * area. The building implementation is handled in ``build``.
     *
     * \param bbox      The validity bbox of the extremum structure
     * \param volume    The volume from which to derive the extremum structure
     * \param scale     The scale by which to multiply the extremum values
     */
    void update_extremum(const ScalarBoundingBox3f &bbox,
                         const Volume *volume,
                         std::optional<ScalarFloat> scale);

    /**
     * \brief Build the extremum structure of \c volume.
     *
     * Implements the logic that constructs the extremum structure from a
     * \c volume. Called by ``update_extremum`` which is itself called by
     * the owning ``Medium``
     *
     * \param volume  Volume to compute extremum values from
     */
    virtual void build(const Volume *volume) = 0;


    /**
     * \brief Traverse the extremum along a ray and applies a callback at each
     * encountered segment.
     *
     * This method traverses the extremum structure segment by segment. At each
     * segment, the callback ``func`` is called to advance the ``state``. This
     * is useful for example to implement Delta Tracking, Ratio Tracking, and
     * Residual Ratio Tracking. The callback is typically defined in the
     * integrator.
     *
     * \param ray           Ray along which to sample
     * \param mint          Minimum distance to consider
     * \param maxt          Maximum distance to consider
     * \param channel       Channel from which to sample
     * \param state         Mutable tracking state carried through the traversal loop
     * \param func          Callback function called at every segment.
     * \param active        Mask for active lanes
     *
     * \return
     *      The final tracking state, that includes the medium interaction if
     *      a real scattering event was sampled, and the throughput and pdfs
     *      accumulated throughout the traversal.
     */
    virtual TrackingStateType traverse_extremum(
        const Ray3f &ray,
        Float mint,
        Float maxt,
        UInt32 channel,
        TrackingStateType state,
        const TrackingFunctionType &func,
        Mask active = true
    ) const;

    // =============================================================
    //! @{ \name Non-virtual query methods
    // =============================================================

    ScalarBoundingBox3f bbox() const { return m_bbox; }
    //! @}
    // =============================================================

    MI_DECLARE_PLUGIN_BASE_CLASS(Extremum)

protected:
    Extremum();
    Extremum(const Properties &props);

protected:
    /// The bbox over which the extremum structure must be valid.
    ScalarBoundingBox3f m_bbox;
    /// Scale by which to multiply the extremum values.
    ScalarFloat m_scale;
};

MI_EXTERN_CLASS(Extremum)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enables vectorized method calls on Dr.Jit medium arrays
// -----------------------------------------------------------------------

DRJIT_CALL_TEMPLATE_BEGIN(mitsuba::Extremum)
    DRJIT_CALL_METHOD(traverse_extremum)
DRJIT_CALL_END()

//! @}
// -----------------------------------------------------------------------
