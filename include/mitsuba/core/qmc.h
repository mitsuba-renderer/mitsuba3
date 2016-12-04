#pragma once

#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(qmc)

/**
 * \brief Calculate the radical inverse function
 *
 * This function is used as a building block to construct Halton and Hammersley
 * sequences. Roughly, it computes a b-ary representation of the input value
 * \c index, mirrors it along the decimal point, and returns the resulting
 * fractional value. The implementation here uses prime numbers for 'b'.
 *
 * \param primeBase
 *     Selects the n-th prime that is used as a base when computing the radical
 *     inverse function (0 corresponds to 2, 1->3, 2->5, etc.). The value
 *     specified here must be between 0 and 1023.
 *
 * \param index
 *     Denotes the index that should be mapped through the radical inverse
 *     function
 */
extern MTS_EXPORT_CORE Float radicalInverse(size_t primeBase, uint64_t index);

/// Vectorized implementation of \ref radicalInverse()
extern MTS_EXPORT_CORE FloatPacket radicalInverse(size_t primeBase,
                                                  UInt64Packet index);

NAMESPACE_END(qmc)
NAMESPACE_END(mitsuba)
