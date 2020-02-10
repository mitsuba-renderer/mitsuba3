#pragma once

#include <mitsuba/core/warp.h>
#include <mitsuba/core/util.h>

NAMESPACE_BEGIN(mitsuba)

/** =======================================================================
 * @{ \name Data-driven warping techniques for two dimensions
 *
 * This file provides two different classes (one of which has two 'variants')
 * for importance sampling arbitrary 2D linear interpolants discretized on a
 * regular grid. All produce exactly the same probability density, but the
 * mapping from random numbers to samples is very different. In particular:
 *
 * \c Hierarchical2D generates samples using hierarchical sample warping, which
 * is essentially a course-to-fine traversal of a MIP map. It generates a
 * mapping with very little shear/distortion, but it has numerous
 * discontinuities that can be problematic for some applications.
 *
 * \c Marginal2D samples a marginal distribution in on axis, then a conditional
 * distribution in the other axis. The mapping lacks the discontinuities of the
 * hierarchical scheme but tends to have significant shear/distortion when the
 * distribution contains isolated regions with a very high probability density.
 *
 * There are two variants of \c Marginal2D: when <tt>Continuous=false</tt>,
 * discrete marginal/conditional distributions are used to select a bilinear
 * bilinear patch, followed by a continuous sampling step that chooses a
 * specific position inside the patch. When <tt>Continuous=true</tt>,
 * continuous marginal/conditional distributions are used instead, and the
 * second step is no longer needed. The latter scheme requires more computation
 * and memory accesses but produces an overall smoother mapping.
 *
 * The implementation also supports <em>conditional distributions</em>, i.e.,
 * 2D distributions that depend on an arbitrary number of parameters (indicated
 * via the \c Dimension template parameter). In this case, a higher-dimensional
 * discretization must be provided that will also be linearly interpolated in
 * these extra dimensions.
 * =======================================================================
 */

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
        ScalarVector2u n_patches = size - 1u;

        m_patch_size = 1.f / n_patches;
        m_inv_patch_size = n_patches;

        // Dependence on additional parameters
        m_slices = 1u;
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
                        return gather<Float>(m_param_values[dim], idx, active) < param[dim];
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
            return 0.f;
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
     * If \c build_hierarchy is set to \c false, the implementation will not
     * construct the hierarchy needed for sample warping, which saves memory
     * in case this functionality is not needed (e.g. if only the interpolation
     * in \c eval() is used). In this case, \c sample() and \c invert()
     * can still be called without triggering undefined behavior, but they
     * will not return meaningful results.
     */
    Hierarchical2D(const ScalarFloat *data,
                   const ScalarVector2u &size,
                   const std::array<uint32_t, Dimension> &param_res = { },
                   const std::array<const ScalarFloat *, Dimension> &param_values = { },
                   bool normalize = true,
                   bool build_hierarchy = true)
        : Base(size, param_res, param_values) {

        // The linear interpolant has 'size-1' patches
        ScalarVector2u n_patches = size - 1u;

        // Keep track of the dependence on additional parameters (optional)
        uint32_t max_level = math::log2i_ceil(hmax(n_patches));

        m_max_patch_index = n_patches - 1u;

        if (!build_hierarchy) {
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

            Float v00 = level.template lookup<>(offset_i, m_param_strides,
                                                param_weight, active);
            offset_i += 1u;

            Float v10 = level.template lookup<>(offset_i, m_param_strides,
                                                param_weight, active);
            offset_i += 1u;

            Float v01 = level.template lookup<>(offset_i, m_param_strides,
                                                param_weight, active);
            offset_i += 1u;

            Float v11 = level.template lookup<>(offset_i, m_param_strides,
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
        Float v00 = level0.template lookup<>(offset_i, m_param_strides,
                                             param_weight, active);

        Float v10 = level0.template lookup<>(offset_i + 1, m_param_strides,
                                             param_weight, active);

        Float v01 = level0.template lookup<>(offset_i + level0.width,
                                             m_param_strides, param_weight,
                                             active);

        Float v11 = level0.template lookup<>(offset_i + level0.width + 1,
                                             m_param_strides, param_weight,
                                             active);

        Float pdf;
        std::tie(sample, pdf) =
            warp::square_to_bilinear(v00, v10, v01, v11, sample);

        return {
            (Point2f(Point2i(offset)) + sample) * m_patch_size,
            pdf
        };
    }

    /// Inverse of the mapping implemented in \c sample()
    std::pair<Point2f, Float> invert(Point2f sample, const Float *param = nullptr,
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

        Float v00 = level0.template lookup<>(offset_i, m_param_strides,
                                             param_weight, active);

        Float v10 = level0.template lookup<>(offset_i + 1, m_param_strides,
                                             param_weight, active);

        Float v01 = level0.template lookup<>(offset_i + level0.width,
                                             m_param_strides, param_weight,
                                             active);

        Float v11 = level0.template lookup<>(offset_i + level0.width + 1,
                                             m_param_strides, param_weight,
                                             active);

        sample -= Point2f(Point2i(offset));

        Float pdf;
        std::tie(sample, pdf) = warp::bilinear_to_square(v00, v10, v01, v11, sample);

        // Hierarchical sample warping -- reverse direction
        for (int l = 1; l < (int) m_levels.size() - 1; ++l) {
            const Level &level = m_levels[l];

            // Fetch values from next MIP level
            offset_i = level.index(offset & ~1u) + slice_offset * level.size;

            v00 = level.template lookup<>(offset_i, m_param_strides,
                                          param_weight, active);
            offset_i += 1u;

            v10 = level.template lookup<>(offset_i, m_param_strides,
                                          param_weight, active);
            offset_i += 1u;

            v01 = level.template lookup<>(offset_i, m_param_strides,
                                          param_weight, active);
            offset_i += 1u;

            v11 = level.template lookup<>(offset_i, m_param_strides,
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

        Float v00 = level0.template lookup<>(offset_i, m_param_strides,
                                             param_weight, active);

        Float v10 = level0.template lookup<>(offset_i + 1, m_param_strides,
                                             param_weight, active);

        Float v01 = level0.template lookup<>(offset_i + level0.width,
                                             m_param_strides, param_weight,
                                             active);

        Float v11 = level0.template lookup<>(offset_i + level0.width + 1,
                                             m_param_strides, param_weight,
                                             active);

        return warp::square_to_bilinear_pdf(v00, v10, v01, v11, pos);
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "Hierarchical2D<" << Dimension << ">[" << std::endl
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

                if constexpr (is_scalar_v<Float>)
                    return gather<Float>(data, i0);
                else
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
 * \remark The Python API exposes explicitly instantiated versions of this
 * class named \c Marginal2DDiscrete0 to \c Marginal2DDiscrete3 and
 * \c Marginal2DContinuous0 to \c Marginal2DContinuous3  and for data that depends
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
     * If \c build_cdf is set to \c false, the implementation will not
     * construct the cdf needed for sample warping, which saves memory in case
     * this functionality is not needed (e.g. if only the interpolation in \c
     * eval() is used).
     */
    Marginal2D(const ScalarFloat *data,
               const ScalarVector2u &size,
               const std::array<uint32_t, Dimension> &param_res = { },
               const std::array<const ScalarFloat *, Dimension> &param_values = { },
               bool normalize = true, bool build_cdf = true)
        : Base(size, param_res, param_values), m_size(size), m_normalized(normalize) {

        uint32_t w          = m_size.x(),
                 h          = m_size.y(),
                 n_values   = w * h,
                 n_marginal = h - 1,
                 n_conditional;

        if constexpr (Continuous)
            n_conditional = h * (w - 1);
        else
            n_conditional = (h - 1) * (w - 1);

        m_data = empty<FloatStorage>(m_slices * n_values);
        m_data.managed();

        if (build_cdf) {
            m_marg_cdf = empty<FloatStorage>(m_slices * n_marginal);
            m_marg_cdf.managed();

            m_cond_cdf = empty<FloatStorage>(m_slices * n_conditional);
            m_cond_cdf.managed();

            ScalarFloat *marginal_cdf    = m_marg_cdf.data(),
                        *conditional_cdf = m_cond_cdf.data(),
                        *data_out = m_data.data();

            double scale = hprod(1.0 / (m_size - 1))
                * (Continuous ? .5 : 0.25);

            for (uint32_t slice = 0; slice < m_slices; ++slice) {
                /* The continuous / discrete cases require different
                   tables with marginal/conditional probabilities */
                double sum = 0.0;
                if constexpr (Continuous) {
                    // Construct conditional CDF
                    for (uint32_t y = 0; y < h; ++y) {
                        uint32_t i = y * w, j = y * (w - 1);
                        for (uint32_t x = 0; x < w - 1; ++x, ++i, ++j) {
                            sum += scale * ((double) data[i] +
                                            (double) data[i + 1]);
                            conditional_cdf[j] = (ScalarFloat) sum;
                        }
                    }

                    // Construct marginal CDF
                    sum = 0.0;
                    for (uint32_t y = 0; y < h - 1; ++y) {
                        sum += .5 * ((double) conditional_cdf[(y + 1) * (w - 1) - 1] +
                                     (double) conditional_cdf[(y + 2) * (w - 1) - 1]);
                        marginal_cdf[y] = (ScalarFloat) sum;
                    }
                } else {
                    // Construct conditional CDF
                    for (uint32_t y = 0; y < h - 1; ++y) {
                        uint32_t i = y * w, j = y * (w - 1);
                        for (uint32_t x = 0; x < w - 1; ++x, ++i, ++j) {
                            sum += scale * ((double) data[i] +
                                            (double) data[i + 1] +
                                            (double) data[i + w] +
                                            (double) data[i + w + 1]);
                            conditional_cdf[j] = (ScalarFloat) sum;
                        }
                    }

                    // Construct marginal CDF
                    sum = 0.0;
                    for (uint32_t y = 0; y < h - 1; ++y) {
                        sum += (double) conditional_cdf[(y + 1) * (w - 1) - 1];
                        marginal_cdf[y] = (ScalarFloat) sum;
                    }
                }

                // Normalize CDFs and PDF (if requested)
                ScalarFloat norm = 1.f;
                if (normalize)
                    norm = ScalarFloat(1.0 / sum);

                for (size_t i = 0; i < n_conditional; ++i)
                    *conditional_cdf++ *= norm;
                for (size_t i = 0; i < n_marginal; ++i)
                    *marginal_cdf++ *= norm;
                for (size_t i = 0; i < n_values; ++i)
                    *data_out++ = *data++ * norm;

                marginal_cdf += m_size.y();
                conditional_cdf += n_values;
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
                            ScalarFloat v00 = data[i],
                                        v10 = data[i + 1],
                                        v01 = data[i + w],
                                        v11 = data[i + w + 1],
                                        avg = .25f * (v00 + v10 + v01 + v11);
                            sum += (double) avg;
                        }
                    }
                    norm = ScalarFloat(1.0 / sum);
                }

                for (uint32_t k = 0; k < n_values; ++k)
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

        if constexpr (Continuous)
            return sample_continuous(sample, param, active);
        else
            return sample_discrete(sample, param, active);
    }

    /// Inverse of the mapping implemented in \c sample()
    std::pair<Point2f, Float> invert(const Point2f &sample,
                                     const Float *param = nullptr,
                                     Mask active = true) const {
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

        Float v00 = lookup<>(m_data.data(), index,
                             size, param_weight, active),
              v10 = lookup<>(m_data.data() + 1, index,
                             size, param_weight, active),
              v01 = lookup<>(m_data.data() + m_size.x(), index,
                             size, param_weight, active),
              v11 = lookup<>(m_data.data() + m_size.x() + 1, index,
                             size, param_weight, active);

        return warp::square_to_bilinear_pdf(v00, v10, v01, v11, pos);
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "Marginal2D<" << Dimension << ">[" << std::endl
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
        uint32_t size_cond = hprod(m_size - 1),
                 size_marg = m_size.y() - 1,
                 size_data = hprod(m_size);

        /// Find offset and interpolation weights wrt. conditional parameters
        Float param_weight[2 * DimensionInt];
        UInt32 slice_offset = interpolate_weights(param, param_weight, active);

        // Avoid degeneracies on the domain boundary
        sample = clamp(sample, math::Epsilon<Float>, math::OneMinusEpsilon<Float>);

        /// Multiply by last entry of marginal CDF if the data is not normalized
        UInt32 offset_marg = slice_offset * size_marg;
        if (!m_normalized)
            sample.y() *= lookup<>(m_marg_cdf.data(),
                                   offset_marg + size_marg - 1,
                                   size_marg, param_weight, active);

        // Sample the row from the marginal distribution
        UInt32 row = enoki::binary_search(
            0u, size_marg - 1u, [&](UInt32 idx) ENOKI_INLINE_LAMBDA {
                return lookup<>(m_marg_cdf.data(), offset_marg + idx,
                                size_marg, param_weight,
                                active) < sample.y();
            });

        // Re-scale uniform variate
        Float row_cdf_0 = lookup<>(m_marg_cdf.data(), offset_marg + row - 1,
                                   size_marg, param_weight, active && row > 0),
              row_cdf_1 = lookup<>(m_marg_cdf.data(), offset_marg + row,
                                   size_marg, param_weight, active);

        sample.y() -= row_cdf_0;
        masked(sample.y(), neq(row_cdf_1, row_cdf_0)) /= row_cdf_1 - row_cdf_0;

        /// Multiply by last entry of conditional CDF
        UInt32 offset_cond =
            slice_offset * size_cond + row * (m_size.x() - 1);
        sample.x() *= lookup<>(m_cond_cdf.data(),
                               offset_cond + m_size.x() - 2,
                               size_cond, param_weight, active);

        // Sample the column from the conditional distribution
        UInt32 col = enoki::binary_search(
            0u, size_cond - 1u, [&](UInt32 idx) ENOKI_INLINE_LAMBDA {
                return lookup<>(m_cond_cdf.data(),
                                offset_cond + idx, size_cond,
                                param_weight, active) < sample.x();
            });

        // Re-scale uniform variate
        Float col_cdf_0 = lookup<>(m_cond_cdf.data(), offset_cond + col - 1,
                                   size_cond, param_weight, active && col > 0),
              col_cdf_1 = lookup<>(m_cond_cdf.data(), offset_cond + col,
                                   size_cond, param_weight, active);

        sample.x() -= col_cdf_0;
        masked(sample.x(), neq(col_cdf_1, col_cdf_0)) /= col_cdf_1 - col_cdf_0;

        // Sample a position on the bilinear patch
        UInt32 offset_data = slice_offset * size_data + row * m_size.x() + col;
        Float v00 = lookup<>(m_data.data(), offset_data, size_data,
                             param_weight, active),
              v10 = lookup<>(m_data.data() + 1, offset_data, size_data,
                             param_weight, active),
              v01 = lookup<>(m_data.data() + m_size.x(), offset_data,
                             size_data, param_weight, active),
              v11 = lookup<>(m_data.data() + m_size.x() + 1, offset_data,
                             size_data, param_weight, active);

        Float pdf;
        std::tie(sample, pdf) =
            warp::square_to_bilinear(v00, v10, v01, v11, sample);

        return { (Point2u(col, row) + sample) * m_patch_size, pdf };
    }

    MTS_INLINE
    std::pair<Point2f, Float> invert_discrete(Point2f sample,
                                              const Float *param,
                                              Mask active) const {
        MTS_MASK_ARGUMENT(active);

        // Size of a slice of various tables (conditional/marginal/data)
        uint32_t size_cond = hprod(m_size - 1),
                 size_marg = m_size.y() - 1,
                 size_data = hprod(m_size);

        /// Find offset and interpolation weights wrt. conditional parameters
        Float param_weight[2 * DimensionInt];
        UInt32 slice_offset = interpolate_weights(param, param_weight, active);

        // Avoid issues with roundoff error
        sample = clamp(sample, 0.f, 1.f);

        // Fetch values at corners of bilinear patch
        sample *= m_inv_patch_size;
        Point2u offset = min(Point2u(Point2i(sample)), m_size - 2u);
        UInt32 index = offset.x() + offset.y() * m_size.x() + slice_offset * size_data;
        sample -= Point2f(Point2i(offset));

        Float v00 = lookup<>(m_data.data(), index,
                             size_data, param_weight, active),
              v10 = lookup<>(m_data.data() + 1, index,
                             size_data, param_weight, active),
              v01 = lookup<>(m_data.data() + m_size.x(), index,
                             size_data, param_weight, active),
              v11 = lookup<>(m_data.data() + m_size.x() + 1, index,
                             size_data, param_weight, active);

        Float pdf;
        std::tie(sample, pdf) = warp::bilinear_to_square(v00, v10, v01, v11, sample);

        UInt32 cond_offset = slice_offset * size_cond + offset.y() * (m_size.x() - 1),
               marg_offset = slice_offset * size_marg;

        Float row_cdf_0 = lookup<>(m_marg_cdf.data(), marg_offset + offset.y() - 1,
                                   size_marg, param_weight, active && offset.y() > 0),
              row_cdf_1 = lookup<>(m_marg_cdf.data(), marg_offset + offset.y(),
                                   size_marg, param_weight, active),
              col_cdf_0 = lookup<>(m_cond_cdf.data(), cond_offset + offset.x() - 1,
                                   size_cond, param_weight, active && offset.x() > 0),
              col_cdf_1 = lookup<>(m_cond_cdf.data(), cond_offset + offset.x(),
                                   size_cond, param_weight, active);

        sample.x() = fmadd(sample.x(), col_cdf_1 - col_cdf_0, col_cdf_0);
        sample.y() = fmadd(sample.y(), row_cdf_1 - row_cdf_0, row_cdf_0);

        sample.x() /= lookup<>(m_cond_cdf.data(),
                               cond_offset + m_size.x() - 2,
                               size_cond, param_weight, active);

        if (!m_normalized)
            sample.y() /= lookup<>(m_marg_cdf.data(),
                                   marg_offset + m_size.y() - 2,
                                   size_marg, param_weight, active);

        return { sample, pdf };
    }

    MTS_INLINE
    std::pair<Point2f, Float> sample_continuous(Point2f sample,
                                                const Float *param,
                                                Mask active) const {
        MTS_MASK_ARGUMENT(active);

        Float param_weight[2 * DimensionInt];
        UInt32 slice_offset = interpolate_weights(param, param_weight, active);
        ENOKI_MARK_USED(slice_offset);

        // Avoid degeneracies on the domain boundary
        sample = clamp(sample, math::Epsilon<Float>, math::OneMinusEpsilon<Float>);

        // Sample the row first
        UInt32 marg_offset = 0;
        if constexpr (Dimension != 0)
            marg_offset = slice_offset * m_size.y();

        auto fetch_marginal = [&](UInt32 idx) ENOKI_INLINE_LAMBDA -> Float {
            return lookup<>(m_marg_cdf.data(), marg_offset + idx,
                            m_size.y(), param_weight, active);
        };

        UInt32 row = math::find_interval(
            m_size.y(),
            [&](UInt32 idx) ENOKI_INLINE_LAMBDA {
                return fetch_marginal(idx) < sample.y();
            });

        sample.y() -= fetch_marginal(row);

        uint32_t slice_size = hprod(m_size);
        UInt32 cond_offset = row * m_size.x();
        if constexpr (Dimension != 0)
            cond_offset += slice_offset * slice_size;

        Float r0 = lookup<>(m_cond_cdf.data(),
                                     cond_offset + m_size.x() - 1, slice_size,
                                     param_weight, active),
              r1 = lookup<>(m_cond_cdf.data(),
                                     cond_offset + (m_size.x() * 2 - 1), slice_size,
                                     param_weight, active);

        Mask is_const = abs(r0 - r1) < 1e-4f * (r0 + r1);
        sample.y() = select(is_const, 2.f * sample.y(),
            r0 - safe_sqrt(r0 * r0 - 2.f * sample.y() * (r0 - r1)));
        sample.y() /= select(is_const, r0 + r1, r0 - r1);

        // Sample the column next
        sample.x() *= (1.f - sample.y()) * r0 + sample.y() * r1;

        auto fetch_conditional = [&](UInt32 idx) ENOKI_INLINE_LAMBDA -> Float {
            Float v0 = lookup<>(m_cond_cdf.data(), cond_offset + idx,
                                         slice_size, param_weight, active),
                  v1 = lookup<>(m_cond_cdf.data() + m_size.x(),
                                         cond_offset + idx, slice_size, param_weight, active);

            return (1.f - sample.y()) * v0 + sample.y() * v1;
        };

        UInt32 col = math::find_interval(
            m_size.x(),
            [&](UInt32 idx) ENOKI_INLINE_LAMBDA {
                return fetch_conditional(idx) < sample.x();
            });

        sample.x() -= fetch_conditional(col);

        cond_offset += col;

        Float v00 = lookup<>(m_data.data(), cond_offset, slice_size,
                                      param_weight, active),
              v10 = lookup<>(m_data.data() + 1, cond_offset, slice_size,
                                      param_weight, active),
              v01 = lookup<>(m_data.data() + m_size.x(), cond_offset,
                                      slice_size, param_weight, active),
              v11 = lookup<>(m_data.data() + m_size.x() + 1, cond_offset,
                                      slice_size, param_weight, active),
              c0  = fmadd((1.f - sample.y()), v00, sample.y() * v01),
              c1  = fmadd((1.f - sample.y()), v10, sample.y() * v11);

        is_const = abs(c0 - c1) < 1e-4f * (c0 + c1);
        sample.x() = select(is_const, 2.f * sample.x(),
            c0 - safe_sqrt(c0 * c0 - 2.f * sample.x() * (c0 - c1)));
        Float divisor = select(is_const, c0 + c1, c0 - c1);
        masked(sample.x(), neq(divisor, 0.f)) /= divisor;

        return {
            (Point2u(col, row) + sample) * m_patch_size,
            fmadd(1.f - sample.x(), c0, sample.x() * c1) * hprod(m_inv_patch_size)
        };
    }

    MTS_INLINE
    std::pair<Point2f, Float> invert_continuous(Point2f sample,
                                                 const Float *param,
                                                 Mask active) const {
        MTS_MASK_ARGUMENT(active);

        Float param_weight[2 * DimensionInt];
        UInt32 slice_offset = interpolate_weights(param, param_weight, active);

        // Avoid issues with roundoff error
        sample = clamp(sample, 0.f, 1.f);

        // Fetch values at corners of bilinear patch
        sample *= m_inv_patch_size;
        Point2u pos = min(Point2u(Point2i(sample)), m_size - 2u);
        sample -= Point2f(Point2i(pos));

        UInt32 offset = pos.x() + pos.y() * m_size.x();
        uint32_t slice_size = hprod(m_size);
        if constexpr (Dimension != 0)
            offset += slice_offset * slice_size;

        // Invert the X component
        Float v00 = lookup<>(m_data.data(), offset, slice_size,
                                      param_weight, active),
              v10 = lookup<>(m_data.data() + 1, offset, slice_size,
                                      param_weight, active),
              v01 = lookup<>(m_data.data() + m_size.x(), offset, slice_size,
                                      param_weight, active),
              v11 = lookup<>(m_data.data() + m_size.x() + 1, offset, slice_size,
                                      param_weight, active);

        Point2f w1 = sample, w0 = 1.f - w1;

        Float c0  = fmadd(w0.y(), v00, w1.y() * v01),
              c1  = fmadd(w0.y(), v10, w1.y() * v11),
              pdf = fmadd(w0.x(), c0, w1.x() * c1);

        sample.x() *= c0 + .5f * sample.x() * (c1 - c0);

        Float v0 = lookup<>(m_cond_cdf.data(), offset,
                                     slice_size, param_weight, active),
              v1 = lookup<>(m_cond_cdf.data() + m_size.x(),
                                     offset, slice_size, param_weight, active);

        sample.x() += (1.f - sample.y()) * v0 + sample.y() * v1;

        offset = pos.y() * m_size.x();
        if (Dimension != 0)
            offset += slice_offset * slice_size;

        Float r0 = lookup<>(m_cond_cdf.data(),
                                     offset + m_size.x() - 1, slice_size,
                                     param_weight, active),
              r1 = lookup<>(m_cond_cdf.data(),
                                     offset + (m_size.x() * 2 - 1), slice_size,
                                     param_weight, active);

        sample.x() /= (1.f - sample.y()) * r0 + sample.y() * r1;

        // Invert the Y component
        sample.y() *= r0 + .5f * sample.y() * (r1 - r0);

        offset = pos.y();
        if (Dimension != 0)
            offset += slice_offset * m_size.y();

        sample.y() += lookup<>(m_marg_cdf.data(), offset,
                                        m_size.y(), param_weight, active);

        return { sample, pdf * hprod(m_inv_patch_size) };
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

