#pragma once

#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

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
inline uint32_t sampleTEA32(uint32_t v0, uint32_t v1, int rounds = 4) {
    uint32_t sum = 0;

    for (int i = 0; i < rounds; ++i) {
        sum += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + sum) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + sum) ^ ((v0 >> 5) + 0x7e95761e);
    }

    return v1;
}

/// Vectorized implementation of \ref sampleTEA32()
inline UInt32Packet sampleTEA32(UInt32Packet v0, UInt32Packet v1, int rounds = 4) {
    UInt32Packet sum = UInt32Packet::Zero();
    using simd::sli;
    using simd::sri;

    for (int i = 0; i < rounds; ++i) {
        sum += 0x9e3779b9;
        v0 += (sli<4>(v1) + 0xa341316c) ^ (v1 + sum) ^ (sri<5>(v1) + 0xc8013ea4);
        v1 += (sli<4>(v0) + 0xad90777d) ^ (v0 + sum) ^ (sri<5>(v0) + 0x7e95761e);
    }

    return v1;
}

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
inline uint64_t sampleTEA64(uint32_t v0, uint32_t v1, int rounds = 4) {
    uint32_t sum = 0;

    for (int i = 0; i < rounds; ++i) {
        sum += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + sum) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + sum) ^ ((v0 >> 5) + 0x7e95761e);
    }

    return ((uint64_t) v1 << 32) + v0;
}

/// Vectorized implementation of \ref sampleTEA64()
inline UInt64Packet sampleTEA64(UInt32Packet v0, UInt32Packet v1, int rounds = 4) {
    UInt32Packet sum = UInt32Packet::Zero();
    using simd::sli;
    using simd::sri;
    using simd::cast;

    for (int i = 0; i < rounds; ++i) {
        sum += 0x9e3779b9;
        v0 += (sli<4>(v1) + 0xa341316c) ^ (v1 + sum) ^ (sri<5>(v1) + 0xc8013ea4);
        v1 += (sli<4>(v0) + 0xad90777d) ^ (v0 + sum) ^ (sri<5>(v0) + 0x7e95761e);
    }

    return cast<UInt64Packet>(v0) + sli<32>(cast<UInt64Packet>(v1));
}

/**
 * \brief Generate fast and reasonably good pseudorandom numbers using the
 * Tiny Encryption Algorithm (TEA) by David Wheeler and Roger Needham.
 *
 * This function uses \ref sampleTEA to return single precision floating point
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
inline float sampleTEASingle(uint32_t v0, uint32_t v1, int rounds = 4) {
    /* Trick from MTGP: generate an uniformly distributed
       single precision number in [1,2) and subtract 1. */
    return memcpy_cast<float>((sampleTEA32(v0, v1, rounds) >> 9) |
                              0x3f800000u) - 1.0f;
}

/// Vectorized implementation of sampleTEASingle
inline Float32Packet sampleTEASingle(UInt32Packet v0, UInt32Packet v1, int rounds = 4) {
    return Float32Packet(simd::sri<9>(sampleTEA32(v0, v1, rounds)) |
                         UInt32Packet::Constant(0x3f800000)) - 1.f;
}

/**
 * \brief Generate fast and reasonably good pseudorandom numbers using the
 * Tiny Encryption Algorithm (TEA) by David Wheeler and Roger Needham.
 *
 * This function uses \ref sampleTEA to return single precision floating point
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
inline double sampleTEADouble(uint32_t v0, uint32_t v1, int rounds = 4) {
    /* Trick from MTGP: generate an uniformly distributed
       single precision number in [1,2) and subtract 1. */
    return memcpy_cast<double>((sampleTEA64(v0, v1, rounds) >> 12) |
                               0x3ff0000000000000ULL) - 1.0;
}

/// Vectorized implementation of sampleTEADouble
inline Float64Packet sampleTEADouble(UInt32Packet v0, UInt32Packet v1, int rounds = 4) {
    return Float64Packet(simd::sri<12>(sampleTEA64(v0, v1, rounds)) |
                         UInt64Packet::Constant(0x3ff0000000000000ULL)) - 1.0;
}

#if defined(SINGLE_PRECISION)
/// Alias to \ref sampleTEASingle or \ref sampleTEADouble based on compilation flags
inline Float sampleTEAFloat(uint32_t v0, uint32_t v1, int rounds = 4) {
    return sampleTEASingle(v0, v1, rounds);
}
inline FloatPacket sampleTEAFloat(UInt32Packet v0, UInt32Packet v1, int rounds = 4) {
    return sampleTEASingle(v0, v1, rounds);
}
#else
/// Alias to \ref sampleTEASingle or \ref sampleTEADouble based on compilation flags
inline Float sampleTEAFloat(uint32_t v0, uint32_t v1, int rounds = 4) {
    return sampleTEADouble(v0, v1, rounds);
}
inline FloatPacket sampleTEAFloat(UInt32Packet v0, UInt32Packet v1, int rounds = 4) {
    return sampleTEADouble(v0, v1, rounds);
}
#endif

NAMESPACE_END(mitsuba)
