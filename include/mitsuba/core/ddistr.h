#pragma once

#include <vector>
#include <ostream>

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Discrete probability distribution
 *
 * This data structure can be used to transform uniformly distributed
 * samples to a stored discrete probability distribution.
 */
template <typename Float_>
class MTS_EXPORT_CORE DiscreteDistribution {
    using Float = Float_;
    using ScalarFloat = scalar_t<Float>;
    using FloatStorage = host_vector<Float>;

public:
    /// Reserve memory for a distribution with the given number of entries
    explicit DiscreteDistribution(size_t n_entries = 0) {
        reserve(n_entries);
        clear();
    }

    /// Initialize the distribution with the given data
    DiscreteDistribution(size_t size, const ScalarFloat *values)
        : DiscreteDistribution(size) {
        for (size_t i = 0; i < size; ++i)
            append(values[i]);
        normalize();
    }

    /// Clear all entries
    void clear() {
        m_cdf.clear();
        m_cdf.push_back(0.f);
        m_normalized = false;
        m_sum = 0.0;
        m_normalization = std::numeric_limits<ScalarFloat>::quiet_NaN();
        m_range_start = 0;
        m_range_end = 0;
    }

    /// Reserve memory for a certain number of entries
    void reserve(size_t n_entries) {
        m_cdf.reserve(n_entries + 1);
    }

    /**
     * \brief Append an entry with the specified discrete probability.
     *
     * \remark \c pdf_value must be non-negative.
     */
    void append(ScalarFloat pdf_value) {
        Assert(pdf_value >= 0, "PDF values added to the distribution must be non-negative.");
        m_sum += double(pdf_value);

        // Maintain indices into m_cdf to trim zero-valued pre- and suffixes
        if (pdf_value > 0)
            m_range_end = uint32_t(m_cdf.size() + 1);
        if (m_sum == 0.0)
            m_range_start = uint32_t(m_cdf.size());

        m_cdf.push_back(ScalarFloat(m_sum));
    }

    /// Return the number of entries so far
    size_t size() const {
        return m_cdf.size() - 1;
    }

    /// Access an entry by its index
    template <typename Index, typename Value = float_array_t<Index>>
    Value eval(Index entry, mask_t<Index> active = true) const {
        return gather<Value>(m_cdf.data(), entry + 1, active) -
               gather<Value>(m_cdf.data(), entry, active);
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
    ScalarFloat sum() const {
        return m_sum;
    }

    /**
     * \brief Return the normalization factor (i.e. the inverse of \ref get_sum())
     *
     * This assumes that \ref normalize() has previously been called
     */
    ScalarFloat normalization() const {
        return m_normalization;
    }

    /**
     * \brief Return the cdf entries.
     *
     * Note that if n values have been appended, there will be (n+1) entries
     * in this CDF (the first one being 0).
     */
    const FloatStorage& cdf() const { return m_cdf; }

    /**
     * \brief Normalize the distribution
     *
     * Throws an exception when the distribution contains no elements. The
     * distribution is not considered to be normalized if the sum of
     * probabilities equals zero.
     *
     * \return Sum of the (previously unnormalized) entries
     */
    ScalarFloat normalize() {
        Assert(size() >= 1, "Attempted to normalize an empty distribution!");

        if (likely(m_sum > 0)) {
            m_normalization = ScalarFloat(1.0 / m_sum);
            for (size_t i = 1; i < m_cdf.size(); ++i)
                m_cdf[i] *= m_normalization;
            m_cdf[m_cdf.size() - 1] = 1.f;
            m_normalized = true;
        } else {
            m_normalization = std::numeric_limits<ScalarFloat>::infinity();
            m_range_start = 0;
            m_range_end = 0;
            m_normalized = false;
        }

        return ScalarFloat(m_sum);
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored distribution
     *
     * \param[in] sample_value
     *     A uniformly distributed sample on [0,1]
     * \return
     *     The discrete index associated with the sample
     */
    template <typename Value,
              typename Index = uint_array_t<Value>>
    Index sample(Value sample_value, mask_t<Value> active = true) const {
        /* Note that the search range is adjusted to exclude entries at the
           beginning and end of the distribution that have probability 0.0.
           Otherwise we might return a indices that have no probability mass
           (in particular for sample_value = 0 or 1). */

        return math::find_interval(
            m_range_start, m_range_end,
            [&](Index idx, mask_t<Index> active) ENOKI_INLINE_LAMBDA {
                return gather<Value>(m_cdf.data(), idx, active) <= sample_value;
            },
            active
        );
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored distribution
     *
     * \param[in] sample_value
     *     A uniformly distributed sample on [0,1]
     *
     * \return
     *     A pair with (the discrete index associated with the sample, probability
     *     value of the sample).
     */
    template <typename Value, typename Index = uint_array_t<Value>>
    std::pair<Index, Value> sample_pdf(Value sample_value,
                                       mask_t<Value> active = true) const {
        Index index = sample(sample_value, active);

        return { index, gather<Value>(m_cdf.data(), index + 1, active) -
                        gather<Value>(m_cdf.data(), index, active) };
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored distribution
     *
     * The original sample is value adjusted so that it can be reused as a
     * uniform variate.
     *
     * \param sample_value
     *     A uniformly distributed sample on [0,1]
     *
     * \return
     *     The discrete index associated with the sample and the re-scaled sample value
     */
    template <typename Value, typename Index = uint_array_t<Value>>
    std::pair<Index, Value>
    sample_reuse(Value sample_value, mask_t<Value> active = true) const {
        Index index = sample(sample_value, active);
        Value cdf0 = gather<Value>(m_cdf.data(), index, active),
              cdf1 = gather<Value>(m_cdf.data(), index + 1, active);

        return { index, (sample_value - cdf0) / (cdf1 - cdf0) };
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored distribution.
     *
     * The original sample is value adjusted so that it can be reused as a
     * uniform variate.
     *
     * \param sample_value
     *     A uniformly distributed sample on [0,1]
     *
     * \return
     *     A tuple containing
     *
     *     1. the discrete index associated with the sample
     *     2. the probability value of the sample
     *     3. the re-scaled sample value
     */
    template <typename Value, typename Index = uint_array_t<Value>>
    std::tuple<Index, Value, Value>
    sample_reuse_pdf(Value sample_value, mask_t<Value> active = true) const {
        auto [index, pdf] = sample_pdf(sample_value, active);
        Value cdf0 = gather<Value>(m_cdf.data(), index, active),
              cdf1 = gather<Value>(m_cdf.data(), index + 1, active);

        return {
            index, pdf,
            (sample_value - cdf0) / (cdf1 - cdf0)
        };
    }

private:
    FloatStorage m_cdf;
    uint32_t m_range_start;       //< Index in m_cdf corresponding to the first entry with positive probability.
    uint32_t m_range_end;         //< 1 + the last index of m_cdf with positive probability, or 0 when there is none.
    ScalarFloat m_normalization;  //< Normalization constant (or infinity)
    bool m_normalized;            //< Is the distribution normalized?
    double m_sum;                 //< Running sum (in higher precision)
};

template <typename Float>
std::ostream &operator<<(std::ostream &os, const DiscreteDistribution<Float> &distribution) {
    os << "DiscreteDistribution[sum=" << distribution.sum() << ", normalized="
        << static_cast<int>(distribution.normalized())
        << ", cdf=" << distribution.cdf() << "]";
    return os;
}

NAMESPACE_END(mitsuba)
