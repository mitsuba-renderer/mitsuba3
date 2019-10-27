#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/core/simd.h>
#include <memory>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Efficient implementation of a radical inverse function with prime
 * bases including scrambled versions.
 *
 * This class is used to implement Halton and Hammersley sequences for QMC
 * integration in Mitsuba.
 */
class MTS_EXPORT_CORE RadicalInverse : public Object {
public:
    MTS_REGISTER_CLASS(RadicalInverse, Object)

    /**
     * \brief Precompute data structures that are used to evaluate the radical
     * inverse and scrambled radical inverse function
     *
     * \param max_base
     *    Sets the value of the largest prime number base. The default
     *    interval [2, 8161] contains exactly 1024 prime bases.
     *
     * \param scramble
     *    Selects the desired permutation type, where <tt>-1</tt> denotes the
     *    Faure permutations; any other number causes a pseudorandom permutation
     *    to be built seeded by the value of \c scramble.
     */
    RadicalInverse(size_t max_base = 8161, int scramble = -1);

    /// Return the number of prime bases for which precomputed tables are available
    size_t bases() const { return m_base_count; }

    /**
     * \brief Returns the n-th prime base used by the sequence
     *
     * These prime numbers are used as bases in the radical inverse
     * function implementation.
     */
    size_t base(size_t index) const;

    /// Return the original scramble value
    int scramble() const { return m_scramble; }

    /**
     * \brief Calculate the radical inverse function
     *
     * This function is used as a building block to construct Halton and Hammersley
     * sequences. Roughly, it computes a b-ary representation of the input value
     * \c index, mirrors it along the decimal point, and returns the resulting
     * fractional value. The implementation here uses prime numbers for \c b.
     *
     * \param base_index
     *     Selects the n-th prime that is used as a base when computing the radical
     *     inverse function (0 corresponds to 2, 1->3, 2->5, etc.). The value
     *     specified here must be between 0 and 1023.
     *
     * \param index
     *     Denotes the index that should be mapped through the radical inverse
     *     function
     */
    Float eval(size_t base_index, uint64_t index) const;

    /// Vectorized implementation of \ref eval()
    FloatP eval(size_t base_index, UInt64P index) const;

    /**
     * \brief Calculate a scrambled radical inverse function
     *
     * This function is used as a building block to construct permuted
     * Halton and Hammersley sequence variants. It works like the normal
     * radical inverse function \ref eval(), except that every digit
     * is run through an extra scrambling permutation.
     */
    Float eval_scrambled(size_t base_index, uint64_t index) const;

    /// Vectorized implementation of \ref eval_scrambled()
    FloatP eval_scrambled(size_t base_index, UInt64P index) const;

    /// Return the permutation corresponding to the given prime number basis
    uint16_t *permutation(size_t basis) const {
        return m_permutations[basis];
    }

    /// Return the inverse permutation corresponding to the given prime number basis
    uint16_t *inverse_permutation(size_t basis) const {
        return m_inv_permutations[basis];
    }

    /// Return a human-readable string representation
    virtual std::string to_string() const override;
private:
    /**
     * \ref Compute the Faure permutations using dynamic programming
     *
     * For reference, see "Good permutations for extreme discrepancy"
     * by Henri Faure, Journal of Number Theory, Vol. 42, 1, 1992.
     */
    void compute_faure_permutations(uint32_t max_base, uint16_t **perm);

    /// Invert one of the permutations
    void invert_permutation(uint32_t i);

protected:
    virtual ~RadicalInverse() = default;

private:
#if defined(_MSC_VER)
#  pragma pack(push, 1)
#endif

    /* Precomputed magic constants for efficient division by a constant.
       One entry for each of the first 1024 prime numbers -- 16 KiB of data */
    struct PrimeBase {
        enoki::divisor<uint64_t> divisor;
        uint8_t unused;
        uint16_t value;
        float recip;
    } ENOKI_PACK;

#if defined(_MSC_VER)
#  pragma pack(pop)
#endif

    size_t m_base_count = 0;
    std::unique_ptr<PrimeBase[]> m_base;
    std::unique_ptr<uint16_t[]> m_permutation_storage;
    std::unique_ptr<uint16_t*[]> m_permutations;
    std::unique_ptr<uint16_t[]> m_inv_permutation_storage;
    std::unique_ptr<uint16_t*[]> m_inv_permutations;
    int m_scramble;
};

NAMESPACE_END(mitsuba)
