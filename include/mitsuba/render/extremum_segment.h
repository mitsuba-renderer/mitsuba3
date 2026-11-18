#pragma once

#include <mitsuba/core/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Stores the extremum (minorant/majorant) data for a ray segment.
 *
 * Used as the output type of Extremum traversal. Tracks the
 * segment's entry/exit distances and the local extinction coefficient
 * bounds within that interval.
 */
template<typename Float, typename Spectrum>
struct ExtremumSegment {
    MI_IMPORT_CORE_TYPES()                                                                         \

    /// Segment entry distance along ray
    Float mint;
    /// Segment exit distance along ray
    Float maxt;
    /// Extremum data stored as [minorant, majorant]
    Vector2f value;

    /// Default constructor — creates an invalid segment via reset()
    ExtremumSegment(){ reset(); };

    /**
     * \brief Construct from entry/exit distances and a combined extremum vector.
     *
     * \param mint    Segment entry distance
     * \param maxt    Segment exit distance
     * \param value   Extremum vector [minorant, majorant]
     */
    ExtremumSegment(
        Float mint,
        Float maxt,
        Vector2f value
    ) : mint(mint),
        maxt(maxt),
        value(value) {}

    /**
     * \brief Construct from entry/exit distances and separate minorant/majorant values.
     *
     * \param mint      Segment entry distance
     * \param maxt      Segment exit distance
     * \param minorant  Lower extinction bound over the segment
     * \param majorant  Upper extinction bound over the segment
     */
    ExtremumSegment(
        const Float& mint,
        const Float& maxt,
        const Float& minorant,
        const Float& majorant
    ) : mint(mint),
        maxt(maxt),
        value(Vector2f(minorant, majorant)) {}

    /**
     * This callback method is invoked by dr::zeros<>, and takes care of fields
     * that deviate from the standard zero-initialization convention. In
     * ExtremumSegment, the ``mint`` and ``maxt`` fields are set to  + and -
     * infinity respectively to to mark invalid intersection records.
     */
    void zero_(size_t size = 1) {
        mint        = dr::full<Float>(dr::Infinity<Float>, size);
        maxt        = dr::full<Float>(-dr::Infinity<Float>, size);
        value       = dr::zeros<Vector2f>(size);
    }

    /**
     * \brief Mark the extremum segment as invalid.
     *
     * This operation sets segment's minimum
     * and maximum distances to \f$\infty\f$ and \f$-\infty\f$,
     * respectively.
     */
    void reset() {
        mint = dr::Infinity<Float>;
        maxt = -dr::Infinity<Float>;
    }

    /// Minorant value over the segment. Accessor to the first element of ``value``.
    MI_INLINE Float minorant() const {
        return value.x();
    }

    /// Majorant value over the segment. Accessor to the second element of ``value``.
    MI_INLINE Float majorant() const {
        return value.y();
    }

    DRJIT_ARRAY_DEFAULTS(ExtremumSegment)
    DRJIT_TRAVERSE(ExtremumSegment, mint, maxt, value)
};

NAMESPACE_END(mitsuba)
