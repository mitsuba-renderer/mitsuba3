#pragma once

#include <mitsuba/core/warp.h>
#include <mitsuba/core/util.h>

NAMESPACE_BEGIN(mitsuba)

/** =======================================================================
 * @{ \name Data-driven warping techniques for two dimensions
 *
 * This file provides three different approaches for importance sampling 2D
 * functions discretized on a regular grid. All functionality is written in a
 * generic fashion and works in scalar mode, packet mode, and the just-in-time
 * compiler (in particular, the complete sampling procedure is designed to be
 * JIT-compiled to a single CUDA or LLVM kernel without any intermediate
 * synchronization steps.)
 *
 * The first class \c DiscreteDistribution2D generates samples proportional to
 * a <em>discrete</em> 2D function sampled on a regular grid by sampling the
 * marginal distribution to choose a row, then a conditional distribution to
 * choose a column. This is a very simple ingredient that can be used to build
 * more advanced kinds of sampling schemes.
 *
 * The other two classes \c Hierarchical2D and \c Marginal2D are significantly
 * more complex and target sampling of <em>linear interpolants</em>, which
 * means that the sampling procedure is a function with floating point inputs
 * and outputs. The mapping is bijective and can be evaluated in <em>both
 * directions</em>. The implementations also supports <em>conditional
 * distributions</em>, i.e., 2D distributions that depend on an arbitrary
 * number of parameters (indicated via the \c Dimension template parameter). In
 * this case, a higher-dimensional discretization must be provided that will
 * also be linearly interpolated in these extra dimensions.
 *
 * Both approaches will produce exactly the same probability density, but the
 * mapping from random numbers to samples tends to be very different, which can
 * play an important role in certain applications. In particular:
 *
 * \c Hierarchical2D generates samples using hierarchical sample warping, which
 * is essentially a course-to-fine traversal of a MIP map. It generates a
 * mapping with very little shear/distortion, but it has numerous
 * discontinuities that can be problematic for some applications.
 *
 * \c Marginal2D is similar to \c DiscreteDistribution2D, in that it samples the
 * marginal, then the conditional. In contrast to \c DiscreteDistribution2D,
 * the mapping provides fractional outputs. In contrast to \c Hierarchical2D,
 * the mapping is guaranteed to not contain any discontinuities but tends to
 * have significant shear/distortion when the distribution contains isolated
 * regions with very high probability densities.
 *
 * There are actually two variants of \c Marginal2D: when <tt>Continuous=false</tt>,
 * discrete marginal/conditional distributions are used to select a bilinear
 * bilinear patch, followed by a continuous sampling step that chooses a
 * specific position inside the patch. When <tt>Continuous=true</tt>,
 * continuous marginal/conditional distributions are used instead, and the
 * second step is no longer needed. The latter scheme requires more computation
 * and memory accesses but produces an overall smoother mapping. The continuous
 * version of \c Marginal2D may be beneficial when this method is not used as a
 * sampling scheme, but rather to generate very high-quality parameterizations.
 *
 * =======================================================================
 */

template <typename Float_, size_t Dimension_ = 0>
class DiscreteDistribution2D {
public:
    using Float                       = Float_;
    using UInt32                      = uint32_array_t<Float>;
    using Mask                        = mask_t<Float>;
    using Point2f                     = Point<Float, 2>;
    using Point2u                     = Point<UInt32, 2>;
    using ScalarFloat                 = scalar_t<Float>;
    using ScalarVector2u              = Vector<uint32_t, 2>;
    using FloatStorage                = DynamicBuffer<Float>;

    DiscreteDistribution2D() = default;

    /**
     * Construct a marginal sample warping scheme for floating point
     * data of resolution \c size.
     */
    DiscreteDistribution2D(const ScalarFloat *data,
                           const ScalarVector2u &size)
        : m_size(size) {

        m_cond_cdf = empty<FloatStorage>(hprod(m_size));
        m_cond_cdf.managed();
        m_marg_cdf = empty<FloatStorage>(m_size.y());
        m_marg_cdf.managed();

        ScalarFloat *cond_cdf = m_cond_cdf.data(),
                    *marg_cdf = m_marg_cdf.data();

        // Construct conditional and marginal CDFs
        double accum_marg = 0.0;
        for (uint32_t y = 0; y < m_size.y(); ++y) {
            double accum_cond = 0.0;
            uint32_t idx = m_size.x() * y;
            for (uint32_t x = 0; x < m_size.x(); ++x, ++idx) {
                accum_cond += (double) data[idx];
                cond_cdf[idx] = (ScalarFloat) accum_cond;
            }
            accum_marg += accum_cond;
            marg_cdf[y] = accum_marg;
        }

        m_inv_normalization = (ScalarFloat) accum_marg;
        m_normalization = (ScalarFloat) (1.0 / accum_marg);
    }

    /// Evaluate the function value at the given integer position
    Float eval(const Point2u &pos, Mask active = true) const {
        UInt32 index = pos.x() + pos.y() * m_size.x();

        return gather<Float>(m_cond_cdf, index, active) -
               gather<Float>(m_cond_cdf, index - 1, active && pos.x() > 0);
    }

    /// Evaluate the normalized function value at the given integer position
    Float pdf(const Point2u &pos, Mask active = true) const {
        return eval(pos, active) * m_normalization;
    }

    /**
     * \brief Given a uniformly distributed 2D sample, draw a sample from the
     * distribution
     *
     * Returns the integer position, the normalized probability value, and
     * re-uniformized random variate that can be used for further sampling
     * steps.
     */
    std::tuple<Point2u, Float, Point2f> sample(const Point2f &sample_,
                                               Mask active = true) const {
        MTS_MASK_ARGUMENT(active);
        Point2f sample(sample_);

        // Avoid degeneracies on the domain boundary
        sample = clamp(sample, std::numeric_limits<ScalarFloat>::min(),
                       math::OneMinusEpsilon<Float>);

        // Scale sample Y range
        sample.y() *= m_inv_normalization;

        // Sample the row from the marginal distribution
        UInt32 row = enoki::binary_search(
            0u, m_size.y() - 1, [&](UInt32 idx) ENOKI_INLINE_LAMBDA {
                return gather<Float>(m_marg_cdf, idx, active) < sample.y();
            });

        UInt32 offset = row * m_size.x();

        // Scale sample X range
        sample.x() *= gather<Float>(m_cond_cdf, offset + m_size.x() - 1, active);

        // Sample the column from the conditional distribution
        UInt32 col = enoki::binary_search(
            0u, m_size.x() - 1, [&](UInt32 idx) ENOKI_INLINE_LAMBDA {
                return gather<Float>(m_cond_cdf, idx + offset, active) < sample.x();
            });

        // Re-scale uniform variate
        Float col_cdf_0 = gather<Float>(m_cond_cdf, offset + col - 1, active && col > 0),
              col_cdf_1 = gather<Float>(m_cond_cdf, offset + col, active),
              row_cdf_0 = gather<Float>(m_marg_cdf, row - 1, active && row > 0),
              row_cdf_1 = gather<Float>(m_marg_cdf, row, active);

        sample.x() -= col_cdf_0;
        sample.y() -= row_cdf_0;
        masked(sample.x(), neq(col_cdf_1, col_cdf_0)) /= col_cdf_1 - col_cdf_0;
        masked(sample.y(), neq(row_cdf_1, row_cdf_0)) /= row_cdf_1 - row_cdf_0;

        return { Point2u(col, row), (col_cdf_1 - col_cdf_0) * m_normalization, sample };
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "DiscreteDistribution2D" << "[" << std::endl
            << "  size = " << m_size << "," << std::endl
            << "  normalization = " << m_normalization << std::endl
            << "]";
        return oss.str();
    }

protected:
    /// Resolution of the discretized density function
    ScalarVector2u m_size;

    /// Density values
    FloatStorage m_data;

    /// Marginal and conditional PDFs
    FloatStorage m_marg_cdf;
    FloatStorage m_cond_cdf;

    ScalarFloat m_inv_normalization;
    ScalarFloat m_normalization;
};

/// Base class of Hierarchical2D and Marginal2D with common functionality
template <typename Float_, size_t Dimension_ = 0> class Distribution2D {
public:
    static constexpr size_t Dimension = Dimension_;
    using Float                       = Float_;
    using UInt32                      = uint32_array_t<Float>;
    using Int32                       = int32_array_t<Float>;
    using Mask                        = mask_t<Float>;
    using Point2f                     = Point<Float, 2>;
    using Point2i                     = Point<Int32, 2>;
    using Point2u                     = Point<UInt32, 2>;
    using ScalarFloat                 = scalar_t<Float>;
    using ScalarVector2f              = Vector<ScalarFloat, 2>;
    using ScalarVector2u              = Vector<uint32_t, 2>;
    using FloatStorage                = DynamicBuffer<Float>;

protected:
    Distribution2D() = default;

    Distribution2D(const ScalarVector2u &size,
                   const std::array<uint32_t, Dimension> &param_res,
                   const std::array<const ScalarFloat *, Dimension> &param_values) {
        if (any(size < 2))
            Throw("Distribution2D(): input array resolution must be >= 2!");

        // The linear interpolant has 'size-1' patches
        ScalarVector2u n_patches = size - 1;

        m_patch_size = 1.f / n_patches;
        m_inv_patch_size = n_patches;

        // Dependence on additional parameters
        m_slices = 1;
        for (int i = (int) Dimension - 1; i >= 0; --i) {
            if (param_res[i] < 1)
                Throw("Distribution2D(): parameter resolution must be >= 1!");

            m_param_values[i] = FloatStorage::copy(param_values[i], param_res[i]);
            m_param_strides[i] = param_res[i] > 1 ? m_slices : 0;
            m_slices *= param_res[i];
        }
    }

    // Look up parameter-related indices and weights (if Dimension != 0)
    UInt32 interpolate_weights(const Float *param, Float *param_weight,
                               Mask active) const {
        ENOKI_MARK_USED(param);

        if constexpr (Dimension > 0) {
            MTS_MASK_ARGUMENT(active);

            UInt32 slice_offset = zero<UInt32>();
            for (size_t dim = 0; dim < Dimension; ++dim) {
                if (unlikely(m_param_values[dim].size() == 1)) {
                    param_weight[2 * dim] = 1.f;
                    param_weight[2 * dim + 1] = 0.f;
                    continue;
                }

                UInt32 param_index = math::find_interval(
                    (uint32_t) m_param_values[dim].size(),
                    [&](UInt32 idx) ENOKI_INLINE_LAMBDA {
                        return gather<Float>(m_param_values[dim], idx, active) <
                               param[dim];
                    });

                Float p0 = gather<Float>(m_param_values[dim], param_index, active),
                      p1 = gather<Float>(m_param_values[dim], param_index + 1, active);

                param_weight[2 * dim + 1] =
                    clamp((param[dim] - p0) / (p1 - p0), 0.f, 1.f);
                param_weight[2 * dim] = 1.f - param_weight[2 * dim + 1];
                slice_offset += m_param_strides[dim] * param_index;
            }

            return slice_offset;
        } else {
            ENOKI_MARK_USED(param);
            ENOKI_MARK_USED(param_weight);
            ENOKI_MARK_USED(active);
            return 0u;
        }
    }

protected:
#if !defined(_MSC_VER)
    static constexpr size_t DimensionInt = Dimension;
#else
    static constexpr size_t DimensionInt = (Dimension != 0) ? Dimension : 1;
#endif

    /// Size of a bilinear patch in the unit square
    ScalarVector2f m_patch_size;

    /// Inverse of the above
    ScalarVector2f m_inv_patch_size;

    /// Stride per parameter in units of sizeof(ScalarFloat)
    uint32_t m_param_strides[DimensionInt];

    /// Discretization of each parameter domain
    FloatStorage m_param_values[DimensionInt];

    /// Total number of slices (in case Dimension > 1)
    uint32_t m_slices;
};

/**
 * \brief Implements a hierarchical sample warping scheme for 2D distributions
 * with linear interpolation and an optional dependence on additional parameters
 *
 * This class takes a rectangular floating point array as input and constructs
 * internal data structures to efficiently map uniform variates from the unit
 * square <tt>[0, 1]^2</tt> to a function on <tt>[0, 1]^2</tt> that linearly
 * interpolates the input array.
 *
 * The mapping is constructed from a sequence of <tt>log2(hmax(res))</tt>
 * hierarchical sample warping steps, where <tt>res</tt> is the input array
 * resolution. It is bijective and generally very well-behaved (i.e. low
 * distortion), which makes it a good choice for structured point sets such
 * as the Halton or Sobol sequence.
 *
 * The implementation also supports <em>conditional distributions</em>, i.e. 2D
 * distributions that depend on an arbitrary number of parameters (indicated
 * via the \c Dimension template parameter).
 *
 * In this case, the input array should have dimensions <tt>N0 x N1 x ... x Nn
 * x res.y() x res.x()</tt> (where the last dimension is contiguous in memory),
 * and the <tt>param_res</tt> should be set to <tt>{ N0, N1, ..., Nn }</tt>,
 * and <tt>param_values</tt> should contain the parameter values where the
 * distribution is discretized. Linear interpolation is used when sampling or
 * evaluating the distribution for in-between parameter values.
 *
 * \remark The Python API exposes explicitly instantiated versions of this
 * class named Hierarchical2D0, Hierarchical2D1, and Hierarchical2D2 for data
 * that depends on 0, 1, and 2 parameters, respectively.
 */
template <typename Float_, size_t Dimension_ = 0>
class Hierarchical2D : public Distribution2D<Float_, Dimension_> {
public:
    using Base = Distribution2D<Float_, Dimension_>;

    ENOKI_USING_TYPES(Base,
        Float, UInt32, Mask, ScalarFloat, Point2i, Point2f,
        Point2u, ScalarVector2f, ScalarVector2u, FloatStorage
    )

    ENOKI_USING_MEMBERS(Base,
        Dimension, DimensionInt, m_patch_size, m_inv_patch_size,
        m_param_strides, m_param_values, m_slices,
        interpolate_weights
    )

    Hierarchical2D() = default;

    /**
     * Construct a hierarchical sample warping scheme for floating point
     * data of resolution \c size.
     *
     * \c param_res and \c param_values are only needed for conditional
     * distributions (see the text describing the Hierarchical2D class).
     *
     * If \c normalize is set to \c false, the implementation will not
     * re-scale the distribution so that it integrates to \c 1. It can
     * still be sampled (proportionally), but returned density values
     * will reflect the unnormalized values.
     *
     * If \c enable_sampling is set to \c false, the implementation will not
     * construct the hierarchy needed for sample warping, which saves memory
     * in case this functionality is not needed (e.g. if only the interpolation
     * in ``eval()`` is used). In this case, ``sample()`` and ``invert()``
     * can still be called without triggering undefined behavior, but they
     * will not return meaningful results.
     */
    Hierarchical2D(const ScalarFloat *data,
                   const ScalarVector2u &size,
                   const std::array<uint32_t, Dimension> &param_res = { },
                   const std::array<const ScalarFloat *, Dimension> &param_values = { },
                   bool normalize = true,
                   bool enable_sampling = true)
        : Base(size, param_res, param_values) {

        // The linear interpolant has 'size-1' patches
        ScalarVector2u n_patches = size - 1;

        // Keep track of the dependence on additional parameters (optional)
        uint32_t max_level = math::log2i_ceil(hmax(n_patches));

        m_max_patch_index = n_patches - 1;

        if (!enable_sampling) {
            m_levels.reserve(1);
            m_levels.emplace_back(size, m_slices);

            for (uint32_t slice = 0; slice < m_slices; ++slice) {
                uint32_t offset = m_levels[0].size * slice;

                ScalarFloat scale = 1.f;
                if (normalize) {
                    double sum = 0.0;
                    for (uint32_t i = 0; i < m_levels[0].size; ++i)
                        sum += (double) data[offset + i];
                    scale = hprod(n_patches) / (ScalarFloat) sum;
                }
                for (uint32_t i = 0; i < m_levels[0].size; ++i)
                    m_levels[0].data_ptr[offset + i] = data[offset + i] * scale;
            }

            return;
        }

        // Allocate memory for input array and MIP hierarchy
        m_levels.reserve(max_level + 2);
        m_levels.emplace_back(size, m_slices);

        ScalarVector2u level_size = n_patches;
        for (int level = max_level; level >= 0; --level) {
            level_size += level_size & 1u; // zero-pad
            m_levels.emplace_back(level_size, m_slices);
            level_size = sr<1>(level_size);
        }

        for (uint32_t slice = 0; slice < m_slices; ++slice) {
            uint32_t offset0 = m_levels[0].size * slice,
                     offset1 = m_levels[1].size * slice;

            // Integrate linear interpolant
            const ScalarFloat *in = data + offset0;

            double sum = 0.0;
            for (uint32_t y = 0; y < n_patches.y(); ++y) {
                for (uint32_t x = 0; x < n_patches.x(); ++x) {
                    ScalarFloat avg = (in[0] + in[1] + in[size.x()] +
                                 in[size.x() + 1]) * .25f;
                    sum += (double) avg;
                    *(m_levels[1].ptr(ScalarVector2u(x, y)) + offset1) = avg;
                    ++in;
                }
                ++in;
            }

            // Copy and normalize fine resolution interpolant
            ScalarFloat scale = normalize ? (ScalarFloat) (hprod(n_patches) / sum) : 1.f;
            for (uint32_t i = 0; i < m_levels[0].size; ++i)
                m_levels[0].data_ptr[offset0 + i] = data[offset0 + i] * scale;
            for (uint32_t i = 0; i < m_levels[1].size; ++i)
                m_levels[1].data_ptr[offset1 + i] *= scale;

            // Build a MIP hierarchy
            level_size = n_patches;
            for (uint32_t level = 2; level <= max_level + 1; ++level) {
                const Level &l0 = m_levels[level - 1];
                Level &l1 = m_levels[level];
                offset0 = l0.size * slice;
                offset1 = l1.size * slice;
                level_size = sr<1>(level_size + 1u);

                // Downsample
                for (uint32_t y = 0; y < level_size.y(); ++y) {
                    for (uint32_t x = 0; x < level_size.x(); ++x) {
                        ScalarFloat *d1 = l1.ptr(ScalarVector2u(x, y)) + offset1;
                        const ScalarFloat *d0 = l0.ptr(ScalarVector2u(x*2, y*2)) + offset0;
                        *d1 = d0[0] + d0[1] + d0[2] + d0[3];
                    }
                }
            }
        }
    }

    /**
     * \brief Given a uniformly distributed 2D sample, draw a sample from the
     * distribution (parameterized by \c param if applicable)
     *
     * Returns the warped sample and associated probability density.
     */
    std::pair<Point2f, Float> sample(Point2f sample,
                                     const Float *param = nullptr,
                                     Mask active = true) const {
        MTS_MASK_ARGUMENT(active);

        /// Find offset and interpolation weights wrt. conditional parameters
        Float param_weight[2 * DimensionInt];
        UInt32 slice_offset = interpolate_weights(param, param_weight, active);

        // Avoid issues with roundoff error
        sample = clamp(sample, 0.f, 1.f);

        // Hierarchical sample warping
        Point2u offset = zero<Point2u>();
        for (int l = (int) m_levels.size() - 2; l > 0; --l) {
            const Level &level = m_levels[l];

            offset = sl<1>(offset);

            // Fetch values from next MIP level
            UInt32 offset_i = level.index(offset) + slice_offset * level.size;

            Float v00 = level.lookup(offset_i, m_param_strides,
                                     param_weight, active);
            offset_i += 1u;

            Float v10 = level.lookup(offset_i, m_param_strides,
                                     param_weight, active);
            offset_i += 1u;

            Float v01 = level.lookup(offset_i, m_param_strides,
                                     param_weight, active);
            offset_i += 1u;

            Float v11 = level.lookup(offset_i, m_param_strides,
                                     param_weight, active);

            // Avoid issues with roundoff error
            sample = clamp(sample, 0.f, 1.f);

            // Select the row
            Float r0 = v00 + v10,
                  r1 = v01 + v11;
            sample.y() *= r0 + r1;
            Mask mask = sample.y() > r0;
            masked(offset.y(), mask) += 1u;
            masked(sample.y(), mask) -= r0;
            sample.y() /= select(mask, r1, r0);

            // Select the column
            Float c0 = select(mask, v01, v00),
                  c1 = select(mask, v11, v10);
            sample.x() *= c0 + c1;
            mask = sample.x() > c0;
            masked(sample.x(), mask) -= c0;
            sample.x() /= select(mask, c1, c0);
            masked(offset.x(), mask) += 1u;
        }

        const Level &level0 = m_levels[0];

        UInt32 offset_i =
            offset.x() + offset.y() * level0.width + slice_offset * level0.size;

        // Fetch corners of bilinear patch
        Float v00 = level0.lookup(offset_i, m_param_strides,
                                  param_weight, active);

        Float v10 = level0.lookup(offset_i + 1, m_param_strides,
                                  param_weight, active);

        Float v01 = level0.lookup(offset_i + level0.width, m_param_strides,
                                  param_weight, active);

        Float v11 = level0.lookup(offset_i + level0.width + 1, m_param_strides,
                                  param_weight, active);

        Float pdf;
        std::tie(sample, pdf) =
            warp::square_to_bilinear(v00, v10, v01, v11, sample);

        return {
            (Point2f(Point2i(offset)) + sample) * m_patch_size,
            pdf
        };
    }

    /// Inverse of the mapping implemented in ``sample()``
    std::pair<Point2f, Float> invert(Point2f sample,
                                     const Float *param = nullptr,
                                     Mask active = true) const {

        MTS_MASK_ARGUMENT(active);

        /// Find offset and interpolation weights wrt. conditional parameters
        Float param_weight[2 * DimensionInt];
        UInt32 slice_offset = interpolate_weights(param, param_weight, active);

        // Avoid issues with roundoff error
        sample = clamp(sample, 0.f, 1.f);

        // Fetch values at corners of bilinear patch
        const Level &level0 = m_levels[0];
        sample *= m_inv_patch_size;

        /// Point2f() -> Point2i() cast because AVX2 has no _mm256_cvtps_epu32 :(
        Point2u offset = min(Point2u(Point2i(sample)), m_max_patch_index);
        UInt32 offset_i =
            offset.x() + offset.y() * level0.width + slice_offset * level0.size;

        Float v00 = level0.lookup(offset_i, m_param_strides,
                                  param_weight, active);

        Float v10 = level0.lookup(offset_i + 1, m_param_strides,
                                  param_weight, active);

        Float v01 = level0.lookup(offset_i + level0.width, m_param_strides,
                                  param_weight, active);

        Float v11 = level0.lookup(offset_i + level0.width + 1, m_param_strides,
                                  param_weight, active);

        sample -= Point2f(Point2i(offset));

        Float pdf;
        std::tie(sample, pdf) = warp::bilinear_to_square(v00, v10, v01, v11, sample);

        // Hierarchical sample warping -- reverse direction
        for (int l = 1; l < (int) m_levels.size() - 1; ++l) {
            const Level &level = m_levels[l];

            // Fetch values from next MIP level
            offset_i = level.index(offset & ~1u) + slice_offset * level.size;

            v00 = level.lookup(offset_i, m_param_strides,
                               param_weight, active);
            offset_i += 1u;

            v10 = level.lookup(offset_i, m_param_strides,
                               param_weight, active);
            offset_i += 1u;

            v01 = level.lookup(offset_i, m_param_strides,
                               param_weight, active);
            offset_i += 1u;

            v11 = level.lookup(offset_i, m_param_strides,
                               param_weight, active);

            Mask x_mask = neq(offset.x() & 1u, 0u),
                 y_mask = neq(offset.y() & 1u, 0u);

            Float r0 = v00 + v10,
                  r1 = v01 + v11,
                  c0 = select(y_mask, v01, v00),
                  c1 = select(y_mask, v11, v10);

            sample.y() *= select(y_mask, r1, r0);
            masked(sample.y(), y_mask) += r0;
            sample.y() /= r0 + r1;

            sample.x() *= select(x_mask, c1, c0);
            masked(sample.x(), x_mask) += c0;
            sample.x() /= c0 + c1;

            // Avoid issues with roundoff error
            sample = clamp(sample, 0.f, 1.f);

            offset = sr<1>(offset);
        }

        return { sample, pdf };
    }

    /**
     * \brief Evaluate the density at position \c pos. The distribution is
     * parameterized by \c param if applicable.
     */
    Float eval(Point2f pos, const Float *param = nullptr,
               Mask active = true) const {
        /// Find offset and interpolation weights wrt. conditional parameters
        Float param_weight[2 * DimensionInt];
        UInt32 slice_offset = interpolate_weights(param, param_weight, active);

        // Avoid issues with roundoff error
        pos = clamp(pos, 0.f, 1.f);

        // Compute linear interpolation weights
        pos *= m_inv_patch_size;
        Point2u offset = min(Point2u(Point2i(pos)), m_max_patch_index);
        pos -= Point2f(Point2i(offset));

        const Level &level0 = m_levels[0];
        UInt32 offset_i =
            offset.x() + offset.y() * level0.width + slice_offset * level0.size;

        Float v00 = level0.lookup(offset_i, m_param_strides,
                                  param_weight, active);

        Float v10 = level0.lookup(offset_i + 1, m_param_strides,
                                  param_weight, active);

        Float v01 = level0.lookup(offset_i + level0.width, m_param_strides,
                                  param_weight, active);

        Float v11 = level0.lookup(offset_i + level0.width + 1, m_param_strides,
                                  param_weight, active);

        return warp::square_to_bilinear_pdf(v00, v10, v01, v11, pos);
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "Hierarchical2D" << Dimension << "[" << std::endl
            << "  size = [" << m_levels[0].width << ", "
            << m_levels[0].size / m_levels[0].width << "]," << std::endl
            << "  levels = " << m_levels.size() << "," << std::endl;
        size_t size = 0;
        if (Dimension > 0) {
            oss << "  param_size = [";
            for (size_t i = 0; i<Dimension; ++i) {
                if (i != 0)
                    oss << ", ";
                oss << m_param_values[i].size();
            }
            oss << "]," << std::endl
                << "  param_strides = [";
            for (size_t i = 0; i<Dimension; ++i) {
                if (i != 0)
                    oss << ", ";
                oss << m_param_strides[i];
            }
            oss << "]," << std::endl;
        }
        oss << "  storage = { " << m_slices << " slice" << (m_slices > 1 ? "s" : "")
            << ", ";
        for (size_t i = 0; i < m_levels.size(); ++i)
            size += m_levels[i].size * m_slices;
        oss << util::mem_string(size * sizeof(ScalarFloat)) << " }" << std::endl
            << "]";
        return oss.str();
    }

protected:
    struct Level {
        uint32_t size;
        uint32_t width;
        FloatStorage data;
        ScalarFloat *data_ptr;

        Level() { }
        Level(ScalarVector2u res, uint32_t slices)
            : size(hprod(res)), width(res.x()),
              data(zero<FloatStorage>(hprod(res) * slices)) {
            data.managed();
            data_ptr = data.data();
        }

        /**
         * \brief Convert from 2D pixel coordinates to an index indicating how the
         * data is laid out in memory.
         *
         * The implementation stores 2x2 patches contigously in memory to
         * improve cache locality during hierarchical traversals
         */
        template <typename Point2u>
        MTS_INLINE value_t<Point2u> index(const Point2u &p) const {
            return ((p.x() & 1u) | sl<1>((p.x() & ~1u) | (p.y() & 1u))) +
                   ((p.y() & ~1u) * width);
        }

        MTS_INLINE ScalarFloat *ptr(const ScalarVector2u &p) {
            return data_ptr + index(p);
        }

        MTS_INLINE const ScalarFloat *ptr(const ScalarVector2u &p) const {
            return data_ptr + index(p);
        }

        template <size_t Dim = Dimension>
        MTS_INLINE Float lookup(const UInt32 &i0,
                                const uint32_t *param_strides,
                                const Float *param_weight,
                                const Mask &active) const {
            if constexpr (Dim != 0) {
                UInt32 i1 = i0 + param_strides[Dim - 1] * size;

                Float w0 = param_weight[2 * Dim - 2],
                      w1 = param_weight[2 * Dim - 1],
                      v0 = lookup<Dim - 1>(i0, param_strides, param_weight, active),
                      v1 = lookup<Dim - 1>(i1, param_strides, param_weight, active);

                return fmadd(v0, w0, v1 * w1);
            } else {
                ENOKI_MARK_USED(param_strides);
                ENOKI_MARK_USED(param_weight);
                return gather<Float>(data, i0, active);
            }
        }
    };

    /// MIP hierarchy over linearly interpolated patches
    std::vector<Level> m_levels;

    /// Number of bilinear patches in the X/Y dimension - 1
    ScalarVector2u m_max_patch_index;
};

/**
 * \brief Implements a marginal sample warping scheme for 2D distributions
 * with linear interpolation and an optional dependence on additional parameters
 *
 * This class takes a rectangular floating point array as input and constructs
 * internal data structures to efficiently map uniform variates from the unit
 * square <tt>[0, 1]^2</tt> to a function on <tt>[0, 1]^2</tt> that linearly
 * interpolates the input array.
 *
 * The mapping is constructed via the inversion method, which is applied to
 * a marginal distribution over rows, followed by a conditional distribution
 * over columns.
 *
 * The implementation also supports <em>conditional distributions</em>, i.e. 2D
 * distributions that depend on an arbitrary number of parameters (indicated
 * via the \c Dimension template parameter).
 *
 * In this case, the input array should have dimensions <tt>N0 x N1 x ... x Nn
 * x res.y() x res.x()</tt> (where the last dimension is contiguous in memory),
 * and the <tt>param_res</tt> should be set to <tt>{ N0, N1, ..., Nn }</tt>,
 * and <tt>param_values</tt> should contain the parameter values where the
 * distribution is discretized. Linear interpolation is used when sampling or
 * evaluating the distribution for in-between parameter values.
 *
 * There are two variants of \c Marginal2D: when <tt>Continuous=false</tt>,
 * discrete marginal/conditional distributions are used to select a bilinear
 * bilinear patch, followed by a continuous sampling step that chooses a
 * specific position inside the patch. When <tt>Continuous=true</tt>,
 * continuous marginal/conditional distributions are used instead, and the
 * second step is no longer needed. The latter scheme requires more computation
 * and memory accesses but produces an overall smoother mapping.
 *
 * \remark The Python API exposes explicitly instantiated versions of this
 * class named \c MarginalDiscrete2D0 to \c MarginalDiscrete2D3 and
 * \c MarginalContinuous2D0 to \c MarginalContinuous2D3 for data that depends
 * on 0 to 3 parameters.
 */
template <typename Float_, size_t Dimension_ = 0, bool Continuous = false>
class Marginal2D : public Distribution2D<Float_, Dimension_> {
public:
    using Base = Distribution2D<Float_, Dimension_>;

    ENOKI_USING_TYPES(Base,
        Float, UInt32, Mask, ScalarFloat, Point2f, Point2i,
        Point2u, ScalarVector2f, ScalarVector2u, FloatStorage
    )

    ENOKI_USING_MEMBERS(Base,
        Dimension, DimensionInt, m_patch_size, m_inv_patch_size,
        m_param_strides, m_param_values, m_slices, interpolate_weights
    )

    Marginal2D() = default;

    /**
     * Construct a marginal sample warping scheme for floating point
     * data of resolution \c size.
     *
     * \c param_res and \c param_values are only needed for conditional
     * distributions (see the text describing the Marginal2D class).
     *
     * If \c normalize is set to \c false, the implementation will not
     * re-scale the distribution so that it integrates to \c 1. It can
     * still be sampled (proportionally), but returned density values
     * will reflect the unnormalized values.
     *
     * If \c enable_sampling is set to \c false, the implementation will not
     * construct the cdf needed for sample warping, which saves memory in case
     * this functionality is not needed (e.g. if only the interpolation in
     * ``eval()`` is used).
     */
    Marginal2D(const ScalarFloat *data,
               const ScalarVector2u &size,
               const std::array<uint32_t, Dimension> &param_res = { },
               const std::array<const ScalarFloat *, Dimension> &param_values = { },
               bool normalize = true, bool enable_sampling = true)
        : Base(size, param_res, param_values), m_size(size), m_normalized(normalize) {

        uint32_t w      = m_size.x(),
                 h      = m_size.y(),
                 n_data = w * h,
                 n_marg = h - 1,
                 n_cond = (w - 1) * (Continuous ? h : (h - 1));

        double scale_x = .5 / (w - 1),
               scale_y = .5 / (h - 1);

        m_data = empty<FloatStorage>(m_slices * n_data);
        m_data.managed();

        if (enable_sampling) {
            m_marg_cdf = empty<FloatStorage>(m_slices * n_marg);
            m_marg_cdf.managed();

            m_cond_cdf = empty<FloatStorage>(m_slices * n_cond);
            m_cond_cdf.managed();

            ScalarFloat *marg_cdf = m_marg_cdf.data(),
                        *cond_cdf = m_cond_cdf.data(),
                        *data_out = m_data.data();

            std::unique_ptr<double[]> cond_cdf_sum(new double[h]);

            for (uint32_t slice = 0; slice < m_slices; ++slice) {
                ScalarFloat norm = 1.f;

                /* The marginal/probability distribution computation
                   differs for the Continuous=false/true cases */
                if constexpr (Continuous) {
                    // Construct conditional CDF
                    for (uint32_t y = 0; y < h; ++y) {
                        double accum = 0.0;
                        uint32_t i = y * w, j = y * (w - 1);
                        for (uint32_t x = 0; x < w - 1; ++x, ++i, ++j) {
                            accum += scale_x * ((double) data[i] +
                                                (double) data[i + 1]);
                            cond_cdf[j] = (ScalarFloat) accum;
                        }
                        cond_cdf_sum[y] = accum;
                    }

                    // Construct marginal CDF
                    double accum = 0.0;
                    for (uint32_t y = 0; y < h - 1; ++y) {
                        accum += scale_y * (cond_cdf_sum[y] + cond_cdf_sum[y + 1]);
                        marg_cdf[y] = (ScalarFloat) accum;
                    }

                    if (normalize)
                        norm = ScalarFloat(1.0 / accum);
                } else {
                    double scale = scale_x * scale_y;

                    // Construct conditional CDF
                    for (uint32_t y = 0; y < h - 1; ++y) {
                        double accum = 0.0;
                        uint32_t i = y * w, j = y * (w - 1);
                        for (uint32_t x = 0; x < w - 1; ++x, ++i, ++j) {
                            accum += scale * ((double) data[i] +
                                              (double) data[i + 1] +
                                              (double) data[i + w] +
                                              (double) data[i + w + 1]);
                            cond_cdf[j] = (ScalarFloat) accum;
                        }
                        cond_cdf_sum[y] = accum;
                    }

                    // Construct marginal CDF
                    double accum = 0.0;
                    for (uint32_t y = 0; y < h - 1; ++y) {
                        accum += cond_cdf_sum[y];
                        marg_cdf[y] = (ScalarFloat) accum;
                    }

                    if (normalize)
                        norm = ScalarFloat(1.0 / accum);
                }

                for (size_t i = 0; i < n_cond; ++i)
                    *cond_cdf++ *= norm;
                for (size_t i = 0; i < n_marg; ++i)
                    *marg_cdf++ *= norm;
                for (size_t i = 0; i < n_data; ++i)
                    *data_out++ = *data++ * norm;
            }
        } else {
            ScalarFloat *data_out = m_data.data();

            for (uint32_t slice = 0; slice < m_slices; ++slice) {
                ScalarFloat norm = 1.f;

                if (normalize) {
                    double sum = 0.0;
                    for (uint32_t y = 0; y < h - 1; ++y) {
                        size_t i = y * w;
                        for (uint32_t x = 0; x < w - 1; ++x, ++i) {
                            sum += (double) data[i] +
                                   (double) data[i + 1] +
                                   (double) data[i + w] +
                                   (double) data[i + w + 1];
                        }
                    }
                    norm = ScalarFloat(1.0 / (scale_x * scale_y * sum));
                }

                for (uint32_t k = 0; k < n_data; ++k)
                    *data_out++ = *data++ * norm;
            }
        }
    }

    /**
     * \brief Given a uniformly distributed 2D sample, draw a sample from the
     * distribution (parameterized by \c param if applicable)
     *
     * Returns the warped sample and associated probability density.
     */
    std::pair<Point2f, Float> sample(const Point2f &sample,
                                     const Float *param = nullptr,
                                     Mask active = true) const {
        Assert(!m_marg_cdf.empty(), "Marginal2D::sample(): enable_sampling=false!");

        if constexpr (Continuous)
            return sample_continuous(sample, param, active);
        else
            return sample_discrete(sample, param, active);
    }

    /// Inverse of the mapping implemented in ``sample()``
    std::pair<Point2f, Float> invert(const Point2f &sample,
                                     const Float *param = nullptr,
                                     Mask active = true) const {
        Assert(!m_marg_cdf.empty(), "Marginal2D::invert(): enable_sampling=false!");

        if constexpr (Continuous)
            return invert_continuous(sample, param, active);
        else
            return invert_discrete(sample, param, active);
    }

    /**
     * \brief Evaluate the density at position \c pos. The distribution is
     * parameterized by \c param if applicable.
     */
    Float eval(Point2f pos, const Float *param = nullptr,
               Mask active = true) const {
        MTS_MASK_ARGUMENT(active);

        Float param_weight[2 * DimensionInt];
        UInt32 slice_offset = interpolate_weights(param, param_weight, active);

        // Avoid issues with roundoff error
        pos = clamp(pos, 0.f, 1.f);

        // Compute linear interpolation weights
        pos *= m_inv_patch_size;
        Point2u offset = min(Point2u(Point2i(pos)), m_size - 2u);
        pos -= Point2f(Point2i(offset));

        UInt32 index = offset.x() + offset.y() * m_size.x();

        uint32_t size = hprod(m_size);
        if (Dimension != 0)
            index += slice_offset * size;

        Float v00 = lookup(m_data.data(), index,
                           size, param_weight, active),
              v10 = lookup(m_data.data() + 1, index,
                           size, param_weight, active),
              v01 = lookup(m_data.data() + m_size.x(), index,
                           size, param_weight, active),
              v11 = lookup(m_data.data() + m_size.x() + 1, index,
                           size, param_weight, active);

        return warp::square_to_bilinear_pdf(v00, v10, v01, v11, pos);
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "Marginal2D" << Dimension << "[" << std::endl
            << "  size = " << m_size << "," << std::endl;
        if (Dimension > 0) {
            oss << "  param_size = [";
            for (size_t i = 0; i<Dimension; ++i) {
                if (i != 0)
                    oss << ", ";
                oss << m_param_values[i].size();
            }
            oss << "]," << std::endl
                << "  param_strides = [";
            for (size_t i = 0; i<Dimension; ++i) {
                if (i != 0)
                    oss << ", ";
                oss << m_param_strides[i];
            }
            oss << "]," << std::endl;
        }
        oss << "  storage = { " << m_slices << " slice" << (m_slices > 1 ? "s" : "")
            << ", ";
        size_t size = m_slices * (hprod(m_size) * 2 + m_size.y());
        oss << util::mem_string(size * sizeof(ScalarFloat)) << " }" << std::endl
            << "]";
        return oss.str();
    }

protected:
    template <size_t Dim = Dimension>
    MTS_INLINE Float lookup(const ScalarFloat *data,
                            UInt32 i0,
                            uint32_t size,
                            const Float *param_weight,
                            Mask active) const {
        if constexpr (Dim != 0) {
            UInt32 i1 = i0 + m_param_strides[Dim - 1] * size;

            Float w0 = param_weight[2 * Dim - 2],
                  w1 = param_weight[2 * Dim - 1],
                  v0 = lookup<Dim - 1>(data, i0, size, param_weight, active),
                  v1 = lookup<Dim - 1>(data, i1, size, param_weight, active);

            return fmadd(v0, w0, v1 * w1);
        } else {
            ENOKI_MARK_USED(param_weight);
            ENOKI_MARK_USED(size);
            return gather<Float>(data, i0, active);
        }
    }

    MTS_INLINE
    std::pair<Point2f, Float> sample_discrete(Point2f sample,
                                              const Float *param,
                                              Mask active) const {
        MTS_MASK_ARGUMENT(active);

        // Size of a slice of various tables (conditional/marginal/data)
        uint32_t n_cond = hprod(m_size - 1),
                 n_marg = m_size.y() - 1,
                 n_data = hprod(m_size);

        /// Find offset and interpolation weights wrt. conditional parameters
        Float param_weight[2 * DimensionInt];
        UInt32 slice_offset = interpolate_weights(param, param_weight, active);

        // Avoid degeneracies on the domain boundary
        sample = clamp(sample, math::Epsilon<Float>, math::OneMinusEpsilon<Float>);

        /// Multiply by last entry of marginal CDF if the data is not normalized
        UInt32 offset_marg = slice_offset * n_marg;

        auto fetch_marginal = [&](UInt32 idx, Mask mask)
                                  ENOKI_INLINE_LAMBDA {
            return lookup(m_marg_cdf.data(), offset_marg + idx,
                          n_marg, param_weight, mask);
        };

        if (!m_normalized)
            sample.y() *= fetch_marginal(n_marg - 1, active);

        // Sample the row from the marginal distribution
        UInt32 row = enoki::binary_search(
            0u, n_marg - 1, [&](UInt32 idx) ENOKI_INLINE_LAMBDA {
                return fetch_marginal(idx, active) < sample.y();
            });

        // Re-scale uniform variate
        Float row_cdf_0 = fetch_marginal(row - 1, active && row > 0),
              row_cdf_1 = fetch_marginal(row, active);

        sample.y() -= row_cdf_0;
        masked(sample.y(), neq(row_cdf_1, row_cdf_0)) /= row_cdf_1 - row_cdf_0;

        /// Multiply by last entry of conditional CDF
        UInt32 offset_cond = slice_offset * n_cond + row * (m_size.x() - 1);
        sample.x() *= lookup(m_cond_cdf.data(),
                             offset_cond + m_size.x() - 2,
                             n_cond, param_weight, active);

        // Sample the column from the conditional distribution
        UInt32 col = enoki::binary_search(
            0u, m_size.x() - 2, [&](UInt32 idx) ENOKI_INLINE_LAMBDA {
                return lookup(m_cond_cdf.data(),
                              offset_cond + idx, n_cond,
                              param_weight, active) < sample.x();
            });

        // Re-scale uniform variate
        Float col_cdf_0 = lookup(m_cond_cdf.data(), offset_cond + col - 1,
                                 n_cond, param_weight, active && col > 0),
              col_cdf_1 = lookup(m_cond_cdf.data(), offset_cond + col,
                                 n_cond, param_weight, active);

        sample.x() -= col_cdf_0;
        masked(sample.x(), neq(col_cdf_1, col_cdf_0)) /= col_cdf_1 - col_cdf_0;

        // Sample a position on the bilinear patch
        UInt32 offset_data = slice_offset * n_data + row * m_size.x() + col;
        Float v00 = lookup(m_data.data(), offset_data, n_data,
                           param_weight, active),
              v10 = lookup(m_data.data() + 1, offset_data, n_data,
                           param_weight, active),
              v01 = lookup(m_data.data() + m_size.x(), offset_data,
                           n_data, param_weight, active),
              v11 = lookup(m_data.data() + m_size.x() + 1, offset_data,
                           n_data, param_weight, active);

        Float pdf;
        std::tie(sample, pdf) =
            warp::square_to_bilinear(v00, v10, v01, v11, sample);

        return { (Point2i(Point2u(col, row)) + sample) * m_patch_size, pdf };
    }

    MTS_INLINE
    std::pair<Point2f, Float> invert_discrete(Point2f sample,
                                              const Float *param,
                                              Mask active) const {
        MTS_MASK_ARGUMENT(active);

        // Size of a slice of various tables (conditional/marginal/data)
        uint32_t n_cond = hprod(m_size - 1),
                 n_marg = m_size.y() - 1,
                 n_data = hprod(m_size);

        /// Find offset and interpolation weights wrt. conditional parameters
        Float param_weight[2 * DimensionInt];
        UInt32 slice_offset = interpolate_weights(param, param_weight, active);

        // Avoid issues with roundoff error
        sample = clamp(sample, 0.f, 1.f);

        // Fetch values at corners of bilinear patch
        sample *= m_inv_patch_size;
        Point2u offset = min(Point2u(Point2i(sample)), m_size - 2u);
        UInt32 index = offset.x() + offset.y() * m_size.x() + slice_offset * n_data;
        sample -= Point2f(Point2i(offset));

        Float v00 = lookup(m_data.data(), index,
                           n_data, param_weight, active),
              v10 = lookup(m_data.data() + 1, index,
                           n_data, param_weight, active),
              v01 = lookup(m_data.data() + m_size.x(), index,
                           n_data, param_weight, active),
              v11 = lookup(m_data.data() + m_size.x() + 1, index,
                           n_data, param_weight, active);

        Float pdf;
        std::tie(sample, pdf) = warp::bilinear_to_square(v00, v10, v01, v11, sample);

        UInt32 offset_cond = slice_offset * n_cond + offset.y() * (m_size.x() - 1),
               offset_marg = slice_offset * n_marg;

        Float row_cdf_0 = lookup(m_marg_cdf.data(), offset_marg + offset.y() - 1,
                                 n_marg, param_weight, active && offset.y() > 0),
              row_cdf_1 = lookup(m_marg_cdf.data(), offset_marg + offset.y(),
                                 n_marg, param_weight, active),
              col_cdf_0 = lookup(m_cond_cdf.data(), offset_cond + offset.x() - 1,
                                 n_cond, param_weight, active && offset.x() > 0),
              col_cdf_1 = lookup(m_cond_cdf.data(), offset_cond + offset.x(),
                                 n_cond, param_weight, active);

        sample.x() = lerp(col_cdf_0, col_cdf_1, sample.x());
        sample.y() = lerp(row_cdf_0, row_cdf_1, sample.y());

        sample.x() /= lookup(m_cond_cdf.data(),
                             offset_cond + m_size.x() - 2,
                             n_cond, param_weight, active);

        if (!m_normalized)
            sample.y() /= lookup(m_marg_cdf.data(), offset_marg + n_marg - 1,
                                 n_marg, param_weight, active);

        return { sample, pdf };
    }

    MTS_INLINE
    std::pair<Point2f, Float> sample_continuous(Point2f sample,
                                                const Float *param,
                                                Mask active) const {
        MTS_MASK_ARGUMENT(active);

        // Size of a slice of various tables (conditional/marginal/data)
        uint32_t n_cond = m_size.y() * (m_size.x() - 1),
                 n_marg = m_size.y() - 1,
                 n_data = hprod(m_size);

        /// Find offset and interpolation weights wrt. conditional parameters
        Float param_weight[2 * DimensionInt];
        UInt32 slice_offset = interpolate_weights(param, param_weight, active);
        ENOKI_MARK_USED(slice_offset);

        // Avoid degeneracies on the domain boundary
        sample = clamp(sample, math::Epsilon<Float>,
                       math::OneMinusEpsilon<Float>);

        // Sample the row first
        UInt32 offset_marg = slice_offset * n_marg;

        auto fetch_marginal = [&](UInt32 idx, Mask mask)
                                  ENOKI_INLINE_LAMBDA {
            return lookup(m_marg_cdf.data(), offset_marg + idx,
                          n_marg, param_weight, mask);
        };

        if (!m_normalized)
            sample.y() *= fetch_marginal(n_marg - 1, active);

        UInt32 row = enoki::binary_search(
            0u, n_marg - 1,
            [&](UInt32 idx) ENOKI_INLINE_LAMBDA {
                return fetch_marginal(idx, active) < sample.y();
            });

        /// Subtract the marginal CDF value up to the current interval
        sample.y() -= fetch_marginal(row - 1, active && row > 0);

        UInt32 offset_cond = slice_offset * n_cond + row * (m_size.x() - 1);

        /// Look up conditional CDF values of surrounding rows for x == 1
        Float r0 = lookup(m_cond_cdf.data() + 1 * (m_size.x() - 1) - 1,
                          offset_cond, n_cond, param_weight, active),
              r1 = lookup(m_cond_cdf.data() + 2 * (m_size.x() - 1) - 1,
                          offset_cond, n_cond, param_weight, active);

        sample.y() = sample_segment(sample.y(), m_inv_patch_size.y(), r0, r1);

        // Multiply sample.x() by the integrated density along the 'x' axis
        sample.x() *= lerp(r0, r1, sample.y());

        // Sample the column next
        auto fetch_conditional = [&](UInt32 idx, Mask mask)
                                     ENOKI_INLINE_LAMBDA {
            idx += offset_cond;
            Float v0 = lookup(m_cond_cdf.data(),
                              idx, n_cond, param_weight, mask),
                  v1 = lookup(m_cond_cdf.data() + m_size.x() - 1,
                              idx, n_cond, param_weight, mask);
            return lerp(v0, v1, sample.y());
        };

        UInt32 col = enoki::binary_search(
            0, m_size.x() - 1,
            [&](UInt32 idx) ENOKI_INLINE_LAMBDA {
                return fetch_conditional(idx, active) < sample.x();
            });

        /// Subtract the CDF value up to the current interval
        sample.x() -= fetch_conditional(col - 1, active && col > 0);

        UInt32 offset_data = slice_offset * n_data + row * m_size.x() + col;

        Float v00 = lookup(m_data.data(), offset_data, n_data,
                           param_weight, active),
              v10 = lookup(m_data.data() + 1, offset_data, n_data,
                           param_weight, active),
              v01 = lookup(m_data.data() + m_size.x(), offset_data,
                           n_data, param_weight, active),
              v11 = lookup(m_data.data() + m_size.x() + 1, offset_data,
                           n_data, param_weight, active),
              c0  = lerp(v00, v01, sample.y()),
              c1  = lerp(v10, v11, sample.y());

        sample.x() = sample_segment(sample.x(), m_inv_patch_size.x(), c0, c1);

        return {
            (Point2i(Point2u(col, row)) + sample) * m_patch_size,
            lerp(c0, c1, sample.x())
        };
    }

    MTS_INLINE
    std::pair<Point2f, Float> invert_continuous(Point2f sample,
                                                 const Float *param,
                                                 Mask active) const {
        MTS_MASK_ARGUMENT(active);

        // Size of a slice of various tables (conditional/marginal/data)
        uint32_t n_cond = m_size.y() * (m_size.x() - 1),
                 n_marg = m_size.y() - 1,
                 n_data = hprod(m_size);

        /// Find offset and interpolation weights wrt. conditional parameters
        Float param_weight[2 * DimensionInt];
        UInt32 slice_offset = interpolate_weights(param, param_weight, active);

        // Avoid issues with roundoff error
        sample = clamp(sample, 0.f, 1.f);

        // Fetch values at corners of bilinear patch
        sample *= m_inv_patch_size;
        Point2u pos = min(Point2u(Point2i(sample)), m_size - 2u);
        sample -= Point2f(Point2i(pos));

        UInt32 offset_data =
            slice_offset * n_data + pos.y() * m_size.x() + pos.x();

        // Invert the X component
        Float v00 = lookup(m_data.data(), offset_data, n_data,
                           param_weight, active),
              v10 = lookup(m_data.data() + 1, offset_data, n_data,
                           param_weight, active),
              v01 = lookup(m_data.data() + m_size.x(), offset_data,
                           n_data, param_weight, active),
              v11 = lookup(m_data.data() + m_size.x() + 1, offset_data,
                           n_data, param_weight, active);

        Float c0  = lerp(v00, v01, sample.y()),
              c1  = lerp(v10, v11, sample.y()),
              pdf = lerp(c0, c1, sample.x());

        sample.x() = invert_segment(sample.x(), m_patch_size.x(), c0, c1);

        UInt32 offset_cond =
            slice_offset * n_cond + pos.y() * (m_size.x() - 1);

        auto fetch_conditional = [&](UInt32 idx, Mask mask)
                                     ENOKI_INLINE_LAMBDA {
            idx += offset_cond;
            Float v0 = lookup(m_cond_cdf.data(),
                              idx, n_cond, param_weight, mask),
                  v1 = lookup(m_cond_cdf.data() + m_size.x() - 1,
                              idx, n_cond, param_weight, mask);
            return lerp(v0, v1, sample.y());
        };

        sample.x() += fetch_conditional(pos.x() - 1, active && pos.x() > 0);

        Float r0 = lookup(m_cond_cdf.data() + 1 * (m_size.x() - 1) - 1,
                          offset_cond, n_cond, param_weight, active),
              r1 = lookup(m_cond_cdf.data() + 2 * (m_size.x() - 1) - 1,
                          offset_cond, n_cond, param_weight, active);

        sample.x() /= lerp(r0, r1, sample.y());

        // Invert the Y component
        sample.y() = invert_segment(sample.y(), m_patch_size.y(), r0, r1);

        UInt32 offset_marg = slice_offset * n_marg;
        sample.y() += lookup(m_marg_cdf.data(), offset_marg + pos.y() - 1,
                             m_size.y(), param_weight, active && pos.y() > 0);

        if (!m_normalized)
            sample.y() /= lookup(m_marg_cdf.data(), offset_marg + n_marg - 1,
                          n_marg, param_weight, active);

        return { sample, pdf };
    }

    MTS_INLINE Float sample_segment(Float sample, ScalarFloat inv_width,
                                    Float v0, Float v1) const {
        Mask non_const = abs(v0 - v1) > 1e-4f * (v0 + v1);
        Float divisor = select(non_const, v0 - v1, v0 + v1);
        sample *= 2.f * inv_width;
        masked(sample, non_const) =
            v0 - safe_sqrt(sqr(v0) + sample * (v1 - v0));
        masked(sample, neq(divisor, 0.f)) /= divisor;
        return sample;
    }

    MTS_INLINE Float invert_segment(Float sample, ScalarFloat width,
                                    Float v0, Float v1) const {
        return sample * lerp(v0, v1, .5f * sample) * width;
    }
protected:
    /// Resolution of the discretized density function
    ScalarVector2u m_size;

    /// Density values
    FloatStorage m_data;

    /// Marginal and conditional PDFs
    FloatStorage m_marg_cdf;
    FloatStorage m_cond_cdf;

    /// Are the probability values normalized?
    bool m_normalized;
};

//! @}
// =======================================================================

NAMESPACE_END(mitsuba)
