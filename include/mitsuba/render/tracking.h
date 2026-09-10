#pragma once

#include <mitsuba/render/fwd.h>
#include <mitsuba/render/extremum_segment.h>
#include <mitsuba/render/interaction.h>
#include <drjit/random.h>
#include <functional>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief State carried through extremum traversal to accumulate the throughput
 * and its PDF.
 *
 * Can be used to for delta tracking, ratio tracking, and residual ratio tracking.
 * Note: Since the number of required dimensions is different for all pixel
 * samples, ``rng`` is used to sample distances and event types.
 *
 */
template< typename Float, typename Spectrum >
struct TrackingState {
    MI_IMPORT_TYPES()

    Ray3f ray;
    dr::PCG32<UInt64> rng;
    MediumInteraction3f mei;
    Float target_ot;
    Mask has_spectral_extinction;

    // Note that ``throughput`` is shared between algorithm and should be accumulated
    // accordingly. If used for volpathmis, new members and data types will need to
    // be introduced.
    UnpolarizedSpectrum throughput;

    DRJIT_STRUCT(TrackingState, ray, rng, mei, target_ot, \
        has_spectral_extinction, throughput)
};

/**
 * \brief Signature of the tracking function callback accepted by
 * ``Extremum::traverse_extremum``.
 *
 * \param segment
 *      An extremum segment along a ray.
 * \param state
 *      Pointer to the tracking state that holds interaction information and
 *      accumulates throughput and pdfs.
 * \param channel
 *      The channel to use for sampling.
 * \param active
 *      Represents the active lanes.
 *
 *
 * \return A pair (advance, active):
 *      advance:    If true, tracking has exited the segment and requires a
 *                  new one. If false, repeat the loop with the same segment.
 *      active:     Represent active lanes. Lanes that have sampled a real
 *                  interaction or terminated for other reasons will return
 *                  ``false``, prompting the termination of the traversal.
 */
template< typename Float, typename Spectrum >
using TrackingFunction = std::function<
    std::pair<dr::mask_t<Float>, dr::mask_t<Float>>(
    const ExtremumSegment<Float, Spectrum>& /*segment*/,
    TrackingState<Float, Spectrum>* /*state*/,
    const dr::uint32_array_t<Float>& /*channel*/,
    dr::mask_t<Float> /*active*/
)>;

/// Helper function to index the channel of an ``UnpolarizedSpectrum``.
template< typename Float, typename Spectrum >
MI_INLINE
Float index_spectrum(
    const unpolarized_spectrum_t<Spectrum> &spec,
    const dr::uint32_array_t<Float> &idx
) {
    Float m = spec[0];
    if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
        dr::masked(m, idx == 1u) = spec[1];
        dr::masked(m, idx == 2u) = spec[2];
    } else {
        DRJIT_MARK_USED(idx);
    }
    return m;
}

NAMESPACE_END(mitsuba)
