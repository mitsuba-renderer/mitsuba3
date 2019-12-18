#include <mitsuba/core/qmc.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/random.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/timer.h>

NAMESPACE_BEGIN(mitsuba)
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

RadicalInverse::RadicalInverse(size_t max_base, int scramble) : m_scramble(scramble) {
    static_assert(sizeof(PrimeBase) == 16, "Base data structure is not packed!");

    Timer timer;
    auto primes = detail::sieve(max_base);
    Assert(primes.size() > 0);
    m_base = std::unique_ptr<PrimeBase[]>(new PrimeBase[primes.size()]);
    m_base_count = primes.size();

    Log(Debug, "Precomputing inverses for %i bases (%s)", m_base_count,
        util::mem_string(sizeof(PrimeBase) * primes.size()));

    for (size_t i = 0; i < primes.size(); ++i) {
        PrimeBase &d = m_base[i];
        uint64_t value = primes[i];
        d.value = (uint16_t) value;
        d.recip = 1.f / (float) value;
        d.divisor = enoki::divisor<uint64_t>(value);
    }

    /* Compute the size of the final permutation table (corresponding to primes) */
    size_t final_size = 0;
    for (size_t i = 0; i < m_base_count; ++i)
        final_size += m_base[i].value;
    final_size += 3; /* Padding for 64bit gather operations */

    /* Allocate memory for them */
    m_permutation_storage = std::unique_ptr<uint16_t[]>(new uint16_t[final_size]);
    m_permutations = std::unique_ptr<uint16_t*[]>(new uint16_t*[m_base_count]);

    /* Check whether Faure or random permutations were requested */
    if (scramble == -1) {
        /* Efficiently compute all Faure permutations using dynamic programming */
        uint16_t initial_bases = m_base[m_base_count - 1].value;
        size_t initial_size =
            ((size_t) initial_bases * (size_t) (initial_bases + 1)) / 2;

        uint16_t *initial_permutation_storage = new uint16_t[initial_size];
        uint16_t **initial_perm = new uint16_t *[initial_bases + 1],
                 *ptr = initial_permutation_storage;

        Log(Debug, "Constructing Faure permutations using %s of memory",
            util::mem_string(initial_size * sizeof(uint16_t)));

        initial_perm[0] = NULL;
        for (size_t i = 1; i <= initial_bases; ++i) {
            initial_perm[i] = ptr;
            ptr += i;
        }
        compute_faure_permutations(initial_bases, initial_perm);

        Log(Debug, "Compactifying permutations to %s of memory",
            util::mem_string(final_size * sizeof(uint16_t)));

        ptr = m_permutation_storage.get();
        for (size_t i = 0; i < m_base_count; ++i) {
            size_t prime = m_base[i].value;
            memcpy(ptr, initial_perm[prime], prime * sizeof(uint16_t));
            m_permutations[i] = ptr; ptr += prime;
        }

        delete[] initial_permutation_storage;
        delete[] initial_perm;
    } else {
        Log(Debug, "Generating random permutations for the seed value = %i", scramble);

        uint16_t *ptr = m_permutation_storage.get();
        PCG32<uint32_t> p((uint64_t) scramble);
        for (size_t i = 0; i < m_base_count; ++i) {
            int prime = m_base[i].value;
            for (int j=0; j<prime; ++j)
                ptr[j] = (uint16_t) j;
            p.shuffle(ptr, ptr + prime);
            m_permutations[i] = ptr;  ptr += prime;
        }
    }
    Log(Debug, "Done (took %s)", util::time_string(timer.value()));

    /* Invert the first two permutations */
    m_inv_permutation_storage = std::unique_ptr<uint16_t[]>(new uint16_t[5]);
    m_inv_permutations = std::unique_ptr<uint16_t*[]>(new uint16_t*[2]);
    m_inv_permutations[0] = m_inv_permutation_storage.get();
    m_inv_permutations[1] = m_inv_permutation_storage.get() + 2;
    invert_permutation(0);
    invert_permutation(1);
}

size_t RadicalInverse::base(size_t index) const {
    if (unlikely(index >= 1024))
        Throw("RadicalInverse::base(): out of bounds");
    return (size_t) m_base[index].value;
}

/**
 * \ref Compute the Faure permutations using dynamic programming
 *
 * For reference, see "Good permutations for extreme discrepancy"
 * by Henri Faure, Journal of Number Theory, Vol. 42, 1, 1992.
 */
void RadicalInverse::compute_faure_permutations(uint32_t max_base, uint16_t **perm) {
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

void RadicalInverse::invert_permutation(uint32_t i) {
    uint16_t *perm    = m_permutations[i],
             *inv_perm = m_inv_permutations[i];
    for (size_t j = 0; j < m_base[i].value; ++j)
        inv_perm[perm[j]] = (uint16_t) j;
}

std::string RadicalInverse::to_string() const {
    return tfm::format("RadicalInverse[base_count=%i, scramble=%i]",
                       m_base_count, m_scramble);
}

MTS_IMPLEMENT_CLASS(RadicalInverse, Object)

NAMESPACE_END(mitsuba)
