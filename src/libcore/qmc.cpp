#include <mitsuba/core/qmc.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/timer.h>
#include <tbb/tbb.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(qmc)

/* Precomputed magic constants for efficient division by a constant.
   One entry for each of the first 1024 prime numbers -- 16 KiB of data */
struct PrimeDivisor {
    enoki::divisor<uint64_t> divisor;
    uint8_t unused;
    uint16_t value;
    float recip;
};

NAMESPACE_BEGIN(detail)

template <typename T> std::vector<T> sieve(T n) {
    std::vector<bool> sieve(n + 1, true);

    for (T i = 2, bound = std::sqrt(n); i <= bound; i++) {
        if (!sieve[i])
            continue;

        for (size_t j = i * i; j <= n; j += i)
            sieve[j] = false;
    }

    std::vector<T> result;
    result.push_back(2);
    for (T i = 3; i <= n; i += 2) {
        if (!sieve[i])
            continue;
        result.push_back(i);
    }

    return result;
}

NAMESPACE_END(detail)

static std::unique_ptr<PrimeDivisor[], enoki::aligned_deleter> prime_divisors;
static size_t prime_divisor_count = 0;

void staticInitialization() {
    static_assert(sizeof(PrimeDivisor) == 16, "Divisor data structure is not packed!");
    auto primes = detail::sieve<uint64_t>(8161);
    Assert(primes.size() == 1024);

    prime_divisor_count = 1024;
    prime_divisors = std::unique_ptr<PrimeDivisor[], enoki::aligned_deleter>(
        enoki::alloc<PrimeDivisor>(primes.size()));

    for (uint64_t i = 0; i< primes.size(); ++i) {
        PrimeDivisor &d = prime_divisors[i];
        uint64_t value = primes[i];
        d.value = (uint16_t) value;
        d.recip = 1.f / (float) value;
        d.divisor = enoki::divisor<uint64_t>(value);
    }
}

void staticShutdown() {
    prime_divisors.reset();
    prime_divisor_count = 0;
}

size_t prime_base(size_t index) {
    if (unlikely(index >= 1024))
        Throw("prime_base(): out of bounds (prime base too large)");
    return (size_t) prime_divisors[index].value;
}

FloatP radicalInverse(size_t primeBase, UInt64P index) {
    if (unlikely(primeBase >= 1024))
        Throw("radicalInverse(): out of bounds (prime base too large)");
    const PrimeDivisor div = prime_divisors[primeBase];

    UInt64P::Mask mask = neq(index, enoki::zero<UInt64P>());
    UInt64P divisor = UInt64P(div.value);
    UInt64P value = enoki::zero<UInt64P>();
    FloatP factor = FloatP(1);
    FloatP recip = FloatP(div.recip);

    while (any(mask)) {
        UInt64P next = index / div.divisor;
        value = value * divisor;
        auto fmask = reinterpret_array<typename FloatP::Mask>(mask);
        factor = enoki::select(fmask, factor * recip, factor);
        value += (index - next*divisor) & mask;
        index = next;
        mask = neq(index, enoki::zero<UInt64P>());
    }

    return min(
        FloatP(FloatP(value) * factor),
        FloatP(math::OneMinusEpsilon)
    );
}


#if 0


Float radicalInverse(size_t primeBase, uint64_t index) {
    static_assert(sizeof(Divisor) == 16, "Divisor data structure is not packed!");
    if (unlikely(primeBase >= 1024))
        Throw("radicalInverse(): out of bounds (prime base too large)");

    const Divisor div = prime_divisors[primeBase];

    uint64_t value = 0;
    Float factor = 1;

    while (index) {
        uint64_t next  = div(index);
        value = value * div.divisor;
        factor *= div.recip;
        value += index - next*div.divisor;
        index = next;
    }

    return std::min(math::OneMinusEpsilon, (Float) value * factor);
}

FloatP radicalInverse(size_t primeBase, UInt64P index) {
    if (unlikely(primeBase >= 1024))
        Throw("radicalInverse(): out of bounds (prime base too large)");
    const Divisor div = prime_divisors[primeBase];

    UInt64P::Mask mask = neq(index, enoki::zero<UInt64P>());
    UInt64P divisor = UInt64P(div.divisor);
    UInt64P value = enoki::zero<UInt64P>();
    Float64P factor = Float64P(1);
    Float64P recip = Float64P((double) div.recip);

    while (any(mask)) {
        UInt64P next = div(index);
        value = value * divisor;
        factor = enoki::select(Float64P::Mask(mask), factor * recip, factor);
        value += (index - next*divisor) & mask;
        index = next;
        mask = neq(index, enoki::zero<UInt64P>());
    }

    return min(
        FloatP(enoki::cast<Float64P>(value) * factor),
        FloatP(math::OneMinusEpsilon)
    );
}

Float scrambledRadicalInverse(size_t primeBase, uint64_t index, const uint16_t *perm) {
    static_assert(sizeof(Divisor) == 16, "Divisor data structure is not packed!");
    if (unlikely(primeBase >= 1024))
        Throw("radicalInverse(): out of bounds (prime base too large)");

    const Divisor div = prime_divisors[primeBase];

    uint64_t value = 0;
    Float factor = 1;

    while (index) {
        uint64_t next  = div(index);
        value = value * div.divisor;
        factor *= div.recip;
        value += perm[index - next*div.divisor];
        index = next;
    }

    Float correction = div.recip * perm[0] / (1.0f - div.recip);

    return std::min(math::OneMinusEpsilon,
                    factor * ((Float) value + correction));
}

FloatP scrambledRadicalInverse(size_t primeBase, UInt64P index, const uint16_t *perm) {
    static_assert(sizeof(Divisor) == 16, "Divisor data structure is not packed!");
    if (unlikely(primeBase >= 1024))
        Throw("radicalInverse(): out of bounds (prime base too large)");
    const Divisor div = prime_divisors[primeBase];

    UInt64P::Mask mask = index != enoki::zero<UInt64P>();
    UInt64P divisor = UInt64P(div.divisor);
    UInt64P value = enoki::zero<UInt64P>();
    Float64P factor = Float64P(1);
    Float64P recip = Float64P((double) div.recip);

    while (enoki::any(mask)) {
        UInt64P next = div(index);
        value = value * divisor;
        factor = enoki::select(Float64P::Mask(mask), factor * recip, factor);
        UInt64P digit = (index - next*divisor) & mask;
        digit = enoki::gather<UInt64P>(perm, digit) & UInt64P(0xffff);
        value += digit;
        index = next;
        mask = index != enoki::zero<UInt64P>();
    }

    Float64P correction = Float64P((double) (div.recip * perm[0] / (1.0f - div.recip)));

    return enoki::min(
        enoki::cast<FloatP>((enoki::cast<Float64P>(value) + correction) * factor),
        FloatP(math::OneMinusEpsilon)
    );
}
#endif

PermutationStorage::PermutationStorage(int scramble) : m_scramble(scramble) {
    /* Compute the size of the final permutation table (corresponding to primes) */
    size_t final_size = 0;
    for (size_t i=0; i<prime_divisor_count; ++i)
        final_size += prime_divisors[i].value;
    final_size += 3; /* Padding for 64bit gather operations */

    /* Allocate memory for them */
    m_storage = new uint16_t[final_size];
    m_permutations = new uint16_t*[prime_divisor_count];

    /* Check whether Faure or random permutations were requested */
    Timer timer;
    if (scramble == -1) {
        /* Efficiently compute all Faure permutations using dynamic programming */
        uint16_t initial_bases = prime_divisors[prime_divisor_count-1].value;
        size_t initial_size = ((size_t) initial_bases * (size_t) (initial_bases + 1))/2;
        uint16_t *initial_storage = new uint16_t[initial_size];
        uint16_t **initial_perm = new uint16_t*[initial_bases+1],
                 *ptr = initial_storage;

        Log(EDebug, "Constructing Faure permutations using %s of memory",
            util::mem_string(initial_size * sizeof(uint16_t)));

        initial_perm[0] = NULL;
        for (size_t i = 1; i <= initial_bases; ++i) {
            initial_perm[i] = ptr;
            ptr += i;
        }
        compute_faure_permutations(initial_bases, initial_perm);

        Log(EDebug, "Compactifying permutations to %s of memory",
            util::mem_string(final_size * sizeof(uint16_t)));

        ptr = m_storage;
        for (size_t i = 0; i < prime_divisor_count; ++i) {
            size_t prime = prime_divisors[i].value;
            memcpy(ptr, initial_perm[prime], prime * sizeof(uint16_t));
            m_permutations[i] = ptr; ptr += prime;
        }

        delete[] initial_storage;
        delete[] initial_perm;
    } else {
        Log(EDebug, "Generating random permutations for the seed value = %i", scramble);

        uint16_t *ptr = m_storage;
        PCG32 p;
        for (size_t i = 0; i < prime_divisor_count; ++i) {
            int prime = prime_divisors[i].value;
            for (int j=0; j<prime; ++j)
                ptr[j] = (uint16_t) j;
            p.shuffle(ptr, ptr + prime);
            m_permutations[i] = ptr;  ptr += prime;
        }
    }
    Log(EDebug, "Done (took %s)", util::time_string(timer.value()));

    /* Invert the first two permutations */
    m_inv_storage = new uint16_t[5];
    m_inv_permutations = new uint16_t*[2];
    m_inv_permutations[0] = m_inv_storage;
    m_inv_permutations[1] = m_inv_storage + 2;
    invert_permutation(0);
    invert_permutation(1);
}

PermutationStorage::~PermutationStorage()  {
    delete[] m_storage;
    delete[] m_inv_storage;
    delete[] m_permutations;
    delete[] m_inv_permutations;
}

/**
 * \ref Compute the Faure permutations using dynamic programming
 *
 * For reference, see "Good permutations for extreme discrepancy"
 * by Henri Faure, Journal of Number Theory, Vol. 42, 1, 1992.
 */
void PermutationStorage::compute_faure_permutations(uint32_t max_base, uint16_t **perm) {
    Assert(max_base >= 2);

    /* Dimension 1 */
    perm[1][0] = 0;

    /* Dimension 2 */
    perm[2][0] = 0;
    perm[2][1] = 1;

    for (uint32_t b = 2; b <= max_base; ++b) {
        if (b & 1) {
            /* Odd dimension */
            uint16_t c = (b - 1) /2;

            for (uint16_t i=0; i<b; ++i) {
                if (i == c) {
                    perm[b][i] = c;
                } else {
                    uint16_t f = perm[b-1][i - (int) (i>c)];
                    perm[b][i] = f + (int) (f >= c);
                }
            }
        } else {
            /* Even dimension */
            uint16_t c = b / 2;

            for (uint16_t i=0; i<b; ++i)
                perm[b][i] = i < c ? 2 * perm[c][i] : 2 * perm[c][i - c] + 1;
        }
    }
}

void PermutationStorage::invert_permutation(uint32_t i) {
    uint16_t *perm    = m_permutations[i],
             *inv_perm = m_inv_permutations[i];
    for (size_t j = 0; j < prime_divisors[i].value; ++j)
        inv_perm[perm[j]] = j;
}

MTS_IMPLEMENT_CLASS(PermutationStorage, Object)
NAMESPACE_END(qmc)
NAMESPACE_END(mitsuba)
