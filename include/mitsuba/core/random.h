/*
 * Tiny self-contained version of the PCG Random Number Generation for C++ put
 * together from pieces of the much larger C/C++ codebase with vectorization
 * using Enoki.
 *
 * Wenzel Jakob, February 2017
 *
 * The PCG random number generator was developed by Melissa O'Neill
 * <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *     http://www.pcg-random.org
 */

#pragma once

#include <mitsuba/core/simd.h>
#include <mitsuba/core/logger.h>

#define PCG32_DEFAULT_STATE  0x853c49e6748fea9bULL
#define PCG32_DEFAULT_STREAM 0xda3e39cb94b95bdbULL
#define PCG32_MULT           0x5851f42d4c957f2dULL

NAMESPACE_BEGIN(mitsuba)

template <typename T> struct TPCG32 {
    /* Some convenient type aliases for vectorization */
    using  Int64     = int64_array_t<T>;
    using UInt64     = uint64_array_t<T>;
    using UInt32     = uint32_array_t<T>;
    using Float64    = float64_array_t<T>;
    using Float32    = float32_array_t<T>;
    using UInt32Mask = mask_t<UInt32>;
    using UInt64Mask = mask_t<UInt64>;

    /// Initialize the pseudorandom number generator with the \ref seed() function
    TPCG32(UInt64 initstate = PCG32_DEFAULT_STATE,
           UInt64 initseq = index_sequence<UInt64>() + PCG32_DEFAULT_STREAM) {
        seed(initstate, initseq);
    }

    /**
     * \brief Seed the pseudorandom number generator
     *
     * Specified in two parts: a state initializer and a sequence selection
     * constant (a.k.a. stream id)
     */
    void seed(UInt64 initstate, UInt64 initseq) {
        state = zero<UInt64>();
        inc = sli<1>(initseq) | 1u;
        next_uint32();
        state += initstate;
        next_uint32();
    }

    /// Generate a uniformly distributed unsigned 32-bit random number
    UInt32 next_uint32() {
        UInt64 oldstate = state;
        state = oldstate * uint64_t(PCG32_MULT) + inc;
        UInt32 xorshifted = UInt32(sri<27>(sri<18>(oldstate) ^ oldstate));
        UInt32 rot_offset = UInt32(sri<59>(oldstate));
        return ror(xorshifted, rot_offset);
    }

    /// Generate a single precision floating point value on the interval [0, 1)
    Float32 next_float32() {
        return reinterpret_array<Float32>(sri<9>(next_uint32()) | 0x3f800000u) - 1.f;
    }

    /**
     * \brief Generate a double precision floating point value on the interval [0, 1)
     *
     * \remark Since the underlying random number generator produces 32 bit output,
     * only the first 32 mantissa bits will be filled (however, the resolution is still
     * finer than in \ref next_float(), which only uses 23 mantissa bits)
     */
    Float64 next_float64() {
        /* Trick from MTGP: generate an uniformly distributed
           double precision number in [1,2) and subtract 1. */
        return reinterpret_array<Float64>(sli<20>(UInt64(next_uint32())) |
                                          0x3ff0000000000000ull) - 1.0;
    }

    auto next_float() {
        #if defined(SINGLE_PRECISION)
            return next_float32();
        #else
            return next_float64();
        #endif
    }

    /// Generate a uniformly distributed number, r, where 0 <= r < bound
    UInt32 next_uint32(uint32_t bound) {
        // To avoid bias, we need to make the range of the RNG a multiple of
        // bound, which we do by dropping output less than a threshold.
        // A naive scheme to calculate the threshold would be to do
        //
        //     UInt32 threshold = 0x100000000ull % bound;
        //
        // but 64-bit div/mod is slower than 32-bit div/mod (especially on
        // 32-bit platforms).  In essence, we do
        //
        //     UInt32 threshold = (0x100000000ull-bound) % bound;
        //
        // because this version will calculate the same modulus, but the LHS
        // value is less than 2^32.

        const UInt32 threshold = (~bound + 1u) % bound;
        UInt32 result;

        if (!enoki::is_array<UInt32>::value) {
            // Uniformity guarantees that this loop will terminate.  In practice, it
            // should usually terminate quickly; on average (assuming all bounds are
            // equally likely), 82.25% of the time, we can expect it to require just
            // one iteration.  In the worst case, someone passes a bound of 2^31 + 1
            // (i.e., 2147483649), which invalidates almost 50% of the range.  In
            // practice, bounds are typically small and only a tiny amount of the range
            // is eliminated.

            while (true) {
                result = next_uint32();

                if (all(result >= threshold))
                    return result % bound;
            }
        } else {
            // Vectorized version of the above code, which additionally keeps
            // track of which SIMD lanes have already finished and stops
            // advancing the associated PRNGs

            UInt32Mask mask(true);
            do {
                result = select(mask, next_uint32_masked(mask), result);
                mask &= result < threshold;
            } while (any(mask));
        }

        return result % bound;
    }

    /**
     * \brief Multi-step advance function (jump-ahead, jump-back)
     *
     * The method used here is based on Brown, "Random Number Generation
     * with Arbitrary Stride", Transactions of the American Nuclear
     * Society (Nov. 1994). The algorithm is very similar to fast
     * exponentiation.
     */
    void advance(Int64 delta_) {
        UInt64 cur_mult = PCG32_MULT,
               cur_plus = inc,
               acc_mult = 1ull,
               acc_plus = 0ull;

        /* Even though delta is an unsigned integer, we can pass a signed
           integer to go backwards, it just goes "the long way round". */
        UInt64 delta(delta_);

        while (delta != zero<UInt64>()) {
            auto mask = neq(delta & UInt64(1), zero<UInt64>());
            acc_mult = select(mask, acc_mult * cur_mult, acc_mult);
            acc_plus = select(mask, acc_plus * cur_mult + cur_plus, acc_plus);
            cur_plus = (cur_mult + UInt64(1)) * cur_plus;
            cur_mult *= cur_mult;
            delta = sri<1>(delta);
        }

        state = acc_mult * state + acc_plus;
    }

    /// Compute the distance between two PCG32 pseudorandom number generators
    Int64 operator-(const TPCG32 &other) const {
        Assert(inc == other.inc);

        UInt64 cur_mult = PCG32_MULT,
               cur_plus = inc,
               cur_state = other.state,
               the_bit = 1ull,
               distance = 0ull;

        while (state != cur_state) {
            auto mask = neq(state & the_bit, cur_state & the_bit);
            cur_state = select(mask, cur_state * cur_mult + cur_plus, cur_state);
            distance = select(mask, distance | the_bit, distance);
            Assert((state & the_bit) == (cur_state & the_bit));
            the_bit = sli<1>(the_bit);
            cur_plus = (cur_mult + UInt64(1)) * cur_plus;
            cur_mult *= cur_mult;
        }

        return Int64(distance);
    }

    /**
     * \brief Draw uniformly distributed permutation and permute the
     * given STL container
     *
     * From: Knuth, TAoCP Vol. 2 (3rd 3d), Section 3.4.2
     */
    template <typename Iterator, typename T2 = T,
              std::enable_if_t<std::is_arithmetic<T2>::value, int> = 0>
    void shuffle(Iterator begin, Iterator end) {
        for (Iterator it = end - 1; it > begin; --it)
            std::iter_swap(it, begin + next_uint32((uint32_t) (it - begin + 1)));
    }

    /// Equality operator
    bool operator==(const TPCG32 &other) const { return state == other.state && inc == other.inc; }

    /// Inequality operator
    bool operator!=(const TPCG32 &other) const { return state != other.state || inc != other.inc; }

    UInt64 state;  // RNG state.  All values are possible.
    UInt64 inc;    // Controls which RNG sequence (stream) is selected. Must *always* be odd.

private:
    /**
     * \brief Generate a uniformly distributed unsigned 32-bit random number
     * \remark Masked version: only activates some of the RNGs
     */
    UInt32 next_uint32_masked(UInt32Mask mask) {
        UInt64 oldstate = state;
        state = select(reinterpret_array<mask_t<UInt64>>(mask),
                       oldstate * uint64_t(PCG32_MULT) + inc, oldstate);
        UInt32 xorshifted = UInt32(sri<27>(sri<18>(oldstate) ^ oldstate));
        UInt32 rot_offset = UInt32(sri<59>(oldstate));
        return ror(xorshifted, rot_offset);
    }
};

MTS_EXPORT_CORE_TEMPLATE TPCG32<uint32_t>;
MTS_EXPORT_CORE_TEMPLATE TPCG32<UInt32P>;

using PCG32  = TPCG32<uint32_t>;
using PCG32P = TPCG32<UInt32P>;

/**
 * \brief Generate fast and reasonably good pseudorandom numbers using the
 * Tiny Encryption Algorithm (TEA) by David Wheeler and Roger Needham.
 *
 * For details, refer to "GPU Random Numbers via the Tiny Encryption Algorithm"
 * by Fahad Zafar, Marc Olano, and Aaron Curtis.
 *
 * \param v0
 *     First input value to be encrypted (could be the sample index)
 * \param v1
 *     Second input value to be encrypted (e.g. the requested random number dimension)
 * \param rounds
 *     How many rounds should be executed? The default for random number
 *     generation is 4.
 * \return
 *     A uniformly distributed 32-bit integer
 */

template <typename UInt32>
UInt32 sample_tea_32(UInt32 v0, UInt32 v1, int rounds = 4) {
    UInt32 sum = 0;

    ENOKI_NOUNROLL for (int i = 0; i < rounds; ++i) {
        sum += 0x9e3779b9;
        v0 += (sli<4>(v1) + 0xa341316c) ^ (v1 + sum) ^ (sri<5>(v1) + 0xc8013ea4);
        v1 += (sli<4>(v0) + 0xad90777d) ^ (v0 + sum) ^ (sri<5>(v0) + 0x7e95761e);
    }

    return v1;
}

extern template MTS_EXPORT_CORE uint32_t sample_tea_32(uint32_t, uint32_t, int);
extern template MTS_EXPORT_CORE UInt32P  sample_tea_32(UInt32P,  UInt32P,  int);

/**
 * \brief Generate fast and reasonably good pseudorandom numbers using the
 * Tiny Encryption Algorithm (TEA) by David Wheeler and Roger Needham.
 *
 * For details, refer to "GPU Random Numbers via the Tiny Encryption Algorithm"
 * by Fahad Zafar, Marc Olano, and Aaron Curtis.
 *
 * \param v0
 *     First input value to be encrypted (could be the sample index)
 * \param v1
 *     Second input value to be encrypted (e.g. the requested random number dimension)
 * \param rounds
 *     How many rounds should be executed? The default for random number
 *     generation is 4.
 * \return
 *     A uniformly distributed 64-bit integer
 */

template <typename UInt32>
uint64_array_t<UInt32> sample_tea_64(UInt32 v0, UInt32 v1, int rounds = 4) {
    UInt32 sum = 0;

    ENOKI_NOUNROLL for (int i = 0; i < rounds; ++i) {
        sum += 0x9e3779b9;
        v0 += (sli<4>(v1) + 0xa341316c) ^ (v1 + sum) ^ (sri<5>(v1) + 0xc8013ea4);
        v1 += (sli<4>(v0) + 0xad90777d) ^ (v0 + sum) ^ (sri<5>(v0) + 0x7e95761e);
    }

    return uint64_array_t<UInt32>(v0) + sli<32>(uint64_array_t<UInt32>(v1));
}

extern template MTS_EXPORT_CORE uint64_array_t<uint32_t> sample_tea_64(uint32_t, uint32_t, int);
extern template MTS_EXPORT_CORE uint64_array_t<UInt32P>  sample_tea_64(UInt32P,  UInt32P,  int);

/**
 * \brief Generate fast and reasonably good pseudorandom numbers using the
 * Tiny Encryption Algorithm (TEA) by David Wheeler and Roger Needham.
 *
 * This function uses \ref sample_tea_ to return single precision floating point
 * numbers on the interval <tt>[0, 1)</tt>
 *
 * \param v0
 *     First input value to be encrypted (could be the sample index)
 * \param v1
 *     Second input value to be encrypted (e.g. the requested random number dimension)
 * \param rounds
 *     How many rounds should be executed? The default for random number
 *     generation is 4.
 * \return
 *     A uniformly distributed floating point number on the interval <tt>[0, 1)</tt>
 */
template <typename UInt32>
float32_array_t<UInt32> sample_tea_float32(UInt32 v0, UInt32 v1, int rounds = 4) {
    return reinterpret_array<float32_array_t<UInt32>>(
        sri<9>(sample_tea_32(v0, v1, rounds)) | 0x3f800000u) - 1.f;
}

extern template MTS_EXPORT_CORE float32_array_t<uint32_t> sample_tea_float32(uint32_t, uint32_t, int);
extern template MTS_EXPORT_CORE float32_array_t<UInt32P>  sample_tea_float32(UInt32P,  UInt32P,  int);

/**
 * \brief Generate fast and reasonably good pseudorandom numbers using the
 * Tiny Encryption Algorithm (TEA) by David Wheeler and Roger Needham.
 *
 * This function uses \ref sample_tea_ to return double precision floating point
 * numbers on the interval <tt>[0, 1)</tt>
 *
 * \param v0
 *     First input value to be encrypted (could be the sample index)
 * \param v1
 *     Second input value to be encrypted (e.g. the requested random number dimension)
 * \param rounds
 *     How many rounds should be executed? The default for random number
 *     generation is 4.
 * \return
 *     A uniformly distributed floating point number on the interval <tt>[0, 1)</tt>
 */

template <typename UInt32>
float64_array_t<UInt32> sample_tea_float64(UInt32 v0, UInt32 v1, int rounds = 4) {
    return reinterpret_array<float64_array_t<UInt32>>(
        sri<12>(sample_tea_64(v0, v1, rounds)) | 0x3ff0000000000000ull) - 1.0;
}

extern template MTS_EXPORT_CORE float64_array_t<uint32_t> sample_tea_float64(uint32_t, uint32_t, int);
extern template MTS_EXPORT_CORE float64_array_t<UInt32P>  sample_tea_float64(UInt32P,  UInt32P,  int);

/// Alias to \ref sample_tea_float32 or \ref sample_tea_float64 based on compilation flags
template <typename UInt32>
auto sample_tea_float(UInt32 v0, UInt32 v1, int rounds = 4) {
    #if defined(SINGLE_PRECISION)
        return sample_tea_float32(v0, v1, rounds);
    #else
        return sample_tea_float64(v0, v1, rounds);
    #endif
}

NAMESPACE_END(mitsuba)
