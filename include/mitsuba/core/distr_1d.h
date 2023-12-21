#pragma once

#include <mitsuba/core/logger.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/math.h>
#include <drjit/dynamic.h>

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
template <typename Value> struct DiscreteDistribution {
    using Float = std::conditional_t<dr::is_static_array_v<Value>,
                                     dr::value_t<Value>, Value>;
    using FloatStorage   = DynamicBuffer<Float>;
    using UInt32         = dr::uint32_array_t<Float>;
    using Index          = dr::uint32_array_t<Value>;
    using Mask           = dr::mask_t<Value>;
    using Vector2u       = dr::Array<UInt32, 2>;

    using ScalarFloat    = dr::scalar_t<Float>;
    using ScalarVector2u = dr::Array<uint32_t, 2>;

public:
    /// Create an uninitialized DiscreteDistribution instance
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
        : m_pmf(dr::load<FloatStorage>(values, size)) {
        compute_cdf_scalar(values, size);
    }

    /// Update the internal state. Must be invoked when changing the pmf.
    void update() {
        if constexpr (dr::is_jit_v<Float>)
            compute_cdf();
        else
            compute_cdf_scalar(m_pmf.data(), m_pmf.size());
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
    Float sum() const { return m_sum; }

    /// \brief Return the normalization factor (i.e. the inverse of \ref sum())
    Float normalization() const { return m_normalization; }

    /// Return the number of entries
    size_t size() const { return m_pmf.size(); }

    /// Is the distribution object empty/uninitialized?
    bool empty() const { return m_pmf.empty(); }

    /// Evaluate the unnormalized probability mass function (PMF) at index \c index
    Value eval_pmf(Index index, Mask active = true) const {
        return dr::gather<Value>(m_pmf, index, active);
    }

    /// Evaluate the normalized probability mass function (PMF) at index \c index
    Value eval_pmf_normalized(Index index, Mask active = true) const {
        return dr::gather<Value>(m_pmf, index, active) * m_normalization;
    }

    /// Evaluate the unnormalized cumulative distribution function (CDF) at index \c index
    Value eval_cdf(Index index, Mask active = true) const {
        return dr::gather<Value>(m_cdf, index, active);
    }

    /// Evaluate the normalized cumulative distribution function (CDF) at index \c index
    Value eval_cdf_normalized(Index index, Mask active = true) const {
        return dr::gather<Value>(m_cdf, index, active) * m_normalization;
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored
     * distribution
     *
     * \param sample
     *     A uniformly distributed sample on the interval [0, 1].
     *
     * \return
     *     The discrete index associated with the sample
     */
    Index sample(Value sample, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

        sample *= m_sum;

        return dr::binary_search<Index>(
            m_valid.x(), m_valid.y(),
            [&](Index index) DRJIT_INLINE_LAMBDA {
                Value value = dr::gather<Value>(m_cdf, index, active);
                if constexpr (!dr::is_jit_v<Float>) {
                    return value < sample;
                } else {
                    // `m_valid` is not computed in JIT variants
                    return ((value < sample) || (dr::eq(value, 0))) &&
                           dr::neq(value, m_sum);
                }
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
    std::pair<Index, Value> sample_pmf(Value value, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

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
    std::pair<Index, Value>
    sample_reuse(Value value, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

        Index index = sample(value, active);

        Value pmf = eval_pmf_normalized(index, active),
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
    std::tuple<Index, Value, Value>
    sample_reuse_pmf(Value value, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

        auto [index, pdf] = sample_pmf(value, active);

        Value pmf = eval_pmf_normalized(index, active),
              cdf = eval_cdf_normalized(index - 1, active && index > 0);

        return {
            index,
            (value - cdf) / pmf,
            pmf
        };
    }

private:
    void compute_cdf() {
        if (m_pmf.empty())
            Throw("DiscreteDistribution: empty distribution!");
        if (!dr::all(m_pmf >= 0.f))
            Throw("DiscreteDistribution: entries must be non-negative!");
        if (!dr::any(m_pmf > 0.f))
            Throw("DiscreteDistribution: no probability mass found!");

        m_cdf = dr::prefix_sum(m_pmf, false);
        m_valid = Vector2u(0, m_pmf.size() - 1);
        m_sum = dr::gather<Float>(m_cdf, m_valid.y());
        m_normalization = dr::rcp(m_sum);
        dr::make_opaque(m_valid, m_sum, m_normalization);
    }

    void compute_cdf_scalar(const ScalarFloat *pmf, size_t size) {
        if (size == 0)
            Throw("DiscreteDistribution: empty distribution!");

        std::vector<ScalarFloat> cdf(size);
        ScalarVector2u valid = (uint32_t) -1;

        double sum = 0.0;
        for (uint32_t i = 0; i < size; ++i) {
            double value = (double) *pmf++;
            sum += value;
            cdf[i] = (ScalarFloat) sum;

            if (value < 0.0) {
                Throw("DiscreteDistribution: entries must be non-negative!");
            } else if (value > 0.0) {
                // Determine the first and last bin with nonzero density
                if (valid.x() == (uint32_t) -1)
                    valid.x() = i;
                valid.y() = i;
            }
        }

        if (dr::any(dr::eq(valid, (uint32_t) -1)))
            Throw("DiscreteDistribution: no probability mass found!");

        m_cdf = dr::load<FloatStorage>(cdf.data(), size);
        m_valid = valid;
        m_sum = dr::gather<Float>(m_cdf, m_valid.y());
        m_normalization = dr::rcp(m_sum);
        dr::make_opaque(m_valid, m_sum, m_normalization);
    }

private:
    FloatStorage m_pmf;
    FloatStorage m_cdf;
    Float m_sum = 0.f;
    Float m_normalization = 0.f;
    Vector2u m_valid;
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
template <typename Value> struct ContinuousDistribution {
    using Float = std::conditional_t<dr::is_static_array_v<Value>,
                                     dr::value_t<Value>, Value>;
    using FloatStorage = DynamicBuffer<Float>;
    using UInt32 = dr::uint32_array_t<Float>;
    using Index = dr::uint32_array_t<Value>;
    using Mask = dr::mask_t<Value>;
    using Vector2u = dr::Array<UInt32, 2>;

    using ScalarFloat = dr::scalar_t<Float>;
    using ScalarVector2f = Vector<ScalarFloat, 2>;
    using ScalarVector2u = Vector<uint32_t, 2>;

public:
    /// Create an uninitialized ContinuousDistribution instance
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
                           const ScalarFloat *values, size_t size)
        : m_pdf(dr::load<FloatStorage>(values, size)),
          m_range(range) {
        compute_cdf_scalar(values, size);
    }

    /// Update the internal state. Must be invoked when changing the pdf.
    void update() {
        if constexpr (dr::is_jit_v<Float>)
            compute_cdf();
        else
            compute_cdf_scalar(m_pdf.data(), m_pdf.size());
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
    Float integral() const { return m_integral; }

    /// \brief Return the normalization factor (i.e. the inverse of \ref sum())
    Float normalization() const { return m_normalization; }

    /// Return the number of discretizations
    size_t size() const { return m_pdf.size(); }

    /// Is the distribution object empty/uninitialized?
    bool empty() const { return m_pdf.empty(); }

    /// Evaluate the unnormalized probability mass function (PDF) at position \c x
    Value eval_pdf(Value x, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

        active &= x >= m_range.x() && x <= m_range.y();
        x = (x - m_range.x()) * m_inv_interval_size;

        Index index = dr::clamp(Index(x), 0u, uint32_t(m_pdf.size() - 2));

        Value y0 = dr::gather<Value>(m_pdf, index,      active),
              y1 = dr::gather<Value>(m_pdf, index + 1u, active);

        Value w1 = x - Value(index),
              w0 = 1.f - w1;

        return dr::fmadd(w0, y0, w1 * y1);
    }

    /// Evaluate the normalized probability mass function (PDF) at position \c x
    Value eval_pdf_normalized(Value x, Mask active) const {
        MI_MASK_ARGUMENT(active);

        return eval_pdf(x, active) * m_normalization;
    }

    /// Evaluate the unnormalized cumulative distribution function (CDF) at position \c p
    Value eval_cdf(Value x_, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

        Value x = (x_ - m_range.x()) * m_inv_interval_size;

        Index index = dr::clamp(Index(x), 0u, uint32_t(m_pdf.size() - 2));

        Value y0 = dr::gather<Value>(m_pdf, index,      active),
              y1 = dr::gather<Value>(m_pdf, index + 1u, active),
              c0 = dr::gather<Value>(m_cdf, index - 1u, active && index > 0u);

        Value t   = dr::clamp(x - Value(index), 0.f, 1.f),
              cdf = dr::fmadd(t, dr::fmadd(.5f * t, y1 - y0, y0) * m_interval_size, c0);

        return cdf;
    }

    /// Evaluate the unnormalized cumulative distribution function (CDF) at position \c p
    Value eval_cdf_normalized(Value x, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

        return eval_cdf(x, active) * m_normalization;
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored
     * distribution
     *
     * \param sample
     *     A uniformly distributed sample on the interval [0, 1].
     *
     * \return
     *     The sampled position.
     */
    Value sample(Value sample, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

        sample *= m_integral;

        Index index = dr::binary_search<Index>(
            m_valid.x(), m_valid.y(),
            [&](Index index) DRJIT_INLINE_LAMBDA {
                Value value = dr::gather<Value>(m_cdf, index, active);
                if constexpr (!dr::is_jit_v<Float>) {
                    return value < sample;
                } else {
                    // `m_valid` is not computed in JIT variants
                    return ((value < sample) || (dr::eq(value, 0))) &&
                           dr::neq(value, m_integral);
                }
            }
        );

        Value y0 = dr::gather<Value>(m_pdf, index,      active),
              y1 = dr::gather<Value>(m_pdf, index + 1u, active),
              c0 = dr::gather<Value>(m_cdf, index - 1u, active && index > 0);

        sample = (sample - c0) * m_inv_interval_size;

        Value t_linear = (y0 - dr::safe_sqrt(dr::fmadd(y0, y0, 2.f * sample * (y1 - y0)))) * dr::rcp(y0 - y1),
              t_const  = sample * dr::rcp(y0),
              t        = dr::select(dr::eq(y0, y1), t_const, t_linear);

        return dr::fmadd(Value(index) + t, m_interval_size, m_range.x());
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored
     * distribution
     *
     * \param sample 
     *     A uniformly distributed sample on the interval [0, 1].
     *
     * \return
     *     A tuple consisting of
     *
     *     1. the sampled position.
     *     2. the normalized probability density of the sample.
     */
    std::pair<Value, Value> sample_pdf(Value sample, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

        sample *= m_integral;

        Index index = dr::binary_search<Index>(
            m_valid.x(), m_valid.y(),
            [&](Index index) DRJIT_INLINE_LAMBDA {
                Value value = dr::gather<Value>(m_cdf, index, active);
                if constexpr (!dr::is_jit_v<Float>) {
                    return value < sample;
                } else {
                    // `m_valid` is not computed in JIT variants
                    return ((value < sample) || (dr::eq(value, 0))) &&
                           dr::neq(value, m_integral);
                }
            }
        );

        Value y0 = dr::gather<Value>(m_pdf, index,      active),
              y1 = dr::gather<Value>(m_pdf, index + 1u, active),
              c0 = dr::gather<Value>(m_cdf, index - 1u, active && index > 0);

        sample = (sample - c0) * m_inv_interval_size;

        Value t_linear = (y0 - dr::safe_sqrt(dr::fmadd(y0, y0, 2.f * sample * (y1 - y0)))) * dr::rcp(y0 - y1),
              t_const  = sample * dr::rcp(y0),
              t        = dr::select(dr::eq(y0, y1), t_const, t_linear);

        return { dr::fmadd(Value(index) + t, m_interval_size, m_range.x()),
                 dr::fmadd(t, y1 - y0, y0) * m_normalization };
    }

    /// Return the minimum resolution of the discretization
    ScalarFloat interval_resolution() const {
        return m_interval_size_scalar;
    }

    ScalarFloat max() const {
        return m_max;
    }

private:
    void compute_cdf() {
        if (m_pdf.size() < 2)
            Throw("ContinuousDistribution: needs at least two entries!");
        if (!(m_range.x() < m_range.y()))
            Throw("ContinuousDistribution: invalid range!");
        if (!dr::all(m_pdf >= 0.f))
            Throw("ContinuousDistribution: entries must be non-negative!");
        if (!dr::any(m_pdf > 0.f))
            Throw("ContinuousDistribution: no probability mass found!");

        uint32_t size = m_pdf.size() - 1;
        m_interval_size_scalar = (m_range.y() - m_range.x()) / size;
        m_interval_size = dr::opaque<Float>(m_interval_size_scalar);
        UInt32 index_1_to_n = dr::arange<UInt32>(1, size + 1);

        // cdf[i] = interval_size * (sum(pmf[:i+1]) - 0.5 * (pmf[0] + pmf[i+1]))
        m_cdf =
            m_interval_size *
            (dr::gather<Float>(dr::prefix_sum(m_pdf, false), index_1_to_n) -
             0.5 * dr::gather<Float>(m_pdf, UInt32(0)) -
             0.5 * dr::gather<Float>(m_pdf, index_1_to_n));

        m_valid = Vector2u(0, size - 1);
        m_integral = dr::gather<Float>(m_cdf, m_valid.y());
        m_normalization = dr::rcp(m_integral);
        m_inv_interval_size = dr::rcp(m_interval_size);
        m_max = dr::slice(dr::max(m_pdf));
        dr::make_opaque(m_valid, m_cdf, m_integral, m_normalization,
                        m_inv_interval_size);
    }

    void compute_cdf_scalar(const ScalarFloat *pdf, size_t size) {
        if (size < 2)
            Throw("ContinuousDistribution: needs at least two entries!");

        if (!(m_range.x() < m_range.y()))
            Throw("ContinuousDistribution: invalid range!");

        std::vector<ScalarFloat> cdf(size - 1);
        ScalarVector2u valid = (uint32_t) -1;

        double range = double(m_range.y()) - double(m_range.x()),
               interval_size = range / (size - 1),
               integral = 0.;

        m_max = pdf[0];
        for (size_t i = 0; i < size - 1; ++i) {
            double y0 = (double) pdf[0],
                   y1 = (double) pdf[1];

            double value = 0.5 * interval_size * (y0 + y1);

            m_max = dr::maximum(m_max, (ScalarFloat) y1);

            integral += value;
            cdf[i] = (ScalarFloat) integral;
            pdf++;

            if (y0 < 0. || y1 < 0.) {
                Throw("ContinuousDistribution: entries must be non-negative!");
            } else if (value > 0.) {
                // Determine the first and last wavelength bin with nonzero density
                if (valid.x() == (uint32_t) -1)
                    valid.x() = (uint32_t) i;
                valid.y() = (uint32_t) i;
            }
        }

        if (dr::any(dr::eq(valid, (uint32_t) -1)))
            Throw("ContinuousDistribution: no probability mass found!");

        m_valid = valid;
        dr::make_opaque(m_valid);
        m_cdf = dr::load<FloatStorage>(cdf.data(), size - 1);
        m_integral = dr::gather<Float>(m_cdf, m_valid.y());
        m_normalization = dr::rcp(m_integral);
        m_interval_size = dr::opaque<Float>(interval_size);
        m_inv_interval_size = dr::rcp(m_interval_size);
        m_interval_size_scalar = (ScalarFloat) interval_size;
        dr::make_opaque(m_integral, m_normalization, m_inv_interval_size);
    }

private:
    FloatStorage m_pdf;
    FloatStorage m_cdf;
    Float m_integral = 0.f;
    Float m_normalization = 0.f;
    Float m_interval_size = 0.f;
    ScalarFloat m_interval_size_scalar = 0.f;
    Float m_inv_interval_size = 0.f;
    ScalarVector2f m_range { 0.f, 0.f };
    Vector2u m_valid;
    ScalarFloat m_max = 0.f;
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
template <typename Value> struct IrregularContinuousDistribution {
    using Float = std::conditional_t<dr::is_static_array_v<Value>,
                                     dr::value_t<Value>, Value>;
    using FloatStorage = DynamicBuffer<Float>;
    using UInt32 = dr::uint32_array_t<Float>;
    using Index = dr::uint32_array_t<Value>;
    using Mask = dr::mask_t<Value>;
    using Vector2u       = dr::Array<UInt32, 2>;

    using ScalarFloat = dr::scalar_t<Float>;
    using ScalarVector2f = dr::Array<ScalarFloat, 2>;
    using ScalarVector2u = dr::Array<uint32_t, 2>;

public:
    /// Create an uninitialized IrregularContinuousDistribution instance
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
                                    const ScalarFloat *pdf,
                                    size_t size)
        : m_nodes(dr::load<FloatStorage>(nodes, size)), m_pdf(dr::load<FloatStorage>(pdf, size)) {
        compute_cdf_scalar(nodes, pdf, size);
    }

    /// Update the internal state. Must be invoked when changing the pdf or range.
    void update() {
        if (m_pdf.size() != m_nodes.size())
            Throw("IrregularContinuousDistribution: 'pdf' and 'nodes' size mismatch!");

        if constexpr (dr::is_jit_v<Float>)
            compute_cdf();
        else
            compute_cdf_scalar(m_nodes.data(), m_pdf.data(), m_nodes.size());
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
    Float integral() const { return m_integral; }

    /// \brief Return the normalization factor (i.e. the inverse of \ref sum())
    Float normalization() const { return m_normalization; }

    /// Return the number of discretizations
    size_t size() const { return m_pdf.size(); }

    /// Is the distribution object empty/uninitialized?
    bool empty() const { return m_pdf.empty(); }

    /// Return the range of the distribution
    ScalarVector2f &range() { return m_range; }

    /// Return the range of the distribution (const version)
    const ScalarVector2f &range() const { return m_range; }

    /// Evaluate the unnormalized probability mass function (PDF) at position \c x
    Value eval_pdf(Value x, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

        active &= x >= m_range.x() && x <= m_range.y();

        Index index = dr::binary_search<Index>(
            0, (uint32_t) m_nodes.size(),
            [&](Index index) DRJIT_INLINE_LAMBDA {
                return dr::gather<Value>(m_nodes, index, active) < x;
            }
        );

        index = dr::maximum(dr::minimum(index, (uint32_t) m_nodes.size() - 1u), 1u) - 1u;

        Value x0 = dr::gather<Value>(m_nodes, index,      active),
              x1 = dr::gather<Value>(m_nodes, index + 1u, active),
              y0 = dr::gather<Value>(m_pdf,   index,      active),
              y1 = dr::gather<Value>(m_pdf,   index + 1u, active);

        x = (x - x0) / (x1 - x0);

        return dr::select(active, dr::fmadd(x, y1 - y0, y0), 0.f);
    }

    /// Evaluate the normalized probability mass function (PDF) at position \c x
    Value eval_pdf_normalized(Value x, Mask active) const {
        MI_MASK_ARGUMENT(active);

        return eval_pdf(x, active) * m_normalization;
    }

    /// Evaluate the unnormalized cumulative distribution function (CDF) at position \c p
    Value eval_cdf(Value x, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

        Index index = dr::binary_search<Index>(
            0, (uint32_t) m_nodes.size(),
            [&](Index index) DRJIT_INLINE_LAMBDA {
                return dr::gather<Value>(m_nodes, index, active) < x;
            }
        );

        index = dr::maximum(dr::minimum(index, (uint32_t) m_nodes.size() - 1u), 1u) - 1u;

        Value x0 = dr::gather<Value>(m_nodes, index,      active),
              x1 = dr::gather<Value>(m_nodes, index + 1u, active),
              y0 = dr::gather<Value>(m_pdf,   index,      active),
              y1 = dr::gather<Value>(m_pdf,   index + 1u, active),
              c0 = dr::gather<Value>(m_cdf,   index - 1u, active && index > 0u);

        Value w   = x1 - x0,
              t   = dr::clamp((x - x0) / w, 0.f, 1.f),
              cdf = c0 + w * t * (y0 + .5f * t * (y1 - y0));

        return cdf;
    }

    /// Evaluate the unnormalized cumulative distribution function (CDF) at position \c p
    Value eval_cdf_normalized(Value x, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

        return eval_cdf(x, active) * m_normalization;
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored
     * distribution
     *
     * \param sample
     *     A uniformly distributed sample on the interval [0, 1].
     *
     * \return
     *     The sampled position.
     */
    Value sample(Value sample, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

        sample *= m_integral;

        Index index = dr::binary_search<Index>(
            m_valid.x(), m_valid.y(),
            [&](Index index) DRJIT_INLINE_LAMBDA {
                Value value = dr::gather<Value>(m_cdf, index, active);
                if constexpr (!dr::is_jit_v<Float>) {
                    return value < sample;
                } else {
                    // `m_valid` is not computed in JIT variants
                    return ((value < sample) || (dr::eq(value, 0))) &&
                           dr::neq(value, m_integral);
                }
            }
        );

        Value x0 = dr::gather<Value>(m_nodes, index,      active),
              x1 = dr::gather<Value>(m_nodes, index + 1u, active),
              y0 = dr::gather<Value>(m_pdf,   index,      active),
              y1 = dr::gather<Value>(m_pdf,   index + 1u, active),
              c0 = dr::gather<Value>(m_cdf,   index - 1u, active && index > 0),
              w  = x1 - x0;

        sample = (sample - c0) / w;

        Value t_linear = (y0 - dr::safe_sqrt(dr::sqr(y0) + 2.f * sample * (y1 - y0))) / (y0 - y1),
              t_const  = sample / y0,
              t        = dr::select(dr::eq(y0, y1), t_const, t_linear);

        return dr::fmadd(t, w, x0);
    }

    /**
     * \brief %Transform a uniformly distributed sample to the stored
     * distribution
     *
     * \param sample
     *     A uniformly distributed sample on the interval [0, 1].
     *
     * \return
     *     A tuple consisting of
     *
     *     1. the sampled position.
     *     2. the normalized probability density of the sample.
     */
    std::pair<Value, Value> sample_pdf(Value sample, Mask active = true) const {
        MI_MASK_ARGUMENT(active);

        sample *= m_integral;

        Index index = dr::binary_search<Index>(
            m_valid.x(), m_valid.y(),
            [&](Index index) DRJIT_INLINE_LAMBDA {
                Value value = dr::gather<Value>(m_cdf, index, active);
                if constexpr (!dr::is_jit_v<Float>) {
                    return value < sample;
                } else {
                    // `m_valid` is not computed in JIT variants
                    return ((value < sample) || (dr::eq(value, 0))) &&
                           dr::neq(value, m_integral);
                }
            }
        );

        Value x0 = dr::gather<Value>(m_nodes, index,      active),
              x1 = dr::gather<Value>(m_nodes, index + 1u, active),
              y0 = dr::gather<Value>(m_pdf,   index,      active),
              y1 = dr::gather<Value>(m_pdf,   index + 1u, active),
              c0 = dr::gather<Value>(m_cdf,   index - 1u, active && index > 0),
              w  = x1 - x0;

        sample = (sample - c0) / w;

        Value t_linear = (y0 - dr::safe_sqrt(dr::sqr(y0) + 2.f * sample * (y1 - y0))) / (y0 - y1),
              t_const  = sample / y0,
              t        = dr::select(dr::eq(y0, y1), t_const, t_linear);

        return { dr::fmadd(t, w, x0),
                 dr::fmadd(t, y1 - y0, y0) * m_normalization };
    }

    /**
     * \brief Return the minimum resolution of the discretization
     */
    ScalarFloat interval_resolution() const {
        return m_interval_size;
    }

    ScalarFloat max() const {
        return m_max;
    }

private:
    void compute_cdf() {
        if (m_pdf.size() < 2)
            Throw("IrregularContinuousDistribution: needs at least two entries!");
        if (!dr::all(m_pdf >= 0.f))
            Throw("IrregularContinuousDistribution: entries must be non-negative!");
        if (!dr::any(m_pdf > 0.f))
            Throw("IrregularContinuousDistribution: no probability mass found!");

        uint32_t size = m_pdf.size() - 1;
        UInt32 index_curr = dr::arange<UInt32>(size);
        UInt32 index_next = dr::arange<UInt32>(1, size + 1);

        Float nodes_curr = dr::gather<Float>(m_nodes, index_curr);
        Float nodes_next = dr::gather<Float>(m_nodes, index_next);

        if (dr::any(nodes_next - nodes_curr <= 0))
            Throw("IrregularContinuousDistribution: node positions must be strictly increasing!");

        Float pdf_curr = dr::gather<Float>(m_pdf, index_curr);
        Float pdf_next = dr::gather<Float>(m_pdf, index_next);

        Float interval_integral =
            0.5 * (nodes_next - nodes_curr) * (pdf_curr + pdf_next);
        m_cdf = dr::prefix_sum(interval_integral, false);

        m_range = ScalarVector2f(dr::slice(m_nodes, 0), dr::slice(m_nodes, size));
        m_valid = Vector2u(0, size - 1);
        m_integral = dr::gather<Float>(m_cdf, m_valid.y());
        m_normalization = dr::rcp(m_integral);
        dr::make_opaque(m_valid, m_integral, m_normalization);
        m_interval_size = dr::slice(dr::min(nodes_next - nodes_curr));
        m_max = dr::slice(dr::max(m_pdf));
    }

    void compute_cdf_scalar(const ScalarFloat *nodes, const ScalarFloat *pdf, size_t size) {
        if (size < 2)
            Throw("IrregularContinuousDistribution: needs at least two entries!");

        m_interval_size = dr::Infinity<Float>;
        ScalarVector2u valid = (uint32_t) -1;
        m_range = ScalarVector2f(
             dr::Infinity<ScalarFloat>,
            -dr::Infinity<ScalarFloat>
        );

        double integral = 0.;
        std::vector<ScalarFloat> cdf(size - 1);

        m_max = pdf[0];
        for (size_t i = 0; i < size - 1; ++i) {
            double x0 = (double) nodes[0],
                   x1 = (double) nodes[1],
                   y0 = (double) pdf[0],
                   y1 = (double) pdf[1];

            double value = 0.5 * (x1 - x0) * (y0 + y1);

            m_max = dr::maximum(m_max, (ScalarFloat) y1);

            m_range.x() = dr::minimum(m_range.x(), (ScalarFloat) x0);
            m_range.y() = dr::maximum(m_range.y(), (ScalarFloat) x1);

            m_interval_size = dr::minimum(m_interval_size, (ScalarFloat) x1 - (ScalarFloat) x0);

            integral += value;
            cdf[i] = (ScalarFloat) integral;
            pdf++; nodes++;

            if (!(x1 > x0)) {
                Throw("IrregularContinuousDistribution: node positions must be strictly increasing!");
            } else if (y0 < 0. || y1 < 0.) {
                Throw("IrregularContinuousDistribution: entries must be non-negative!");
            } else if (value > 0.) {
                // Determine the first and last wavelength bin with nonzero density
                if (valid.x() == (uint32_t) -1)
                    valid.x() = (uint32_t) i;
                valid.y() = (uint32_t) i;
            }
        }

        if (dr::any(dr::eq(valid, (uint32_t) -1)))
            Throw("IrregularContinuousDistribution: no probability mass found!");

        m_valid = valid;
        dr::make_opaque(m_valid);
        m_cdf = dr::load<FloatStorage>(cdf.data(), size);
        m_integral = dr::gather<Float>(m_cdf, m_valid.y());
        m_normalization = dr::rcp(m_integral);
        dr::make_opaque(m_integral, m_normalization);
    }

private:
    FloatStorage m_nodes;
    FloatStorage m_pdf;
    FloatStorage m_cdf;
    Float m_integral = 0.f;
    Float m_normalization = 0.f;
    ScalarVector2f m_range { 0.f, 0.f };
    Vector2u m_valid;
    ScalarFloat m_interval_size = 0.f;
    ScalarFloat m_max = 0.f;
};

template <typename Value>
std::ostream &operator<<(std::ostream &os, const DiscreteDistribution<Value> &distr) {
    os << "DiscreteDistribution[" << std::endl
        << "  size = " << distr.size() << "," << std::endl
        << "  sum = " << distr.sum() << "," << std::endl
        << "  pmf = " << distr.pmf() << std::endl
        << "]";
    return os;
}

template <typename Value>
std::ostream &operator<<(std::ostream &os, const ContinuousDistribution<Value> &distr) {
    os << "ContinuousDistribution[" << std::endl
        << "  size = " << distr.size() << "," << std::endl
        << "  range = " << distr.range() << "," << std::endl
        << "  integral = " << distr.integral() << "," << std::endl
        << "  pdf = " << distr.pdf() << std::endl
        << "]";
    return os;
}

template <typename Value>
std::ostream &operator<<(std::ostream &os, const IrregularContinuousDistribution<Value> &distr) {
    os << "IrregularContinuousDistribution[" << std::endl
        << "  size = " << distr.size() << "," << std::endl
        << "  nodes = " << distr.nodes() << "," << std::endl
        << "  integral = " << distr.integral() << "," << std::endl
        << "  pdf = " << distr.pdf() << "," << std::endl
        << "]";
    return os;
}

NAMESPACE_END(mitsuba)
