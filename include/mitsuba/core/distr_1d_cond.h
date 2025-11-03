#pragma once

#include <drjit/dynamic.h>
#include <drjit/tensor.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/vector.h>
#include <drjit/traversable_base.h>

NAMESPACE_BEGIN(mitsuba)

/**
 *
 * \brief Conditional 1D irregular distribution
 *
 * Similarly to the irregular 1D distribution, this class represents a
 * 1-dimensional irregular distribution. It differs in the fact that it has N-1
 * extra dimensions on which it is conditioned.
 *
 * As an example, assume you have a 3D distribution P(x,y,z), with leading
 * dimension X. This class would allow you to obtain the linear interpolated
 * value of the PDF for \c x given \c y and \c z. Additionally, it allows you to
 * sample from the distribution P(x|Y=y,Z=z) for a given \c y and \c z.
 *
 * It assumes every conditioned PDF has the same size.
 *
 * If the user requests a method that needs the integral, it will automatically
 * schedule its computation on-the-fly.
 *
 * This distribution can be used in the context of spectral rendering, where
 * each wavelength conditions the underlying distribution.
 */
template <typename Value>
class ConditionalIrregular1D : drjit::TraversableBase {
    using Float        = std::conditional_t<dr::is_static_array_v<Value>,
                                            dr::value_t<Value>, Value>;
    using FloatStorage = DynamicBuffer<Float>;
    using Index        = dr::uint32_array_t<Value>;
    using UInt32       = dr::uint32_array_t<Float>;
    using IndexFloat   = dr::uint32_array_t<Float>;
    using Mask         = dr::mask_t<Value>;
    using TensorXf     = dr::Tensor<FloatStorage>;

    using ScalarFloat    = dr::scalar_t<Float>;
    using ScalarVector2f = Vector<ScalarFloat, 2>;
    using ScalarVector2u = Vector<uint32_t, 2>;
    using ScalarUInt32   = dr::uint32_array_t<ScalarFloat>;

public:
    ConditionalIrregular1D() {};

    /**
     * \brief Construct a conditional irregular 1D distribution.
     *
     * \param nodes
     *     Points where the leading dimension N is defined.
     *
     * \param pdf
     *     Flattened array of shape [D1, D2, ..., Dn, N], containing the PDFs.
     *
     * \param nodes_cond
     *     Arrays containing points where each conditional dimension is
     *     evaluated.
     */
    ConditionalIrregular1D(const FloatStorage &nodes, const FloatStorage &pdf,
                           const std::vector<FloatStorage> &nodes_cond)
        : m_nodes(nodes), m_nodes_cond(nodes_cond) {

        // Update tensor with the information
        std::vector<size_t> shape;
        for (size_t i = 0; i < m_nodes_cond.size(); i++)
            shape.push_back(dr::width(m_nodes_cond[i]));
        shape.push_back(dr::width(m_nodes));

        // Initialize tensor storing the distribution values
        m_pdf = TensorXf(pdf, shape.size(), shape.data());
    }

    /**
     * \brief Construct a conditional irregular 1D distribution.
     *
     * \param nodes
     *     Points where the leading dimension N is defined.
     *
     * \param pdf
     *     Tensor containing the values of the PDF of shape [D1, D2, ..., Dn,
     *     N].
     *
     * \param nodes_cond
     *     Arrays containing points where each conditional dimension is
     *     evaluated.
     */
    ConditionalIrregular1D(const FloatStorage &nodes, const TensorXf &pdf,
                           const std::vector<FloatStorage> &nodes_cond)
        : m_nodes(nodes), m_pdf(pdf), m_nodes_cond(nodes_cond) {}

    /**
     * \brief Construct a conditional irregular 1D distribution
     *
     * \param nodes
     *     Points where the PDFs are evaluated.
     *
     * \param size_nodes
     *     Size of the nodes array.
     *
     * \param pdf
     *     Flattened array of shape [D1, D2, ..., Dn, N], containing the PDFs.
     *
     * \param size_pdf
     *     Size of the pdf array.
     *
     * \param nodes_cond
     *     Arrays containing points where the conditional is evaluated.
     *
     * \param sizes_cond
     *     Array with the sizes of the conditional nodes arrays.
     */
    ConditionalIrregular1D(const ScalarFloat *nodes, const size_t size_nodes,
                           const ScalarFloat *pdf, const size_t size_pdf,
                           const std::vector<ScalarFloat *> &nodes_cond,
                           const std::vector<size_t> &sizes_cond)
        : m_nodes(dr::load<FloatStorage>(nodes, size_nodes)) {
        for (size_t i = 0; i < nodes_cond.size(); i++) {
            m_nodes_cond.push_back(
                dr::load<FloatStorage>(nodes_cond[i], sizes_cond[i]));
        }

        // Update tensor with the information
        std::vector<size_t> shape;
        for (size_t i = 0; i < m_nodes_cond.size(); i++)
            shape.push_back(dr::width(m_nodes_cond[i]));
        shape.push_back(dr::width(m_nodes));

        // Initialize tensor storing the distribution values
        m_pdf = TensorXf(dr::load<FloatStorage>(pdf, size_pdf), shape.size(),
                         shape.data());
    }

    /**
     * \brief Update the internal state.
     *
     * Must be invoked when PDF is changed.
     */
    void update() {
        if constexpr (dr::is_jit_v<Float>) {
            compute_cdf();
        } else {
            compute_cdf_scalar(m_nodes.data(), m_nodes.size(), m_pdf.data(),
                               m_pdf.size());
        }
    }

    /**
     * \brief Evaluate the unnormalized probability density function (PDF) at
     * position \c pos, conditioned on \c cond.
     *
     * \param pos
     *     Position where the PDF is evaluated.
     *
     * \param cond
     *     Array of values where the conditionals are evaluated.
     *
     * \return
     *     The value of the PDF at position \c pos, conditioned on \c cond.
     */
    Value eval_pdf(Value pos, std::vector<Value> &cond,
                   Mask active = true) const {
        if (cond.size() != m_nodes_cond.size())
            Log(Error, "The number of conditionals should be %u instead of %u",
                m_nodes_cond.size(), cond.size());

        auto [value, integral] = lookup(pos, cond, 0, 0, active);
        return value;
    }

    /**
     * \brief Evaluate the normalized probability density function (PDF) at
     * position \c pos, conditioned on \c cond.
     *
     * \param pos
     *     Position where the PDF is evaluated.
     *
     * \param cond
     *     Array of values where the conditionals are evaluated.
     *
     * \return
     *     The value of the normalized PDF at position \c pos, conditioned
     *     on \c cond.
     */
    Value eval_pdf_normalized(Value pos, std::vector<Value> &cond,
                              Mask active = true) const {
        if (cond.size() != m_nodes_cond.size())
            Log(Error, "The number of conditionals should be %u instead of %u",
                m_nodes_cond.size(), cond.size());

        ensure_cdf_computed();

        auto [value, integral] = lookup(pos, cond, 0, 0, active);
        return value * dr::rcp(integral);
    }

    /**
     * \brief Sample the distribution given a uniform sample \c u, conditioned
     * on \c cond.
     *
     * \param u
     *     Uniform sample.
     *
     * \param cond
     *     Conditionals where the PDF is sampled.
     *
     * \return
     *     A pair where the first element is the sampled position and the
     *     second element the value of the normalized PDF at that position
     *     conditioned on \c cond.
     */
    std::pair<Value, Value> sample_pdf(Value u, std::vector<Value> &cond,
                                       Mask active) const {
        if (cond.size() != m_nodes_cond.size())
            Log(Error, "The number of conditionals should be %u instead of %u",
                m_nodes_cond.size(), cond.size());

        ensure_cdf_computed();

        std::vector<Index> indices(1 << m_nodes_cond.size());
        std::vector<Value> weights(1 << m_nodes_cond.size());

        lookup_fill(cond, 0, 1.f, indices, weights, 0, 0, active);
        return lookup_sample(u, indices, weights, true, active);
    }

    /// Is the distribution object empty/uninitialized?
    bool empty() const { return m_pdf.empty(); }

    /// Return the maximum value of the distribution
    ScalarFloat max() const {
        ensure_cdf_computed();
        return m_max;
    }

    /**
     * \brief Return the integral of the distribution conditioned on \c cond.
     *
     * \param cond
     *     Conditionals that define the distribution.
     *
     * \return
     *     The integral of the distribution.
     */
    Value integral(std::vector<Value> &cond) const {
        ensure_cdf_computed();

        const Value dummy_pos  = dr::gather<Value>(m_nodes, Index(0));
        auto [value, integral] = lookup(dummy_pos, cond, 0, 0, true);
        return integral;
    }

    /// Return the underlying tensor storing the distribution values.
    TensorXf &pdf() { return m_pdf; }
    const TensorXf &pdf() const { return m_pdf; }

    /// Return the nodes of the underlying discretization.
    FloatStorage &nodes() { return m_nodes; }
    const FloatStorage &nodes() const { return m_nodes; }

    /// Return the conditional nodes of the underlying discretization.
    std::vector<FloatStorage> &nodes_cond() { return m_nodes_cond; }
    const std::vector<FloatStorage> &nodes_cond() const { return m_nodes_cond; }

    /// Return the CDF.
    FloatStorage &cdf_array() { return m_cdf; }
    const FloatStorage &cdf_array() const { return m_cdf; }

    /// Return the integral array.
    FloatStorage &integral_array() { return m_integral; }
    const FloatStorage &integral_array() const { return m_integral; }

private:
    DRJIT_INLINE void ensure_cdf_computed() const {
        if (unlikely(m_cdf.empty())) {
            const_cast<ConditionalIrregular1D *>(this)->update();
        }
    }

    std::pair<Value, Value> lookup(Value pos, std::vector<Value> &cond_,
                                   Index index_, ScalarUInt32 dim,
                                   Mask active) const {
        if (dim > m_nodes_cond.size()) {
            Mask valid =
                active && (index_ >= 0) && (index_ < dr::width(m_pdf.array()));
            return { dr::gather<Value>(m_pdf.array(), index_, valid), 0.f };
        }

        const FloatStorage *data;
        size_t length;
        Value cond;
        if (dim == m_nodes_cond.size()) {
            data   = &m_nodes;
            length = dr::width(*data);
            cond   = pos;
        } else {
            data   = &m_nodes_cond[dim];
            length = dr::width(*data);
            cond   = cond_[dim];
        }

        // Check cond is inside the range (disable otherwise)
        if (length > 1)
            active &= (cond >= dr::gather<Value>(*data, Index(0))) &&
                      (cond <= dr::gather<Value>(*data, Index(length - 1)));

        Index bin_index = dr::binary_search<Index>(
            0, (uint32_t) length, [&](Index index) DRJIT_INLINE_LAMBDA {
                return dr::gather<Value>(*data, index, active) < cond;
            });

        bin_index =
            dr::maximum(dr::minimum(bin_index, (uint32_t) length - 1u), 1u) -
            1u;

        Value w0, w1;
        if (length > 1) {
            Value b0 = dr::gather<Value>(*data, bin_index, active);
            Value b1 = dr::gather<Value>(*data, bin_index + 1, active);
            w1       = dr::clip((cond - b0) * dr::rcp(b1 - b0), 0.f, 1.f);
            w0       = 1.0f - w1;
        } else {
            w0 = 1.f;
            w1 = 0.f;
        }

        Index index1;
        Index index2;
        if (dim == 0) {
            index1 = bin_index;
            index2 = bin_index + 1;
        } else {
            index1 = index_ * length + bin_index;
            index2 = index_ * length + bin_index + 1;
        }

        Value v0 = 0.f, v1 = 0.f, integral0 = 0.f, integral1 = 0.f;
        auto data0 = lookup(pos, cond_, index1, dim + 1, active);
        v0         = std::get<0>(data0);
        integral0  = std::get<1>(data0);

        if (length > 1) {
            auto data1 = lookup(pos, cond_, index2, dim + 1, active);
            v1         = std::get<0>(data1);
            integral1  = std::get<1>(data1);
        }

        // Final case : read from memory the integrals
        if (!m_integral.empty() && dim == (m_nodes_cond.size() - 1)) {
            integral0 = dr::gather<Value>(m_integral, index1, active);
            if (length > 1)
                integral1 = dr::gather<Value>(m_integral, index2, active);
        }

        return { dr::fmadd(v1, w1, dr::fmadd(v0, -w1, v0)),
                 dr::fmadd(integral1, w1,
                           dr::fmadd(integral0, -w1, integral0)) };
    }

    void lookup_fill(std::vector<Value> &cond_, Index index_, Value weight_,
                     std::vector<Index> &index_res_array_,
                     std::vector<Value> &weight_res_array_,
                     ScalarUInt32 res_index_, ScalarUInt32 dim,
                     Mask active) const {
        if (dim == m_nodes_cond.size()) {
            index_res_array_[res_index_]  = index_;
            weight_res_array_[res_index_] = weight_;
            return;
        }

        const FloatStorage *data = &(m_nodes_cond[dim]);
        size_t length            = dr::width(m_nodes_cond[dim]);
        Value cond               = cond_[dim];

        Index bin_index = dr::binary_search<Index>(
            0, (uint32_t) length, [&](Index index) DRJIT_INLINE_LAMBDA {
                return dr::gather<Value>(*data, index,
                                         active & (weight_ > 0.f)) < cond;
            });

        bin_index =
            dr::maximum(dr::minimum(bin_index, (uint32_t) length - 1u), 1u) -
            1u;

        Index index1;
        Index index2;
        if (dim == 0) {
            index1 = bin_index;
            index2 = bin_index + 1;
        } else {
            index1 = index_ * length + bin_index;
            index2 = index_ * length + bin_index + 1;
        }

        Value w0, w1;
        if (length > 1) {
            Value b0 =
                dr::gather<Value>(*data, bin_index, active & (weight_ > 0.f));
            Value b1 = dr::gather<Value>(*data, bin_index + 1,
                                         active & (weight_ > 0.f));
            w1       = dr::clip((cond - b0) / (b1 - b0), 0.f, 1.f);
            w0       = 1.0f - w1;
        } else {
            w0 = 1.f;
            w1 = 0.f;
        }

        lookup_fill(cond_, index1, w0 * weight_, index_res_array_,
                    weight_res_array_, 2 * res_index_, dim + 1, active);
        lookup_fill(cond_, index2, w1 * weight_, index_res_array_,
                    weight_res_array_, 2 * res_index_ + 1, dim + 1, active);
    }

    std::pair<Value, Value> lookup_sample(Value u,
                                          std::vector<Index> &index_res_array_,
                                          std::vector<Value> &weight_res_array_,
                                          bool normalize, Mask active) const {
        // Compute the value of the CDF which we are looking for
        Value cond = 0.0;
        for (ScalarUInt32 i = 0; i < index_res_array_.size(); i++) {
            cond = cond +
                   u * weight_res_array_[i] *
                       dr::gather<Value>(m_integral, index_res_array_[i],
                                         active & (weight_res_array_[i] > 0.f));
        }

        // This is the length of the CDF, which has one element less than the
        // nodes
        size_t length_cdf = dr::width(m_nodes) - 1;

        // On the fly generate interpolated CDF and search for its entry
        Index bin_index = dr::binary_search<Index>(
            0, (uint32_t) length_cdf, [&](Index index) DRJIT_INLINE_LAMBDA {
                Value val = 0.;
                for (ScalarUInt32 i = 0; i < index_res_array_.size(); i++) {
                    val =
                        val +
                        weight_res_array_[i] *
                            dr::gather<Value>(
                                m_cdf,
                                index + Index(index_res_array_[i] * length_cdf),
                                active & (weight_res_array_[i] > 0.f));
                }
                return val < cond;
            });

        // Obtain the sampled positions
        Value x0 = dr::gather<Value>(m_nodes, bin_index, active);
        Value x1 = dr::gather<Value>(m_nodes, bin_index + 1, active);

        Value y0 = 0.f;
        Value y1 = 0.f;
        Value c0 = 0.f;

        for (ScalarUInt32 i = 0; i < index_res_array_.size(); i++) {
            Index index_tmp =
                index_res_array_[i] * dr::width(m_nodes) + bin_index;
            y0 = y0 +
                 weight_res_array_[i] *
                     dr::gather<Value>(m_pdf.array(), index_tmp,
                                       active & (weight_res_array_[i] > 0.f));
            y1 = y1 +
                 weight_res_array_[i] *
                     dr::gather<Value>(m_pdf.array(), index_tmp + 1,
                                       active & (weight_res_array_[i] > 0.f));

            Index c0_index = index_res_array_[i] * length_cdf + bin_index;
            c0             = c0 + weight_res_array_[i] *
                          dr::gather<Value>(m_cdf, c0_index - 1,
                                            active & (bin_index > 0) &
                                                (weight_res_array_[i] > 0.f));
        }

        Value w     = x1 - x0;
        Value value = (cond - c0) / w;

        Value t_linear =
            (y0 - dr::safe_sqrt(dr::square(y0) + 2.f * value * (y1 - y0))) /
            (y0 - y1);
        Value t_const = value / y0;
        Value t       = dr::select(y0 == y1, t_const, t_linear);

        Value integral = 0.0;
        if (normalize) {
            for (ScalarUInt32 i = 0; i < index_res_array_.size(); i++) {
                integral +=
                    weight_res_array_[i] *
                    dr::gather<Value>(m_integral, index_res_array_[i],
                                      active & (weight_res_array_[i] > 0.f));
            }
        }

        return { dr::fmadd(t, w, x0),
                 dr::fmadd(t, y1 - y0, y0) * dr::rcp(integral) };
    }

    void compute_cdf() {
        if (m_pdf.array().size() < 2)
            Throw("ConditionalIrregular1D: needs at least two entries!");
#ifndef NDEBUG
        if (!dr::all(m_pdf.array() >= 0.f))
            Throw("ConditionalIrregular1D: entries must be "
                  "non-negative!");
        if (!dr::any(m_pdf.array() > 0.f))
            Throw("ConditionalIrregular1D: no probability mass found!");
#endif

        const size_t size_nodes = dr::width(m_nodes);
        const size_t size_pdf   = dr::width(m_pdf.array());
        size_t size_cond        = 1;
        for (size_t i = 0; i < m_nodes_cond.size(); i++)
            size_cond *= dr::width(m_nodes_cond[i]);

        if (size_pdf != size_nodes * size_cond)
            Log(Error,
                "ConditionalIrregular1D: %f (size_pdf) != %f (size_nodes) * %f "
                "(size_cond)",
                size_pdf, size_nodes, size_cond);

        m_max = dr::slice(dr::max(m_pdf.array()));

        const size_t size = dr::width(m_nodes) - 1;
        UInt32 index_curr = dr::arange<UInt32>(size);
        UInt32 index_next = dr::arange<UInt32>(1, size + 1);
        index_curr        = dr::tile(index_curr, size_cond);
        index_next        = dr::tile(index_next, size_cond);

        Float nodes_curr = dr::gather<Float>(m_nodes, index_curr);
        Float nodes_next = dr::gather<Float>(m_nodes, index_next);

        UInt32 offset =
            dr::repeat(dr::arange<UInt32>(size_cond) * dr::width(m_nodes),
                       dr::width(m_nodes) - 1);

        Float pdf_curr = dr::gather<Float>(m_pdf.array(), index_curr + offset);
        Float pdf_next = dr::gather<Float>(m_pdf.array(), index_next + offset);
        Float interval_integral =
            0.5 * (nodes_next - nodes_curr) * (pdf_curr + pdf_next);

        m_cdf = dr::block_prefix_sum(interval_integral, dr::width(m_nodes) - 1,
                                     false);

        UInt32 indexes_integral =
            dr::arange<UInt32>(size_cond) * (dr::width(m_nodes) - 1) +
            dr::width(m_nodes) - 2;
        m_integral = dr::gather<Float>(m_cdf, indexes_integral, true);

        dr::schedule(m_cdf, m_integral);
    }

    void compute_cdf_scalar(const ScalarFloat *nodes, size_t size_nodes,
                            const ScalarFloat *pdf, size_t size_pdf) {
        if (unlikely(empty()))
            return;

        size_t size_cond = 1;
        for (size_t i = 0; i < m_nodes_cond.size(); i++)
            size_cond *= dr::width(m_nodes_cond[i]);

        size_t size_cdf = (size_nodes - 1) * size_cond;
        std::vector<ScalarFloat> cdf(size_cdf, 0.f);
        std::vector<ScalarFloat> integral(size_cond, 0.f);

        if (size_pdf != size_nodes * size_cond)
            Log(Error,
                "ConditionalIrregular1D: size_pdf != size_nodes * size_cond");

        ScalarFloat integral_val = 0.0;

        m_max = pdf[0];
        for (size_t i = 0; i < size_cond; i++) {
            integral_val         = 0.0;
            ScalarVector2u valid = (uint32_t) -1;
            for (size_t j = 0; j < size_nodes - 1; j++) {
                ScalarFloat x0 = nodes[j];
                ScalarFloat x1 = nodes[j + 1];

                if (x0 >= x1)
                    Log(Error, "ConditionalIrregular1D: Nodes must be strictly "
                               "increasing");

                ScalarFloat y0 = pdf[i * size_nodes + j];
                ScalarFloat y1 = pdf[i * size_nodes + j + 1];

                if (y0 < 0.f || y1 < 0.f)
                    Log(Error, "ConditionalIrregular1D: Entries of the "
                               "conditioned PDFs must be non-negative!");

                m_max = dr::maximum(m_max, (ScalarFloat) y1);

                ScalarFloat value = 0.5f * (x1 - x0) * (y0 + y1);

                if (value > 0.f) {
                    // Determine the first and last wavelength bin with nonzero
                    // density
                    if (valid.x() == (uint32_t) -1)
                        valid.x() = (uint32_t) j;
                    valid.y() = (uint32_t) j;
                }

                integral_val += value;

                cdf[i * (size_nodes - 1) + j] = integral_val;
            }

            if (dr::any(valid == (uint32_t) -1))
                Log(Error, "ConditionalIrregular1D: No probability mass found "
                           "for one conditioned PDF");

            integral[i] = integral_val;
        }

        m_cdf      = dr::load<FloatStorage>(cdf.data(), cdf.size());
        m_integral = dr::load<FloatStorage>(integral.data(), integral.size());
    }

private:
    FloatStorage m_nodes;
    TensorXf m_pdf;
    std::vector<FloatStorage> m_nodes_cond;
    FloatStorage m_cdf;
    FloatStorage m_integral;
    ScalarFloat m_max = 0.f;

    MI_TRAVERSE_CB(drjit::TraversableBase, m_nodes, m_pdf, m_nodes_cond, m_cdf,
                   m_integral)
};

template <typename Value>
std::ostream &operator<<(std::ostream &os,
                         const ConditionalIrregular1D<Value> & /*distr*/) {
    os << "ConditionalIrregular1D[]" << std::endl;
    return os;
}

/**
 *
 * \brief Conditional 1D regular distribution.
 *
 *
 * Similar to the regular 1D distribution, but this class represents an
 * N-Dimensional regular one (with the extra conditional dimensions being also
 * regular).
 *
 * As an example, assume you have a 3D distribution P(x,y,z), with leading
 * dimension X. This class would allow you to obtain the linear interpolated
 * value of the PDF for \c x given \c y and \c z. Additionally, it allows you to
 * sample from the distribution P(x|Y=y,Z=z) for a given \c y and \c z.
 *
 * It assumes every conditioned PDF has the same size.
 * If the user requests a method that needs the integral, it will schedule its
 * computation.
 *
 * This distribution can be used in the context of spectral rendering, where
 * each wavelength conditions the underlying distribution.
 *
 */
template <typename Value> class ConditionalRegular1D : drjit::TraversableBase {
    using Float        = std::conditional_t<dr::is_static_array_v<Value>,
                                            dr::value_t<Value>, Value>;
    using FloatStorage = DynamicBuffer<Float>;
    using Index        = dr::uint32_array_t<Value>;
    using UInt32       = dr::uint32_array_t<Float>;
    using IndexFloat   = dr::uint32_array_t<Float>;
    using Mask         = dr::mask_t<Value>;
    using TensorXf     = dr::Tensor<FloatStorage>;

    using ScalarFloat    = dr::scalar_t<Float>;
    using ScalarVector2f = Vector<ScalarFloat, 2>;
    using ScalarVector2u = Vector<uint32_t, 2>;
    using ScalarUInt32   = dr::uint32_array_t<ScalarFloat>;

    using Vector2f = Vector<Float, 2>;

public:
    ConditionalRegular1D() {};

    /**
     * \brief Construct a conditional regular 1D distribution
     *
     * \param pdf
     *     Flattened array of shape [D1, D2, ..., Dn, N] containing the PDFs.
     *
     * \param range
     *     Range where the leading dimension N is defined.
     *
     * \param range_cond
     *     Array of ranges where the dimensional conditionals are defined.
     *
     * \param size_cond
     *     Array with the size of each conditional dimension.
     */
    ConditionalRegular1D(const FloatStorage &pdf, const ScalarVector2f &range,
                         const std::vector<ScalarVector2f> &range_cond,
                         const std::vector<ScalarUInt32> &size_cond)
        : m_range(range), m_size_cond(size_cond), m_range_cond(range_cond) {
        // Update tensor with the information
        std::vector<size_t> shape;
        size_t total_size_cond = 1;
        for (size_t i = 0; i < m_size_cond.size(); i++) {
            size_t s = m_size_cond[i];
            shape.push_back(s);
            total_size_cond *= s;
            if (s < 2)
                Log(Error,
                    "Dimension %u should have at least size 2 instead of %u", i,
                    s);
        }
        shape.push_back(dr::width(pdf) / total_size_cond);

        m_pdf = TensorXf(pdf, shape.size(), shape.data());

        m_size_nodes = dr::width(pdf) / total_size_cond;
        if (m_size_nodes < 2)
            Log(Error,
                "The number of the leading dimension should have at least size "
                "2 instead of %u",
                m_size_nodes);

        prepare_distribution();
    }

    /**
     * \brief Construct a conditional regular 1D distribution.
     *
     * \param pdf
     *     Tensor containing the values of the PDF of shape [D1, D2, ..., Dn, N].
     *
     * \param range
     *     Range where the leading dimension N is defined.
     *
     * \param range_cond
     *     Array of ranges where the dimensional conditionals are defined.
     */
    ConditionalRegular1D(const TensorXf &pdf, const ScalarVector2f &range,
                         const std::vector<ScalarVector2f> &range_cond)
        : m_range(range), m_pdf(pdf), m_range_cond(range_cond) {

        m_size_cond.reserve(m_pdf.ndim() - 1);
        for (size_t i = 0; i < m_pdf.ndim() - 1; i++) {
            const size_t s = m_pdf.shape(i);
            m_size_cond.push_back(s);
            if (s < 2)
                Log(Error,
                    "Dimension %u should have at least size 2 instead of %u", i,
                    s);
        }

        m_size_nodes = m_pdf.shape(m_pdf.ndim() - 1);
        if (m_size_nodes < 2)
            Log(Error,
                "The number of the leading dimension should have at least size "
                "2 instead of %u",
                m_size_nodes);

        prepare_distribution();
    }

    ConditionalRegular1D(const ScalarFloat *pdf, const size_t size_pdf,
                         const ScalarVector2f &range,
                         const std::vector<ScalarVector2f> &range_cond,
                         const std::vector<size_t> &size_cond)
        : m_range(range) {
        for (size_t i = 0; i < size_cond.size(); i++) {
            m_size_cond.push_back(size_cond[i]);
            m_range_cond.push_back(range_cond[i]);
            if (size_cond[i] < 2)
                Log(Error,
                    "Dimension %u should have at least size 2 instead of %u", i,
                    size_cond[i]);
        }

        // Update tensor with the information
        std::vector<size_t> shape;
        size_t total_size_cond = 1;
        for (size_t i = 0; i < m_size_cond.size(); i++) {
            size_t s = m_size_cond[i];
            shape.push_back(s);
            total_size_cond *= s;
            if (s < 2)
                Log(Error,
                    "Dimension %u should have at least size 2 instead of %u", i,
                    s);
        }
        m_size_nodes = dr::width(pdf) / total_size_cond;
        shape.push_back(m_size_nodes);

        if (m_size_nodes < 2)
            Log(Error,
                "The number of the leading dimension should have at least size "
                "2 instead of %u",
                m_size_nodes);

        m_pdf = TensorXf(dr::load<FloatStorage>(pdf, size_pdf), shape.size(),
                         shape.data());

        prepare_distribution();
    }

    /**
     * \brief Update the internal state. Must be invoked when changing the
     * distribution.
     */
    void update() {
        prepare_distribution();
        prepare_cdf();
    }

    /**
     * \brief Evaluate the unnormalized probability density function (PDF) at
     * position \c x, conditioned on \c cond.
     *
     * \param x
     *     Position where the PDF is evaluated.
     *
     * \param cond
     *     Conditionals where the PDF is evaluated.
     */
    Value eval_pdf(Value x, std::vector<Value> &cond,
                   Mask active = true) const {
        Mask active2 = active && (x >= m_range.x()) && (x <= m_range.y());

        if (cond.size() != m_size_cond.size())
            Log(Error, "The number of conditionals should be %u instead of %u",
                m_size_cond.size(), cond.size());

        auto [value, integral] = lookup(x, cond, 0, 0, active2);
        return value;
    }

    /**
     * \brief Evaluate the normalized probability density function (PDF) at
     * position \c x, conditioned on \c cond.
     *
     * \param x
     *     Position where the PDF is evaluated.
     * \param cond
     *     Conditionals where the PDF is evaluated.
     */
    Value eval_pdf_normalized(Value x, std::vector<Value> &cond,
                              Mask active = true) const {
        if (cond.size() != m_size_cond.size())
            Log(Error, "The number of conditionals should be %u instead of %u",
                m_size_cond.size(), cond.size());

        ensure_cdf_computed();

        Mask active2 = active && (x >= m_range.x()) && (x <= m_range.y());
        auto [value, integral] = lookup(x, cond, 0, 0, active2);
        return value * dr::rcp(integral);
    }

    /**
     * \brief Sample the distribution given a uniform sample \c u, conditioned
     * on \c cond.
     *
     * \param u
     *     Uniform sample.
     *
     * \param cond
     *     Conditionals where the PDF is sampled.
     */
    std::pair<Value, Value> sample_pdf(Value u, std::vector<Value> &cond,
                                       Mask active) const {
        if (cond.size() != m_size_cond.size())
            Log(Error, "The number of conditionals should be %u instead of %u",
                m_size_cond.size(), cond.size());

        ensure_cdf_computed();

        std::vector<Index> indices(1 << m_size_cond.size());
        std::vector<Value> weights(1 << m_size_cond.size());

        lookup_fill(cond, 0, 1.f, indices, weights, 0, 0, active);
        return lookup_sample(u, indices, weights, active);
    }

    /// Is the distribution object empty/uninitialized?
    bool empty() const { return m_pdf.empty(); }

    /// Return the maximum value of the distribution
    ScalarFloat max() const {
        ensure_cdf_computed();
        return m_max;
    }

    /**
     * \brief Return the integral of the distribution conditioned on \c cond
     */
    Value integral(std::vector<Value> &cond) const {
        ensure_cdf_computed();
        auto [value, integral] = lookup(m_range.x(), cond, 0, 0, true);
        return integral;
    }

    /// Return the underlying tensor storing the distribution values
    TensorXf &pdf() { return m_pdf; }
    const TensorXf &pdf() const { return m_pdf; }

    /// Return the range where the distribution is defined
    ScalarVector2f &range() { return m_range; }
    const ScalarVector2f &range() const { return m_range; }

    /// Return the conditional range where the distribution is defined
    std::vector<ScalarVector2f> &range_cond() { return m_range_cond; }
    const std::vector<ScalarVector2f> &range_cond() const {
        return m_range_cond;
    }

    /// Return the cdf array of the distribution
    FloatStorage &cdf_array() { return m_cdf; }
    const FloatStorage &cdf_array() const { return m_cdf; }

    /// Return the integral array of the distribution
    FloatStorage &integral_array() { return m_integral; }
    const FloatStorage &integral_array() const { return m_integral; }

private:
    DRJIT_INLINE void ensure_cdf_computed() const {
        if (unlikely(m_cdf.empty())) {
            const_cast<ConditionalRegular1D *>(this)->prepare_cdf();
        }
    }

    DRJIT_INLINE void prepare_cdf() {
        if constexpr (dr::is_jit_v<Float>) {
            compute_cdf();
        } else {
            compute_cdf_scalar(m_pdf.data(), m_pdf.size());
        }
    }

    DRJIT_INLINE void prepare_distribution() {
        m_interval_scalar =
            (m_range.y() - m_range.x()) / ScalarFloat(m_size_nodes - 1);

        m_interval     = dr::opaque<Float>(m_interval_scalar);
        m_inv_interval = dr::opaque<Float>(dr::rcp(m_interval_scalar));

        m_inv_interval_cond.clear();
        m_inv_interval_cond.reserve(m_range_cond.size());
        for (size_t i = 0; i < m_range_cond.size(); i++) {
            ScalarFloat tmp = (m_range_cond[i].y() - m_range_cond[i].x()) /
                              ScalarFloat(m_size_cond[i] - 1);
            m_inv_interval_cond.push_back(dr::opaque<Float>(dr::rcp(tmp)));
        }
    }

    std::pair<Value, Value> lookup(Value pos, std::vector<Value> &cond_,
                                   Index index_, ScalarUInt32 dim,
                                   Mask active) const {
        if (dim > m_range_cond.size()) {
            Mask valid =
                active && (index_ >= 0) && (index_ < dr::width(m_pdf.array()));
            return { dr::gather<Value>(m_pdf.array(), index_, valid), 0.f };
        }

        Index bin_index;
        Value w0, w1;
        size_t length;

        size_t size_nodes;
        Float inv_interval;
        Vector2f range_nodes;
        Value value;
        if (dim == m_range_cond.size()) {
            size_nodes   = m_size_nodes;
            inv_interval = m_inv_interval;
            range_nodes  = m_range;
            value        = pos;
        } else {
            size_nodes   = m_size_cond[dim];
            inv_interval = m_inv_interval_cond[dim];
            range_nodes  = m_range_cond[dim];
            value        = cond_[dim];
        }

        Value new_x = (value - range_nodes.x()) * inv_interval;
        Index index =
            dr::clip(Index(dr::floor(new_x)), 0u, uint32_t(size_nodes - 2));
        w1 = new_x - Value(index);
        w0 = 1.f - w1;

        // Check bounds
        active &= (value >= range_nodes.x()) && (value <= range_nodes.y());
        bin_index = index;

        length = size_nodes;

        Index index1;
        Index index2;
        if (dim == 0) {
            index1 = bin_index;
            index2 = bin_index + 1;
        } else {
            index1 = index_ * length + bin_index;
            index2 = index_ * length + bin_index + 1;
        }

        Value v0 = 0.f, v1 = 0.f, integral0 = 0.f, integral1 = 0.f;
        auto data  = lookup(pos, cond_, index1, dim + 1, active);
        v0         = std::get<0>(data);
        integral0  = std::get<1>(data);
        auto data1 = lookup(pos, cond_, index2, dim + 1, active);
        v1         = std::get<0>(data1);
        integral1  = std::get<1>(data1);

        if (!m_integral.empty() && dim == (m_range_cond.size() - 1)) {
            integral0 = dr::gather<Value>(m_integral, index1, active);
            integral1 = dr::gather<Value>(m_integral, index2, active);
        }

        return { dr::fmadd(v1, w1, dr::fmadd(v0, -w1, v0)),
                 dr::fmadd(integral1, w1,
                           dr::fmadd(integral0, -w1, integral0)) };
    }

    void lookup_fill(std::vector<Value> &cond_, Index index_, Value weight_,
                     std::vector<Index> &index_res_array_,
                     std::vector<Value> &weight_res_array_,
                     ScalarUInt32 res_index_, ScalarUInt32 dim,
                     Mask active) const {

        if (dim == m_size_cond.size()) {
            index_res_array_[res_index_]  = index_;
            weight_res_array_[res_index_] = weight_;
            return;
        }

        Value cond           = cond_[dim];
        size_t size_nodes    = m_size_cond[dim];
        Vector2f range_nodes = m_range_cond[dim];
        Float inv_interval   = m_inv_interval_cond[dim];

        Value new_x = (cond - range_nodes.x()) * inv_interval;
        Index bin_index =
            dr::clip(Index(dr::floor(new_x)), 0u, uint32_t(size_nodes - 2));

        Index index1;
        Index index2;
        if (dim == 0) {
            index1 = bin_index;
            index2 = bin_index + 1;
        } else {
            index1 = index_ * size_nodes + bin_index;
            index2 = index_ * size_nodes + bin_index + 1;
        }

        Value w1 = new_x - Value(bin_index);
        Value w0 = 1.f - w1;

        lookup_fill(cond_, index1, w0 * weight_, index_res_array_,
                    weight_res_array_, 2 * res_index_, dim + 1, active);
        lookup_fill(cond_, index2, w1 * weight_, index_res_array_,
                    weight_res_array_, 2 * res_index_ + 1, dim + 1, active);
    }

    std::pair<Value, Value> lookup_sample(Value u,
                                          std::vector<Index> &index_res_array_,
                                          std::vector<Value> &weight_res_array_,
                                          Mask active) const {
        // Compute the value of the CDF which we are looking for
        Value cond = 0.0;
        for (ScalarUInt32 i = 0; i < index_res_array_.size(); i++) {
            cond = cond +
                   u * weight_res_array_[i] *
                       dr::gather<Value>(m_integral, index_res_array_[i],
                                         active & (weight_res_array_[i] > 0.f));
        }

        // This is the length of the CDF, which has one element less than the
        // nodes
        size_t length_cdf = m_size_nodes - 1;

        // On the fly generate interpolated CDF and search for its entry
        Index bin_index = dr::binary_search<Index>(
            0, (uint32_t) length_cdf, [&](Index index) DRJIT_INLINE_LAMBDA {
                Value val = 0.;
                for (ScalarUInt32 i = 0; i < index_res_array_.size(); i++) {
                    val =
                        val +
                        weight_res_array_[i] *
                            dr::gather<Value>(
                                m_cdf,
                                index + Index(index_res_array_[i] * length_cdf),
                                active & (weight_res_array_[i] > 0.f));
                }
                return val < cond;
            });

        Value y0 = 0.f;
        Value y1 = 0.f;
        Value c0 = 0.f;

        for (ScalarUInt32 i = 0; i < index_res_array_.size(); i++) {
            Index index_tmp = index_res_array_[i] * m_size_nodes + bin_index;
            y0              = y0 +
                 weight_res_array_[i] *
                     dr::gather<Value>(m_pdf.array(), index_tmp,
                                       active & (weight_res_array_[i] > 0.f));
            y1 = y1 +
                 weight_res_array_[i] *
                     dr::gather<Value>(m_pdf.array(), index_tmp + 1,
                                       active & (weight_res_array_[i] > 0.f));

            Index c0_index = index_res_array_[i] * length_cdf + bin_index;
            c0             = c0 + weight_res_array_[i] *
                          dr::gather<Value>(m_cdf, c0_index - 1,
                                            active & (bin_index > 0) &
                                                (weight_res_array_[i] > 0.f));
        }

        Value sample = (cond - c0) * m_inv_interval;

        Value t_linear =
                  (y0 -
                   dr::safe_sqrt(dr::fmadd(y0, y0, 2.f * sample * (y1 - y0)))) *
                  dr::rcp(y0 - y1),
              t_const = sample / y0,
              t       = dr::select(y0 == y1, t_const, t_linear);

        Value integral = 0.0;
        for (ScalarUInt32 i = 0; i < index_res_array_.size(); i++) {
            integral +=
                weight_res_array_[i] *
                dr::gather<Value>(m_integral, index_res_array_[i],
                                  active & (weight_res_array_[i] > 0.f));
        }

        return { dr::fmadd(Value(bin_index) + t, m_interval, m_range.x()),
                 dr::fmadd(t, y1 - y0, y0) * dr::rcp(integral) };
    }

    void compute_cdf() {
        if (m_pdf.array().size() < 2)
            Throw("ConditionalRegular1D: needs at least two entries!");
#ifndef NDEBUG
        if (!dr::all(m_pdf.array() >= 0.f))
            Throw("ConditionalRegular1D: entries must be "
                  "non-negative!");
        if (!dr::any(m_pdf.array() > 0.f))
            Throw("ConditionalRegular1D: no probability mass found!");
#endif

        const size_t size_nodes = m_size_nodes;
        const size_t size_pdf   = dr::width(m_pdf.array());
        size_t size_cond        = 1;
        for (size_t i = 0; i < m_size_cond.size(); i++)
            size_cond *= m_size_cond[i];

        if (size_pdf != size_nodes * size_cond)
            Log(Error,
                "ConditionalRegular1D: %f (size_pdf) != %f (size_nodes) * %f "
                "(size_cond)",
                size_pdf, size_nodes, size_cond);

        m_max = dr::slice(dr::max(m_pdf.array()));

        const size_t size = m_size_nodes - 1;
        UInt32 index_curr = dr::arange<UInt32>(size);
        UInt32 index_next = dr::arange<UInt32>(1, size + 1);
        index_curr        = dr::tile(index_curr, size_cond);
        index_next        = dr::tile(index_next, size_cond);

        UInt32 offset = dr::repeat(dr::arange<UInt32>(size_cond) * m_size_nodes,
                                   m_size_nodes - 1);

        Float pdf_curr =
            dr::gather<Float>(m_pdf.array(), index_curr + offset, true);
        Float pdf_next =
            dr::gather<Float>(m_pdf.array(), index_next + offset, true);
        Float interval_integral =
            0.5f * m_interval_scalar * (pdf_curr + pdf_next);

        m_cdf =
            dr::block_prefix_sum(interval_integral, m_size_nodes - 1, false);

        UInt32 indexes_integral =
            dr::arange<UInt32>(size_cond) * (m_size_nodes - 1) + m_size_nodes -
            2;
        m_integral = dr::gather<Float>(m_cdf, indexes_integral, true);

        dr::eval(m_cdf, m_integral);
    }

    void compute_cdf_scalar(const ScalarFloat *pdf, size_t size_pdf) {
        if (unlikely(empty()))
            return;

        size_t size_cond = 1;
        for (size_t i = 0; i < m_size_cond.size(); i++)
            size_cond *= m_size_cond[i];

        size_t size_cdf = (m_size_nodes - 1) * size_cond;
        std::vector<ScalarFloat> cdf(size_cdf, 0.f);
        std::vector<ScalarFloat> integral(size_cond, 0.f);

        if (size_pdf != m_size_nodes * size_cond)
            Log(Error,
                "ConditionalRegular1D: size_pdf != size_nodes * size_cond");

        ScalarFloat integral_val = 0.0;
        ScalarFloat range = ScalarFloat(m_range.y()) - ScalarFloat(m_range.x()),
                    interval_size = range / (m_size_nodes - 1);

        m_max = pdf[0];
        for (size_t i = 0; i < size_cond; i++) {
            integral_val         = 0.0;
            ScalarVector2u valid = (uint32_t) -1;
            for (size_t j = 0; j < m_size_nodes - 1; j++) {
                ScalarFloat y0 = pdf[i * m_size_nodes + j];
                ScalarFloat y1 = pdf[i * m_size_nodes + j + 1];

                if (y0 < 0.f || y1 < 0.f)
                    Log(Error, "ConditionalRegular1D: Entries of the "
                               "conditioned PDFs must be non-negative!");

                m_max = dr::maximum(m_max, (ScalarFloat) y1);

                ScalarFloat value = 0.5f * interval_size * (y0 + y1);

                if (value > 0.f) {
                    // Determine the first and last wavelength bin with nonzero
                    // density
                    if (valid.x() == (uint32_t) -1)
                        valid.x() = (uint32_t) j;
                    valid.y() = (uint32_t) j;
                }

                integral_val += value;

                cdf[i * (m_size_nodes - 1) + j] = integral_val;
            }

            if (dr::any(valid == (uint32_t) -1))
                Log(Error, "ConditionalRegular1D: No probability mass found "
                           "for one conditioned PDF");

            integral[i] = integral_val;
        }

        m_cdf      = dr::load<FloatStorage>(cdf.data(), cdf.size());
        m_integral = dr::load<FloatStorage>(integral.data(), integral.size());
    }

private:
    ScalarVector2f m_range;
    ScalarUInt32 m_size_nodes;
    Float m_interval;
    ScalarFloat m_interval_scalar;
    Float m_inv_interval;
    TensorXf m_pdf;
    std::vector<ScalarUInt32> m_size_cond;
    std::vector<ScalarVector2f> m_range_cond;
    std::vector<Float> m_inv_interval_cond;
    FloatStorage m_cdf;
    FloatStorage m_integral;
    ScalarFloat m_max = 0.f;

    MI_TRAVERSE_CB(drjit::TraversableBase, m_interval, m_inv_interval, m_pdf,
                   m_inv_interval_cond, m_cdf, m_integral)
};

template <typename Value>
std::ostream &operator<<(std::ostream &os,
                         const ConditionalRegular1D<Value> & /*distr*/) {
    os << "ConditionalRegular1D[" << std::endl << std::endl << "]";
    return os;
}

NAMESPACE_END(mitsuba)
