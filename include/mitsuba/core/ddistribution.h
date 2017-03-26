#pragma once

#include <vector>
#include <ostream>

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/string.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Discrete probability distribution
 *
 * This data structure can be used to transform uniformly distributed
 * samples to a stored discrete probability distribution.
 *
 * \ingroup libcore
 */
class MTS_EXPORT_CORE DiscreteDistribution {
public:
    /// Allocate memory for a distribution with the given number of entries
    explicit DiscreteDistribution(size_t n_entries = 0) {
        reserve(n_entries);
        clear();
    }

    /// Clear all entries
    void clear() {
        m_cdf.clear();
        m_cdf.push_back(0.0f);
        m_normalized = false;
        m_sum = std::numeric_limits<Float>::quiet_NaN();
        m_normalization = std::numeric_limits<Float>::quiet_NaN();
        m_range_start = 0;
        m_range_end = 0;
    }

    /// Reserve memory for a certain number of entries
    void reserve(size_t n_entries) {
        m_cdf.reserve(n_entries + 1);
    }

    /// Append an entry with the specified discrete probability.
    /// Must be non-negative.
    void append(Float pdf_value) {
        Assert(pdf_value >= 0, "PDF values added to the distribution must be non-negative.");
        if (pdf_value > 0) {
            m_range_end = m_cdf.size() + 1;
            if (m_cdf.back() <= 0) {
                // This is the first positive value we see, adjust the range.
                m_range_start = m_cdf.size() - 1;
            }
        }
        m_cdf.push_back(m_cdf.back() + pdf_value);
    }

    /// Return the number of entries so far
    size_t size() const {
        return m_cdf.size() - 1;
    }

    /// Access an entry by its index
    template <typename Index, typename Scalar = like_t<Index, Float>>
    Scalar operator[](Index entry) const {
        return gather<Scalar>(m_cdf.data(), entry + 1) -
               gather<Scalar>(m_cdf.data(), entry);
    }

    /// Have the probability densities been normalized?
    bool normalized() const {
        return m_normalized;
    }

    /**
     * \brief Return the original (unnormalized) sum of all PDF entries
     *
     * This assumes that \ref normalize() has previously been called.
     */
    Float sum() const {
        return m_sum;
    }

    /**
     * \brief Return the normalization factor (i.e. the inverse of \ref get_sum())
     *
     * This assumes that \ref normalize() has previously been called
     */
    Float normalization() const {
        return m_normalization;
    }

    /**
     * \brief Return the cdf entries.
     *
     * Note that if n values have been appended, there will be (n+1) entries
     * in this CDF (the first one being 0).
     */
    const std::vector<Float>& cdf() const {
        return m_cdf;
    }

    /**
     * \brief Normalize the distribution
     *
     * Throws when the distribution is empty.
     *
     * \return Sum of the (previously unnormalized) entries
     */
    Float normalize() {
        Assert(size() >= 1, "The CDF had no entry to normalize.");
        m_sum = m_cdf.back();
        if (likely(m_sum > 0)) {
            m_normalization = 1.0f / m_sum;
            for (size_t i = 1; i < m_cdf.size(); ++i) {
                m_cdf[i] *= m_normalization;
            }
            m_cdf[m_cdf.size() - 1] = 1.0f;
            m_normalized = true;
        } else {
            m_normalization = 0.0f;
            m_range_start = 0;
            m_range_end = 0;
        }
        return m_sum;
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored distribution
     *
     * \param[in] sample_value
     *     A uniformly distributed sample on [0,1]
     * \return
     *     The discrete index associated with the sample
     */
    template <typename Scalar, typename Index = size_array_t<Scalar>>
    Index sample(Scalar sample_value) const {
        // Note that the search range is adjusted to exclude entries at the
        // beginning and end of the distribution that have probability 0.0.
        // Otherwise we might return a indices that have no probability mass (in
        // particular for sample_value = 0 or 1).
        return math::find_interval(
            m_range_start, m_range_end,
            [this, &sample_value](Index indices) {
               return gather<Scalar>(m_cdf.data(), indices) <= sample_value;
            });
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored distribution
     *
     * \param[in] sample_value
     *     A uniformly distributed sample on [0,1]
     * \return
     *     A pair with (the discrete index associated with the sample, probability
     *     value of the sample).
     */
    template <typename Scalar, typename Index = size_array_t<Scalar>>
    std::pair<Index, Scalar> sample_pdf(Scalar sample_value) const {
        Index index = sample(sample_value);
        return std::make_pair(index, operator[](index));
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored distribution
     *
     * The original sample is value adjusted so that it can be "reused".
     *
     * \param[in,out] sample_value
     *     A uniformly distributed sample on [0,1]
     * \return
     *     The discrete index associated with the sample
     *
     * \note In the Python API, the rescaled sample value is returned in second position.
     */
    template <typename Scalar, typename Index = size_array_t<Scalar>>
    Index sample_reuse(Scalar *sample_value) const {
        const auto s = *sample_value;
        const auto index = sample(s);
        const auto cdf_value = gather<Scalar>(m_cdf.data(), index);
        *sample_value = (s - cdf_value) /
                        (gather<Scalar>(m_cdf.data(), index + 1) - cdf_value);
        return index;
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored distribution.
     *
     * The original sample is value adjusted so that it can be "reused".
     *
     * \param[in,out] sample_value
     *     A uniformly distributed sample on [0,1]
     *
     * \return
     *     A pair with (the discrete index associated with the sample, probability
     *     value of the sample).
     * \note In the Python API, the rescaled sample value is returned in third position.
     */
    template <typename Scalar, typename Index = size_array_t<Scalar>>
    std::pair<Index, Scalar> sample_reuse_pdf(Scalar *sample_value) const {
        const auto s = *sample_value;
        const auto res = sample_pdf(s);
        const auto cdf_value = gather<Scalar>(m_cdf.data(), res.first);
        *sample_value = (s - cdf_value) /
                        (gather<Scalar>(m_cdf.data(), res.first + 1) - cdf_value);
        return res;
    }

private:
    std::vector<Float> m_cdf;
    Float m_sum, m_normalization;
    bool m_normalized;
    // Index in m_cdf corresponding to the first entry with positive probability.
    size_t m_range_start;
    // 1 + the last index of m_cdf with positive probability, or 0 when there is none.
    size_t m_range_end;
};

// Print the string representation of the discrete distribution.
extern MTS_EXPORT_CORE std::ostream &operator<<(std::ostream &os,
                                                const DiscreteDistribution &distribution) {
    os << "DiscreteDistribution[sum=" << distribution.sum() << ", normalized="
       << static_cast<int>(distribution.normalized())
       << ", cdf={" << distribution.cdf() << "}]";
    return os;
}

NAMESPACE_END(mitsuba)
