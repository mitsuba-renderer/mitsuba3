#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/volume.h>
#include <mitsuba/render/extremum_segment.h>

#include <optional>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Traversal state carried between successive \c next calls of an
 * \c Extremum.
 *
 * Holds what a structure cannot cheaply recover per segment: local ray
 * information, current visited index and traversal distance. The traversal
 * is over once mint >= maxt.
 *
 * Ray information are stored as origin and direction to keep the struct lean.
 */
template <typename Float, typename Spectrum>
struct ExtremumState {
    MI_IMPORT_CORE_TYPES()

    /// Ray origin in structure-local coordinates.
    Point3f o;

    /// Ray direction in structure-local coordinates, never renormalised.
    Vector3f d;

    /// Current cell index.
    Vector3i pi;

    /// Entry distance of the next segment. Exact in \c t.
    Float mint;

    /// End of the traversal range.
    Float maxt;

    DRJIT_STRUCT(ExtremumState, o, d, pi, mint, maxt)
};

/**
 * \brief Abstract base class for an extremum
 *
 * Extremum provides an interface for spatial data structures that
 * store local extrema (majorant/minorant) of volumetric extinction coefficients.
 * This enables efficient use of tracking algorithms with locally-adaptive
 * majorants and minorants.
 *
 * Extrema follow the pattern of an iterator: the traversal is initialized
 * with ``begin``, and segments are served in order with ``next``.
 *
 * The extremum structure needs to be built using the ``update_extremum``
 * function, it is **not** called automatically in the constructor. The caller,
 * usually a ``Medium``, passes the volumetric data the extremum is derived from.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Extremum : public JitObject<Extremum<Float, Spectrum>> {
public:
    MI_IMPORT_TYPES(Medium, Sampler, Volume)

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
     * structure can be queried. It can be larger or smaller than the underlying
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
     * \brief Set up a stateful traversal along \c ray.
     *
     * Transforms the ray to structure-local coordinates, locates the entry
     * cell and clips <tt>[mint, maxt]</tt> to the structure's domain. Called
     * once per ray; the returned state is then advanced with ``next``
     * until <tt>state.mint >= state.maxt</tt>.
     *
     * \param ray   The ray along which the structure is traversed.
     * \param mint  The minimum traversal distance.
     * \param maxt  The maximum traversal distance.
     *
     * \return      The initalized ``ExtremumState``. Call ``next`` to get the
     *              first and subsequent segment starting from mint.
     *
     * Not a vcall method, called on scalar pointers only.
     */
    virtual ExtremumState begin(const Ray3f &ray, Float mint, Float maxt,
                                       Mask active = true) const = 0;

    /**
     * \brief Return the segment starting at <tt>state.mint</tt> and advance
     * \c state past it.
     *
     * This function traverses the Extremum structure, serving Extremum
     * segments, and advancing the \c state, much like an iterator. The
     * traversal is over when <tt>state.mint >= state.maxt</tt>.
     *
     * \param state State that tracks traversal through the structure. Should
     *              be initialized from ``begin``.
     * \param ray   The world ray ``begin`` was called with.
     *
     * \return      The Exrtemum segment with <tt>segment.mint</tt> the old
     *              <tt>state.mint</tt> and <tt>segment.maxt</tt> the new one.
     *              A structure with internal gaps returns zero-valued segments
     *              for them.
     *
     * Not a vcall method, called on scalar pointers only.
     */
    virtual ExtremumSegment next(ExtremumState &state, const Ray3f &ray,
                                 Mask active = true) const = 0;


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

