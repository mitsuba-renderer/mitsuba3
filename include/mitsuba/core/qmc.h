#pragma once

#include <mitsuba/core/random.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(qmc)

/**
 * \brief Returns the n-th prime number from a lookup table
 *
 * These prime numbers are used as bases in the radical inverse
 * function implementation. \c index must be less than 1024.
 */
extern MTS_EXPORT_CORE size_t primeBase(size_t index);

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

/**
 * \brief Calculate a scrambled radical inverse function
 *
 * This function is used as a building block to construct permuted
 * Halton and Hammersley sequence variants. It works like the normal
 * radical inverse function \ref radicalInverse(), except that every digit
 * is run through an extra scrambling permutation specified as array
 * of size \c base.
 */
extern MTS_EXPORT_CORE Float scrambledRadicalInverse(size_t primeBase,
                                                     uint64_t index,
                                                     const uint16_t *perm);

/// Vectorized implementation of \ref radicalInverse()
extern MTS_EXPORT_CORE FloatPacket radicalInverse(size_t primeBase,
                                                  UInt64Packet index);

/// Vectorized implementation of \ref scrambledRadicalInverse()
extern MTS_EXPORT_CORE FloatPacket scrambledRadicalInverse(size_t primeBase,
                                                           UInt64Packet index,
                                                           const uint16_t *perm);

/**
 * \brief Stores scrambling permutations for Van Der Corput-type
 * sequences with prime bases.
 */
class MTS_EXPORT_CORE PermutationStorage : public Object {
public:
    /**
     * \brief Create new permutations
     *
     * \param scramble
     *    Selects the desired permutation type, where <tt>-1</tt> denotes the
     *    Faure permutations; any other number causes a pseudorandom permutation
     *    to be built seeded by the value of \c scramble.
     */
    PermutationStorage(int scramble = -1);

    /// Return the permutation corresponding to the given prime number basis
    uint16_t *permutation(size_t basis) const {
        return m_permutations[basis];
    }

    /// Return the inverse permutation corresponding to the given prime number basis
    uint16_t *inversePermutation(size_t basis) const {
        return m_invPermutations[basis];
    }

    /// Return the original scramble value
    int scramble() const { return m_scramble; }

private:
    /**
     * \ref Compute the Faure permutations using dynamic programming
     *
     * For reference, see "Good permutations for extreme discrepancy"
     * by Henri Faure, Journal of Number Theory, Vol. 42, 1, 1992.
     */
    void computeFaurePermutations(uint32_t maxBase, uint16_t **perm);

    /// Invert one of the permutations
    void invertPermutation(uint32_t i);

    MTS_DECLARE_CLASS()
protected:
    virtual ~PermutationStorage();

private:
    uint16_t *m_storage, *m_invStorage;
    uint16_t **m_permutations;
    uint16_t **m_invPermutations;
    int m_scramble;
};

NAMESPACE_END(qmc)
NAMESPACE_END(mitsuba)
