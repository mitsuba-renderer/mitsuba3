#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/core/simd.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>
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
    template <typename Float, typename UInt64 = uint64_array_t<Float>>
    Float eval(size_t base_index, UInt64 index) const {
        if (unlikely(base_index >= m_base_count))
            Throw("eval(): out of bounds (prime base too large)");

        const PrimeBase base = m_base[base_index];

        UInt64 value(zero<UInt64>()),
                divisor((uint64_t) base.value);
        Float factor = Float(1.f),
            recip(base.recip);

        auto active = neq(index, enoki::zero<UInt64>());

        while (any(active)) {
            auto active_f = reinterpret_array<mask_t<Float>>(active);
            UInt64 next = base.divisor(index);
            masked(factor, active_f) = factor * recip;
            masked(value, active) = (value - next) * divisor + index;
            index = next;
            active = neq(index, enoki::zero<UInt64>());
        }

        return min(math::OneMinusEpsilon<Float>, Float(value) * factor);
    }

    /**
     * \brief Calculate a scrambled radical inverse function
     *
     * This function is used as a building block to construct permuted
     * Halton and Hammersley sequence variants. It works like the normal
     * radical inverse function \ref eval(), except that every digit
     * is run through an extra scrambling permutation.
     */
    template <typename Float, typename UInt64 = uint64_array_t<Float>>
    Float eval_scrambled(size_t base_index, UInt64 index) const {
        if (unlikely(base_index >= m_base_count))
            Throw("eval(): out of bounds (prime base too large)");

        const PrimeBase base = m_base[base_index];
        const uint16_t *perm = m_permutations[base_index];

        UInt64 value(zero<UInt64>()),
                divisor((uint64_t) base.value),
                mask(0xffffu);
        Float factor(1.f),
            recip(base.recip);

        auto active = neq(index, enoki::zero<UInt64>());

        while (any(active)) {
            auto active_f = reinterpret_array<mask_t<Float>>(active);
            UInt64 next = base.divisor(index);
            masked(factor, active_f) = factor * recip;
            UInt64 digit = index - next * divisor;
            masked(value, active) =
                value * divisor + (enoki::gather<UInt64, 2>(perm, digit, active) & mask);
            index = next;
            active = neq(index, enoki::zero<UInt64>());
        }

        Float correction(base.recip * (Float) perm[0] / ((Float) 1 - base.recip));
        return min(math::OneMinusEpsilon<Float>, (Float(value) + correction) * factor);
    }

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

    MTS_DECLARE_CLASS()
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


/// Van der Corput radical inverse in base 2
template <typename UInt, typename Float = float_array_t<UInt>>
Float radical_inverse_2(UInt index, UInt scramble = 0) {
    if constexpr (is_double_v<Float>) {
        index = (index << 32) | (index >> 32);
        index = ((index & 0x0000ffff0000ffffULL) << 16) | ((index & 0xffff0000ffff0000ULL) >> 16);
        index = ((index & 0x00ff00ff00ff00ffULL) << 8)  | ((index & 0xff00ff00ff00ff00ULL) >> 8);
        index = ((index & 0x0f0f0f0f0f0f0f0fULL) << 4)  | ((index & 0xf0f0f0f0f0f0f0f0ULL) >> 4);
        index = ((index & 0x3333333333333333ULL) << 2)  | ((index & 0xccccccccccccccccULL) >> 2);
        index = ((index & 0x5555555555555555ULL) << 1)  | ((index & 0xaaaaaaaaaaaaaaaaULL) >> 1);

        /* Generate an uniformly distributed double precision number in [1,2)
         * from the scrambled index and subtract 1. */
        return reinterpret_array<Float>(sr<12>(index ^ scramble) | 0x3ff0000000000000ull) - 1.0;
    } else {
        index = (index << 16) | (index >> 16);
        index = ((index & 0x00ff00ff) << 8) | ((index & 0xff00ff00) >> 8);
        index = ((index & 0x0f0f0f0f) << 4) | ((index & 0xf0f0f0f0) >> 4);
        index = ((index & 0x33333333) << 2) | ((index & 0xcccccccc) >> 2);
        index = ((index & 0x55555555) << 1) | ((index & 0xaaaaaaaa) >> 1);

        /* Generate an uniformly distributed single precision number in [1,2)
         * from the scrambled index and subtract 1. */
        return reinterpret_array<Float>(sr<9>(index ^ scramble) | 0x3f800000u) - 1.f;
    }
}


/// Sobol' radical inverse in base 2
template <typename UInt, typename Float = float_array_t<UInt>>
Float sobol_2(UInt index, UInt scramble = 0) {
    if constexpr (is_double_v<Float>) {
        for (UInt v = 1ULL << 52; index != 0; index >>= 1, v ^= v >> 1)
            masked(scramble, eq(index & 1U, 1U)) ^= v;
        return reinterpret_array<Float>(sr<12>(scramble) | 0x3ff0000000000000ull) - 1.0;
    } else {
        for (UInt v = 1U << 31; index != 0; index >>= 1, v ^= v >> 1)
            masked(scramble, eq(index & 1U, 1U)) ^= v;
        return Float(scramble) / Float(1ULL << 32);
    }
}

NAMESPACE_END(mitsuba)
