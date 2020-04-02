#pragma once

#include <mitsuba/core/logger.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/math.h>

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
        for (uint32_t i = 0; i < size; ++i) {
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
        MTS_MASK_ARGUMENT(active);

        value *= m_sum;

        return enoki::binary_search(
            m_valid.x(), m_valid.y(),
            [&](Index index) ENOKI_INLINE_LAMBDA {
                return gather<Float>(m_cdf, index, active) < value;
            }
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
        MTS_MASK_ARGUMENT(active);

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
        MTS_MASK_ARGUMENT(active);

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
        MTS_MASK_ARGUMENT(active);

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
 * \brief Continuous 1D probability distribution defined in terms of a regularly
 * sampled linear interpolant
 *
 * This data structure represents a continuous 1D probability distribution that
 * is defined as a linear interpolant of a regularly discretized signal. The
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
    using ScalarVector2f = Vector<ScalarFloat, 2>;
    using ScalarVector2u = Vector<uint32_t, 2>;

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

            integral += value;
            *cdf_ptr++ = (ScalarFloat) integral;
            pdf_ptr++;

            if (y0 < 0. || y1 < 0.) {
                Throw("ContinuousDistribution: entries must be non-negative!");
            } else if (value > 0.) {
                // Determine the first and last wavelength bin with nonzero density
                if (m_valid.x() == (uint32_t) -1)
                    m_valid.x() = (uint32_t) i;
                m_valid.y() = (uint32_t) i;
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
        MTS_MASK_ARGUMENT(active);

        active &= x >= m_range.x() && x <= m_range.y();
        x = (x - m_range.x()) * m_inv_interval_size;

        Index index = clamp(Index(x), 0u, uint32_t(m_pdf.size() - 2));

        Float y0 = gather<Float>(m_pdf, index,      active),
              y1 = gather<Float>(m_pdf, index + 1u, active);

        Float w1 = x - Float(index),
              w0 = 1.f - w1;

        return fmadd(w0, y0, w1 * y1);
    }

    /// Evaluate the normalized probability mass function (PDF) at position \c x
    Float eval_pdf_normalized(Float x, Mask active) const {
        MTS_MASK_ARGUMENT(active);

        return eval_pdf(x, active) * m_normalization;
    }

    /// Evaluate the unnormalized cumulative distribution function (CDF) at position \c p
    Float eval_cdf(Float x_, Mask active = true) const {
        MTS_MASK_ARGUMENT(active);

        Float x = (x_ - m_range.x()) * m_inv_interval_size;

        Index index = clamp(Index(x), 0u, uint32_t(m_pdf.size() - 2));

        Float y0 = gather<Float>(m_pdf, index,      active),
              y1 = gather<Float>(m_pdf, index + 1u, active),
              c0 = gather<Float>(m_cdf, index - 1u, active && index > 0u);


        Float t   = clamp(x - Float(index), 0.f, 1.f),
              cdf = c0 + t * (y0 + .5f * t * (y1 - y0)) * m_interval_size;

        return cdf;
    }

    /// Evaluate the unnormalized cumulative distribution function (CDF) at position \c p
    Float eval_cdf_normalized(Float x, Mask active = true) const {
        MTS_MASK_ARGUMENT(active);

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
        MTS_MASK_ARGUMENT(active);

        value *= m_integral;

        Index index = enoki::binary_search(
            m_valid.x(), m_valid.y(),
            [&](Index index) ENOKI_INLINE_LAMBDA {
                return gather<Float>(m_cdf, index, active) < value;
            }
        );

        Float y0 = gather<Float>(m_pdf, index,      active),
              y1 = gather<Float>(m_pdf, index + 1u, active),
              c0 = gather<Float>(m_cdf, index - 1u, active && index > 0);

        value = (value - c0) * m_inv_interval_size;

        Float t_linear = (y0 - safe_sqrt(sqr(y0) + 2.f * value * (y1 - y0))) / (y0 - y1),
              t_const  = value / y0,
              t        = select(eq(y0, y1), t_const, t_linear);

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
        MTS_MASK_ARGUMENT(active);

        value *= m_integral;

        Index index = enoki::binary_search(
            m_valid.x(), m_valid.y(),
            [&](Index index) ENOKI_INLINE_LAMBDA {
                return gather<Float>(m_cdf, index, active) < value;
            }
        );

        Float y0 = gather<Float>(m_pdf, index,      active),
              y1 = gather<Float>(m_pdf, index + 1u, active),
              c0 = gather<Float>(m_cdf, index - 1u, active && index > 0);

        value = (value - c0) * m_inv_interval_size;

        Float t_linear = (y0 - safe_sqrt(sqr(y0) + 2.f * value * (y1 - y0))) / (y0 - y1),
              t_const  = value / y0,
              t        = select(eq(y0, y1), t_const, t_linear);

        return { fmadd(Float(index) + t, m_interval_size, m_range.x()),
                 fmadd(t, y1 - y0, y0) * m_normalization };
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

/**
 * \brief Continuous 1D probability distribution defined in terms of an
 * *irregularly* sampled linear interpolant
 *
 * This data structure represents a continuous 1D probability distribution that
 * is defined as a linear interpolant of an irregularly discretized signal. The
 * class provides various routines for transforming uniformly distributed
 * samples so that they follow the stored distribution. Note that unnormalized
 * probability density functions (PDFs) will automatically be normalized during
 * initialization. The associated scale factor can be retrieved using the
 * function \ref normalization().
 */
template <typename Float> struct IrregularContinuousDistribution {
    using FloatStorage = DynamicBuffer<Float>;
    using Index = uint32_array_t<Float>;
    using Mask = mask_t<Float>;

    using ScalarFloat = scalar_t<Float>;
    using ScalarVector2f = Array<ScalarFloat, 2>;
    using ScalarVector2u = Array<uint32_t, 2>;

public:
    /// Create an unitialized IrregularContinuousDistribution instance
    IrregularContinuousDistribution() { }

    /// Initialize from a given density function discretized on nodes \c nodes
    IrregularContinuousDistribution(const FloatStorage &nodes,
                                    const FloatStorage &pdf)
        : m_nodes(nodes), m_pdf(pdf) {
        update();
    }

    /// Initialize from a given density function (rvalue version)
    IrregularContinuousDistribution(FloatStorage &&nodes,
                                    FloatStorage &&pdf)
        : m_nodes(std::move(nodes)), m_pdf(std::move(pdf)) {
        update();
    }

    /// Initialize from a given floating point array
    IrregularContinuousDistribution(const ScalarFloat *nodes,
                                    const ScalarFloat *values,
                                    size_t size)
        : IrregularContinuousDistribution(FloatStorage::copy(nodes, size),
                                          FloatStorage::copy(values, size)) {
    }

    /// Update the internal state. Must be invoked when changing the pdf or range.
    void update() {
        size_t size = m_nodes.size();

        if (m_pdf.size() != size)
            Throw("IrregularContinuousDistribution: 'pdf' and 'nodes' size mismatch!");

        if (size < 2)
            Throw("IrregularContinuousDistribution: needs at least two entries!");

        if (m_cdf.size() != size - 1)
            m_cdf = enoki::empty<FloatStorage>(size - 1);

        // Ensure that we can access these arrays on the CPU
        m_pdf.managed();
        m_cdf.managed();
        m_nodes.managed();

        ScalarFloat *pdf_ptr = m_pdf.data(),
                    *cdf_ptr = m_cdf.data(),
                    *nodes_ptr = m_nodes.data();

        m_valid = (uint32_t) -1;
        m_range = ScalarVector2f(
             math::Infinity<Float>,
            -math::Infinity<Float>
        );

        double integral = 0.;

        for (size_t i = 0; i < size - 1; ++i) {
            double x0 = (double) nodes_ptr[0],
                   x1 = (double) nodes_ptr[1],
                   y0 = (double) pdf_ptr[0],
                   y1 = (double) pdf_ptr[1];

            double value = 0.5 * (x1 - x0) * (y0 + y1);

            m_range.x() = min(m_range.x(), x0);
            m_range.y() = max(m_range.y(), x1);

            integral += value;
            *cdf_ptr++ = (ScalarFloat) integral;
            pdf_ptr++; nodes_ptr++;

            if (!(x1 > x0)) {
                Throw("IrregularContinuousDistribution: node positions must be strictly increasing!");
            } else if (y0 < 0. || y1 < 0.) {
                Throw("IrregularContinuousDistribution: entries must be non-negative!");
            } else if (value > 0.) {
                // Determine the first and last wavelength bin with nonzero density
                if (m_valid.x() == (uint32_t) -1)
                    m_valid.x() = (uint32_t) i;
                m_valid.y() = (uint32_t) i;
            }
        }

        if (any(eq(m_valid, (uint32_t) -1)))
            Throw("IrregularContinuousDistribution: no probability mass found!");

        m_integral = ScalarFloat(integral);
        m_normalization = ScalarFloat(1. / integral);
    }

    /// Return the nodes of the underlying discretization
    FloatStorage &nodes() { return m_nodes; }

    /// Return the nodes of the underlying discretization (const version)
    const FloatStorage &nodes() const { return m_nodes; }

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
        MTS_MASK_ARGUMENT(active);

        active &= x >= m_range.x() && x <= m_range.y();

        Index index = enoki::binary_search(
            0, (uint32_t) m_nodes.size(),
            [&](Index index) ENOKI_INLINE_LAMBDA {
                return gather<Float>(m_nodes, index, active) < x;
            }
        );

        index = enoki::max(enoki::min(index, (uint32_t) m_nodes.size() - 1u), 1u) - 1u;

        Float x0 = gather<Float>(m_nodes, index,      active),
              x1 = gather<Float>(m_nodes, index + 1u, active),
              y0 = gather<Float>(m_pdf,   index,      active),
              y1 = gather<Float>(m_pdf,   index + 1u, active);

        x = (x - x0) / (x1 - x0);

        return select(active, fmadd(x, y1 - y0, y0), 0.f);
    }

    /// Evaluate the normalized probability mass function (PDF) at position \c x
    Float eval_pdf_normalized(Float x, Mask active) const {
        MTS_MASK_ARGUMENT(active);

        return eval_pdf(x, active) * m_normalization;
    }

    /// Evaluate the unnormalized cumulative distribution function (CDF) at position \c p
    Float eval_cdf(Float x, Mask active = true) const {
        MTS_MASK_ARGUMENT(active);

        Index index = enoki::binary_search(
            0, (uint32_t) m_nodes.size(),
            [&](Index index) ENOKI_INLINE_LAMBDA {
                return gather<Float>(m_nodes, index, active) < x;
            }
        );

        index = enoki::max(enoki::min(index, (uint32_t) m_nodes.size() - 1u), 1u) - 1u;

        Float x0 = gather<Float>(m_nodes, index,      active),
              x1 = gather<Float>(m_nodes, index + 1u, active),
              y0 = gather<Float>(m_pdf,   index,      active),
              y1 = gather<Float>(m_pdf,   index + 1u, active),
              c0 = gather<Float>(m_cdf,   index - 1u, active && index > 0u);

        Float w   = x1 - x0,
              t   = clamp((x - x0) / w, 0.f, 1.f),
              cdf = c0 + w * t * (y0 + .5f * t * (y1 - y0));

        return cdf;
    }

    /// Evaluate the unnormalized cumulative distribution function (CDF) at position \c p
    Float eval_cdf_normalized(Float x, Mask active = true) const {
        MTS_MASK_ARGUMENT(active);

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
        MTS_MASK_ARGUMENT(active);

        value *= m_integral;

        Index index = enoki::binary_search(
            m_valid.x(), m_valid.y(),
            [&](Index index) ENOKI_INLINE_LAMBDA {
                return gather<Float>(m_cdf, index, active) < value;
            }
        );

        Float x0 = gather<Float>(m_nodes, index,      active),
              x1 = gather<Float>(m_nodes, index + 1u, active),
              y0 = gather<Float>(m_pdf,   index,      active),
              y1 = gather<Float>(m_pdf,   index + 1u, active),
              c0 = gather<Float>(m_cdf,   index - 1u, active && index > 0),
              w  = x1 - x0;

        value = (value - c0) / w;

        Float t_linear = (y0 - safe_sqrt(sqr(y0) + 2.f * value * (y1 - y0))) / (y0 - y1),
              t_const  = value / y0,
              t        = select(eq(y0, y1), t_const, t_linear);

        return fmadd(t, w, x0);
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
        MTS_MASK_ARGUMENT(active);

        value *= m_integral;

        Index index = enoki::binary_search(
            m_valid.x(), m_valid.y(),
            [&](Index index) ENOKI_INLINE_LAMBDA {
                return gather<Float>(m_cdf, index, active) < value;
            }
        );

        Float x0 = gather<Float>(m_nodes, index,      active),
              x1 = gather<Float>(m_nodes, index + 1u, active),
              y0 = gather<Float>(m_pdf,   index,      active),
              y1 = gather<Float>(m_pdf,   index + 1u, active),
              c0 = gather<Float>(m_cdf,   index - 1u, active && index > 0),
              w  = x1 - x0;

        value = (value - c0) / w;

        Float t_linear = (y0 - safe_sqrt(sqr(y0) + 2.f * value * (y1 - y0))) / (y0 - y1),
              t_const  = value / y0,
              t        = select(eq(y0, y1), t_const, t_linear);

        return { fmadd(t, w, x0),
            fmadd(t, y1 - y0, y0) * m_normalization };
    }

private:
    FloatStorage m_nodes;
    FloatStorage m_pdf;
    FloatStorage m_cdf;
    ScalarFloat m_integral = 0.f;
    ScalarFloat m_normalization = 0.f;
    ScalarVector2f m_range { 0.f, 0.f };
    ScalarVector2u m_valid;
};

template <typename Float>
std::ostream &operator<<(std::ostream &os, const DiscreteDistribution<Float> &distr) {
    os << "DiscreteDistribution[" << std::endl
        << "  size = " << distr.size() << "," << std::endl
        << "  sum = " << distr.sum() << "," << std::endl
        << "  pmf = " << distr.pmf() << std::endl
        << "]";
    return os;
}

template <typename Float>
std::ostream &operator<<(std::ostream &os, const ContinuousDistribution<Float> &distr) {
    os << "ContinuousDistribution[" << std::endl
        << "  size = " << distr.size() << "," << std::endl
        << "  range = " << distr.range() << "," << std::endl
        << "  integral = " << distr.integral() << "," << std::endl
        << "  pdf = " << distr.pdf() << std::endl
        << "]";
    return os;
}

template <typename Float>
std::ostream &operator<<(std::ostream &os, const IrregularContinuousDistribution<Float> &distr) {
    os << "IrregularContinuousDistribution[" << std::endl
        << "  size = " << distr.size() << "," << std::endl
        << "  nodes = " << distr.nodes() << "," << std::endl
        << "  integral = " << distr.integral() << "," << std::endl
        << "  pdf = " << distr.pdf() << "," << std::endl
        << "]";
    return os;
}

NAMESPACE_END(mitsuba)
