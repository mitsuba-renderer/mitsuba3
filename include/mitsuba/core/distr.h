#pragma once

#include <mitsuba/core/logger.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Discrete 1D probability distribution
 *
 * This data structure represents a discrete 1D probability distribution and
 * provides various routines for transforming uniformly distributed samples so
 * that they follow the stored distribution. Note that unnormalized
 * probability mass functions (PMFs) will automatically be normalized during
 * initialization. The associated scale factor can be retrieved using the
 * function \ref normalization().
 */
template <typename Float> struct DiscreteDistribution {
    using FloatStorage = DynamicBuffer<Float>;
    using Index = uint32_array_t<Float>;
    using Mask = mask_t<Float>;

    using ScalarFloat = scalar_t<Float>;
    using ScalarVector2u = Array<uint32_t, 2>;

public:
    /// Create an unitialized DiscreteDistribution instance
    DiscreteDistribution() { }

    /// Initialize from a given probability mass function
    DiscreteDistribution(const FloatStorage &pmf)
        : m_pmf(pmf) {
        update();
    }

    /// Initialize from a given probability mass function (rvalue version)
    DiscreteDistribution(FloatStorage &&pmf)
        : m_pmf(std::move(pmf)) {
        update();
    }

    /// Initialize from a given floating point array
    DiscreteDistribution(const ScalarFloat *values, size_t size)
        : DiscreteDistribution(FloatStorage::copy(values, size)) {
    }

    /// Update the internal state. Must be invoked when changing the pmf.
    void update() {
        size_t size = m_pmf.size();

        if (size == 0)
            Throw("DiscreteDistribution: empty distribution!");

        if (m_cdf.size() != size)
            m_cdf = enoki::empty<FloatStorage>(size);

        // Ensure that we can access these arrays on the CPU
        m_pmf.managed();
        m_cdf.managed();

        ScalarFloat *pmf_ptr = m_pmf.data(),
                    *cdf_ptr = m_cdf.data();

        m_valid = (uint32_t) -1;

        double sum = 0.0;
        for (size_t i = 0; i < size; ++i) {
            double value = (double) *pmf_ptr++;
            sum += value;
            *cdf_ptr++ = (ScalarFloat) sum;

            if (value < 0.0) {
                Throw("DiscreteDistribution: entries must be non-negative!");
            } else if (value > 0.0) {
                // Determine the first and last wavelength bin with nonzero density
                if (m_valid.x() == (uint32_t) -1)
                    m_valid.x() = i;
                m_valid.y() = i;
            }
        }

        if (any(eq(m_valid, (uint32_t) -1)))
            Throw("DiscreteDistribution: no probability mass found!");

        m_sum = ScalarFloat(sum);
        m_normalization = ScalarFloat(1.0 / sum);
    }

    /// Return the unnormalized probability mass function
    FloatStorage &pmf() { return m_pmf; }

    /// Return the unnormalized probability mass function (const version)
    const FloatStorage &pmf() const { return m_pmf; }

    /// Return the unnormalized cumulative distribution function
    FloatStorage &cdf() { return m_cdf; }

    /// Return the unnormalized cumulative distribution function (const version)
    const FloatStorage &cdf() const { return m_cdf; }

    /// \brief Return the original sum of PMF entries before normalization
    ScalarFloat sum() const { return m_sum; }

    /// \brief Return the normalization factor (i.e. the inverse of \ref sum())
    ScalarFloat normalization() const { return m_normalization; }

    /// Return the number of entries
    size_t size() const { return m_pmf.size(); }

    /// Is the distribution object empty/uninitialized?
    bool empty() const { return m_pmf.empty(); }

    /// Evaluate the unnormalized probability mass function (PMF) at index \c index
    Float eval_pmf(Index index, Mask active = true) const {
        return gather<Float>(m_pmf, index, active);
    }

    /// Evaluate the normalized probability mass function (PMF) at index \c index
    Float eval_pmf_normalized(Index index, Mask active = true) const {
        return gather<Float>(m_pmf, index, active) * m_normalization;
    }

    /// Evaluate the unnormalized cumulative distribution function (CDF) at index \c index
    Float eval_cdf(Index index, Mask active = true) const {
        return gather<Float>(m_cdf, index, active);
    }

    /// Evaluate the normalized cumulative distribution function (CDF) at index \c index
    Float eval_cdf_normalized(Index index, Mask active = true) const {
        return gather<Float>(m_cdf, index, active) * m_normalization;
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored
     * distribution
     *
     * \param value
     *     A uniformly distributed sample on the interval [0, 1].
     *
     * \return
     *     The discrete index associated with the sample
     */
    Index sample(Float value, Mask active = true) const {
        value *= m_sum;

        return enoki::binary_search(
            m_valid.x(), m_valid.y(),
            [&](Index index, Mask active) ENOKI_INLINE_LAMBDA {
                return gather<Float>(m_cdf, index, active) < value;
            },
            active
        );
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored
     * distribution
     *
     * \param value
     *     A uniformly distributed sample on the interval [0, 1].
     *
     * \return
     *     A tuple consisting of
     *
     *     1. the discrete index associated with the sample, and
     *     2. the normalized probability value of the sample.
     */
    std::pair<Index, Float> sample_pmf(Float value, Mask active = true) const {
        Index index = sample(value, active);
        return { index, eval_pmf_normalized(index) };
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored
     * distribution
     *
     * The original sample is value adjusted so that it can be reused as a
     * uniform variate.
     *
     * \param value
     *     A uniformly distributed sample on the interval [0, 1].
     *
     * \return
     *     A tuple consisting of
     *
     *     1. the discrete index associated with the sample, and
     *     2. the re-scaled sample value.
     */
    std::pair<Index, Float>
    sample_reuse(Float value, Mask active = true) const {
        Index index = sample(value, active);

        Float pmf = eval_pmf_normalized(index, active),
              cdf = eval_cdf_normalized(index - 1, active && index > 0);

        return { index, (value - cdf) / pmf };
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored
     * distribution.
     *
     * The original sample is value adjusted so that it can be reused as a
     * uniform variate.
     *
     * \param value
     *     A uniformly distributed sample on the interval [0, 1].
     *
     * \return
     *     A tuple consisting of
     *
     *     1. the discrete index associated with the sample
     *     2. the re-scaled sample value
     *     3. the normalized probability value of the sample
     */
    std::tuple<Index, Float, Float>
    sample_reuse_pmf(Float value, Mask active = true) const {
        auto [index, pdf] = sample_pmf(value, active);

        Float pmf = eval_pmf_normalized(index, active),
              cdf = eval_cdf_normalized(index - 1, active && index > 0);

        return {
            index,
            (value - cdf) / pmf,
            pmf
        };
    }

private:
    FloatStorage m_pmf;
    FloatStorage m_cdf;
    ScalarFloat m_sum = 0.f;
    ScalarFloat m_normalization = 0.f;
    ScalarVector2u m_valid;
};

/**
 * \brief Continuous 1D probability distribution
 *
 * This data structure represents a continuous 1D probability distribution that
 * is defined as a linear interpolant of a uniformly discretized signal. The
 * class provides various routines for transforming uniformly distributed
 * samples so that they follow the stored distribution. Note that unnormalized
 * probability density functions (PDFs) will automatically be normalized during
 * initialization. The associated scale factor can be retrieved using the
 * function \ref normalization().
 */
template <typename Float> struct ContinuousDistribution {
    using FloatStorage = DynamicBuffer<Float>;
    using Index = uint32_array_t<Float>;
    using Mask = mask_t<Float>;

    using ScalarFloat = scalar_t<Float>;
    using ScalarVector2f = Array<ScalarFloat, 2>;
    using ScalarVector2u = Array<uint32_t, 2>;

public:
    /// Create an unitialized ContinuousDistribution instance
    ContinuousDistribution() { }

    /// Initialize from a given density function on the interval \c range
    ContinuousDistribution(const ScalarVector2f &range,
                           const FloatStorage &pdf)
        : m_pdf(pdf), m_range(range) {
        update();
    }

    /// Initialize from a given density function (rvalue version)
    ContinuousDistribution(const ScalarVector2f &range,
                           FloatStorage &&pdf)
        : m_pdf(std::move(pdf)), m_range(range) {
        update();
    }

    /// Initialize from a given floating point array
    ContinuousDistribution(const ScalarVector2f &range,
                           const ScalarFloat *values,
                           size_t size)
        : ContinuousDistribution(range, FloatStorage::copy(values, size)) {
    }

    /// Update the internal state. Must be invoked when changing the pdf or range.
    void update() {
        size_t size = m_pdf.size();

        if (size < 2)
            Throw("ContinuousDistribution: needs at least two entries!");

        if (!(m_range.x() < m_range.y()))
            Throw("ContinuousDistribution: invalid range!");

        if (m_cdf.size() != size - 1)
            m_cdf = enoki::empty<FloatStorage>(size - 1);

        // Ensure that we can access these arrays on the CPU
        m_pdf.managed();
        m_cdf.managed();

        ScalarFloat *pdf_ptr = m_pdf.data(),
                    *cdf_ptr = m_cdf.data();

        m_valid = (uint32_t) -1;

        double range = double(m_range.y()) - double(m_range.x()),
               interval_size = range / (size - 1),
               integral = 0.;

        for (size_t i = 0; i < size - 1; ++i) {
            double y0 = (double) pdf_ptr[0],
                   y1 = (double) pdf_ptr[1];

            double value = 0.5 * interval_size * (y0 + y1);
            std::cout << "Integral[" << i << "] = " << value << std::endl;

            integral += value;
            *cdf_ptr++ = (ScalarFloat) integral;
            pdf_ptr++;

            if (y0 < 0. || y1 < 0.) {
                Throw("ContinuousDistribution: entries must be non-negative!");
            } else if (value > 0.) {
                // Determine the first and last wavelength bin with nonzero density
                if (m_valid.x() == (uint32_t) -1)
                    m_valid.x() = i;
                m_valid.y() = i;
            }
        }

        if (any(eq(m_valid, (uint32_t) -1)))
            Throw("ContinuousDistribution: no probability mass found!");

        m_integral = ScalarFloat(integral);
        m_normalization = ScalarFloat(1. / integral);
        m_interval_size = ScalarFloat(interval_size);
        m_inv_interval_size = ScalarFloat(1. / interval_size);
    }

    /// Return the range of the distribution
    ScalarVector2f &range() { return m_range; }

    /// Return the range of the distribution (const version)
    const ScalarVector2f &range() const { return m_range; }

    /// Return the unnormalized discretized probability density function
    FloatStorage &pdf() { return m_pdf; }

    /// Return the unnormalized discretized probability density function (const version)
    const FloatStorage &pdf() const { return m_pdf; }

    /// Return the unnormalized discrete cumulative distribution function over intervals
    FloatStorage &cdf() { return m_cdf; }

    /// Return the unnormalized discrete cumulative distribution function over intervals (const version)
    const FloatStorage &cdf() const { return m_cdf; }

    /// \brief Return the original integral of PDF entries before normalization
    ScalarFloat integral() const { return m_integral; }

    /// \brief Return the normalization factor (i.e. the inverse of \ref sum())
    ScalarFloat normalization() const { return m_normalization; }

    /// Return the number of discretizations
    size_t size() const { return m_pdf.size(); }

    /// Is the distribution object empty/uninitialized?
    bool empty() const { return m_pdf.empty(); }

    /// Evaluate the unnormalized probability mass function (PDF) at position \c x
    Float eval_pdf(Float x, Mask active = true) const {
        active &= x >= m_range.x() && x <= m_range.y();
        x = (x - m_range.x()) * m_inv_interval_size;

        Index index = clamp(Index(x), 0u, uint32_t(m_pdf.size() - 2));

        Float pdf0 = gather<Float>(m_pdf, index,      active),
              pdf1 = gather<Float>(m_pdf, index + 1u, active);

        Float w1 = x - Float(index),
              w0 = 1.f - w1;

        return fmadd(w0, pdf0, w1 * pdf1);
    }

    /// Evaluate the normalized probability mass function (PDF) at position \c x
    Float eval_pdf_normalized(Float x, Mask active) const {
        return eval_pdf(x, active) * m_normalization;
    }

    /// Evaluate the unnormalized cumulative distribution function (CDF) at position \c p
    Float eval_cdf(Float x_, Mask active = true) const {
        active &= x_ >= m_range.x();
        Float x = (x_ - m_range.x()) * m_inv_interval_size;

        Index index = clamp(Index(x), 0u, uint32_t(m_pdf.size() - 2));

        Float c0 = gather<Float>(m_cdf, index - 1u, active && index > 0u);

        Float f0 = gather<Float>(m_pdf, index,      active),
              f1 = gather<Float>(m_pdf, index + 1u, active);

        Float t = x - Float(index);

        return select(
            x_ > m_range.y(),
            Float(m_integral),
            c0 + t * (f0 + .5f * t * (f1 - f0)) * m_interval_size
        );
    }

    /// Evaluate the unnormalized cumulative distribution function (CDF) at position \c p
    Float eval_cdf_normalized(Float x, Mask active = true) const {
        return eval_cdf(x, active) * m_normalization;
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored
     * distribution
     *
     * \param value
     *     A uniformly distributed sample on the interval [0, 1].
     *
     * \return
     *     The sampled position.
     */
    Float sample(Float value, Mask active = true) const {
        std::cout << "Searching for sample pos = " << value << std::endl;
        value *= m_integral;
        std::cout << " -> sample pos = " << value << std::endl;

        Index index = enoki::binary_search(
            m_valid.x(), m_valid.y(),
            [&](Index index, Mask active) ENOKI_INLINE_LAMBDA {
                return gather<Float>(m_cdf, index, active) < value;
            },
            active
        );
        std::cout << " -> got index = " << index << std::endl;

        Float f0 = gather<Float>(m_pdf, index,      active),
              f1 = gather<Float>(m_pdf, index + 1u, active);

        Float c0 = gather<Float>(m_cdf, index- 1u, active && index > 0),
              c1 = gather<Float>(m_cdf, index, active);

        std::cout << " -> pos0 = " << (index * m_interval_size + m_range.x()) << std::endl;
        std::cout << " -> pos1 = " << ((index+1) * m_interval_size + m_range.x()) << std::endl;
        std::cout << " -> pdf0 = " << f0 << std::endl;
        std::cout << " -> pdf1 = " << f1 << std::endl;
        std::cout << " -> cdf0 = " << value-c0 << std::endl;
        std::cout << " -> cdf1 = " << value-c1 << std::endl;
        std::cout << " -> cdf_diff = " << c1-c0 << std::endl;
        std::cout << " -> integral = " << (f0+f1)*0.5f * m_interval_size << std::endl;

        value -= gather<Float>(m_cdf, index - 1u, active && index > 0);
        std::cout << " -> adjusted value = " << value << std::endl;
        value *= m_inv_interval_size;

        Float t_linear = (f0 - safe_sqrt(sqr(f0) + 2.f * value * (f1 - f0))) / (f0 - f1),
              t_const  = value / f0,
              t        = select(eq(f0, f1), t_const, t_linear);
        std::cout << "prediction: const=" << t_const << ", linear=" << t_linear << std::endl;

        return fmadd(Float(index) + t, m_interval_size, m_range.x());
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored
     * distribution
     *
     * \param value
     *     A uniformly distributed sample on the interval [0, 1].
     *
     * \return
     *     A tuple consisting of
     *
     *     1. the sampled position.
     *     2. the normalized probability density of the sample.
     */
    std::pair<Float, Float> sample_pdf(Float value, Mask active = true) const {
        value *= m_integral;

        Index index = enoki::binary_search(
            m_valid.x(), m_valid.y(),
            [&](Index index, Mask active) ENOKI_INLINE_LAMBDA {
                return gather<Float>(m_cdf, index, active) < value;
            },
            active
        );

        Float f0 = gather<Float>(m_pdf, index,      active),
              f1 = gather<Float>(m_pdf, index + 1u, active);

        value -= gather<Float>(m_cdf, index - 1u, active && index > 0);
        value *= m_inv_interval_size;

        Float t_linear = (f0 - safe_sqrt(sqr(f0) + 2.f * value * (f1 - f0))) / (f0 - f1),
              t_const  = value / f0,
              t        = select(eq(f0, f1), t_const, t_linear);

        return { fmadd(Float(index) + t, m_interval_size, m_range.x()),
                 fmadd(t, f1 - f0, f0) * m_normalization };
    }

private:
    FloatStorage m_pdf;
    FloatStorage m_cdf;
    ScalarFloat m_integral = 0.f;
    ScalarFloat m_normalization = 0.f;
    ScalarFloat m_interval_size = 0.f;
    ScalarFloat m_inv_interval_size = 0.f;
    ScalarVector2f m_range { 0.f, 0.f };
    ScalarVector2u m_valid;
};

template <typename Float>
std::ostream &operator<<(std::ostream &os, const DiscreteDistribution<Float> &distr) {
    os << "DiscreteDistribution["
        << "size=" << distr.size()
        << ", sum=" << distr.sum()
        << ", pmf=" << distr.pmf()
        << "]";
    return os;
}

template <typename Float>
std::ostream &operator<<(std::ostream &os, const ContinuousDistribution<Float> &distr) {
    os << "ContinuousDistribution["
        << "size=" << distr.size()
        << ", range=" << distr.range()
        << ", integral=" << distr.integral()
        << ", pdf=" << distr.pdf()
        << "]";
    return os;
}

NAMESPACE_END(mitsuba)
