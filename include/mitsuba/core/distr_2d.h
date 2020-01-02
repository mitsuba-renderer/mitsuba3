#pragma once

#include <mitsuba/core/math.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/util.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Data-driven warping techniques for two dimensions
// =======================================================================

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
 * distortion), which makes it an ideal choice for structured point sets such
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
template <typename Float, size_t Dimension = 0> class Hierarchical2D {
private:
    MTS_IMPORT_CORE_TYPES()
    using FloatStorage = DynamicBuffer<Float>;

#if !defined(_MSC_VER)
    static constexpr size_t ArraySize = Dimension;
#else
    static constexpr size_t ArraySize = (Dimension != 0) ? Dimension : 1;
#endif

public:
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
    Hierarchical2D(const ScalarVector2u &size, const ScalarFloat *data,
                   std::array<uint32_t, Dimension> param_res = { },
                   std::array<const ScalarFloat *, Dimension> param_values = { },
                   bool normalize = true,
                   bool build_hierarchy = true) {
        if (any(size < 2))
            Throw("Hierarchical2D(): input array resolution must be >= 2!");

        // The linear interpolant has 'size-1' patches
        ScalarVector2u n_patches = size - 1u;

        // Keep track of the dependence on additional parameters (optional)
        uint32_t max_level   = math::log2i_ceil(hmax(n_patches)),
                 slices      = 1u;
        for (int i = (int) Dimension - 1; i >= 0; --i) {
            if (param_res[i] < 1)
                Throw("Hierarchical2D(): parameter resolution must be >= 1!");

            m_param_size[i] = param_res[i];
            m_param_values[i].resize(param_res[i]);
            memcpy(m_param_values[i].data(), param_values[i],
                   sizeof(ScalarFloat) * param_res[i]);
            m_param_strides[i] = param_res[i] > 1 ? slices : 0;
            slices *= m_param_size[i];
        }

        m_patch_size = 1.f / n_patches;
        m_inv_patch_size = n_patches;
        m_max_patch_index = n_patches - 1;

        if (!build_hierarchy) {
            m_levels.reserve(1);
            m_levels.emplace_back(size, slices);

            for (uint32_t slice = 0; slice < slices; ++slice) {
                uint32_t offset = m_levels[0].size * slice;

                ScalarFloat scale = 1.f;
                if (normalize) {
                    double sum = 0.0;
                    for (uint32_t i = 0; i < m_levels[0].size; ++i)
                        sum += (double) data[offset + i];
                    scale = hprod(n_patches) / (ScalarFloat) sum;
                }
                for (uint32_t i = 0; i < m_levels[0].size; ++i)
                    m_levels[0].data[offset + i] = data[offset + i] * scale;
            }

            return;
        }

        // Allocate memory for input array and MIP hierarchy
        m_levels.reserve(max_level + 2);
        m_levels.emplace_back(size, slices);

        ScalarVector2u level_size = n_patches;
        for (int level = max_level; level >= 0; --level) {
            level_size += level_size & 1u; // zero-pad
            m_levels.emplace_back(level_size, slices);
            level_size = sr<1>(level_size);
        }

        for (uint32_t slice = 0; slice < slices; ++slice) {
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
            ScalarFloat scale = normalize ? (hprod(n_patches) / (ScalarFloat) sum) : 1.f;
            for (uint32_t i = 0; i < m_levels[0].size; ++i)
                m_levels[0].data[offset0 + i] = data[offset0 + i] * scale;
            for (uint32_t i = 0; i < m_levels[1].size; ++i)
                m_levels[1].data[offset1 + i] *= scale;

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
    template <typename Vector2f, typename Value = value_t<Vector2f>>
    std::pair<Vector2f, Value> sample(Vector2f sample, const Value *param = nullptr,
                                      mask_t<Value> active = true) const {
        using Vector2u = uint32_array_t<Vector2f>;
        using UInt32 = value_t<Vector2u>;
        using Mask = mask_t<Value>;

        // Look up parameter-related indices and weights (if Dimension != 0)
        Value param_weight[2 * ArraySize];
        UInt32 slice_offset = zero<UInt32>();
        for (size_t dim = 0; dim < Dimension; ++dim) {
            if (unlikely(m_param_size[dim] == 1)) {
                param_weight[2 * dim] = 1.f;
                param_weight[2 * dim + 1] = 0.f;
                continue;
            }

            UInt32 param_index = math::find_interval(
                m_param_size[dim],
                [&](UInt32 idx, Mask active_) {
                    return gather<Value>(m_param_values[dim].data(), idx,
                                         active_) <= param[dim];
                },
                active);

            Value p0 = gather<Value>(m_param_values[dim].data(), param_index, active),
                  p1 = gather<Value>(m_param_values[dim].data(), param_index + 1, active);

            param_weight[2 * dim + 1] =
                clamp((param[dim] - p0) / (p1 - p0), 0.f, 1.f);
            param_weight[2 * dim] = 1.f - param_weight[2 * dim + 1];
            slice_offset += m_param_strides[dim] * param_index;
        }

        // Hierarchical sample warping
        Vector2u offset = zero<Vector2u>();
        for (int l = (int) m_levels.size() - 2; l > 0; --l) {
            const Level &level = m_levels[l];

            offset = sl<1>(offset);

            // Fetch values from next MIP level
            UInt32 offset_i = level.index(offset);
            if (Dimension != 0)
                offset_i += slice_offset * level.size;

            Value v00 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);
            offset_i += 1u;

            Value v10 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);
            offset_i += 1u;

            Value v01 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);
            offset_i += 1u;

            Value v11 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);

            // Avoid issues with roundoff error
            sample = clamp(sample, 0.f, 1.f);

            // Select the row
            Value r0 = v00 + v10,
                  r1 = v01 + v11;
            sample.y() *= r0 + r1;
            mask_t<Value> mask = sample.y() > r0;
            masked(offset.y(), mask) += 1;
            masked(sample.y(), mask) -= r0;
            sample.y() /= select(mask, r1, r0);

            // Select the column
            Value c0 = select(mask, v01, v00),
                  c1 = select(mask, v11, v10);
            sample.x() *= c0 + c1;
            mask = sample.x() > c0;
            masked(sample.x(), mask) -= c0;
            sample.x() /= select(mask, c1, c0);
            masked(offset.x(), mask) += 1;
        }

        const Level &level0 = m_levels[0];

        UInt32 offset_i = offset.x() + offset.y() * level0.width;
        if (Dimension != 0)
            offset_i += slice_offset * level0.size;

        // Fetch corners of bilinear patch
        Value v00 = level0.template lookup<Dimension>(
            offset_i, m_param_strides, param_weight, active);

        Value v10 = level0.template lookup<Dimension>(
            offset_i + 1, m_param_strides, param_weight, active);

        Value v01 = level0.template lookup<Dimension>(
            offset_i + level0.width, m_param_strides, param_weight, active);

        Value v11 = level0.template lookup<Dimension>(
            offset_i + level0.width + 1, m_param_strides, param_weight, active);

        Value r0   = v00 + v10,
              r1   = v01 + v11,
              r0_2 = r0 * r0,
              r1_2 = r1 * r1;

        // Invert marginal CDF in the 'y' parameter
        masked(sample.y(), abs(r0 - r1) > 1e-4f * (r0 + r1)) =
            (r0 - safe_sqrt(r0_2 + sample.y() * (r1_2 - r0_2))) / (r0 - r1);

        // Invert conditional CDF in the 'x' parameter
        Value c0   = fmadd(1.f - sample.y(), v00, sample.y() * v01),
              c1   = fmadd(1.f - sample.y(), v10, sample.y() * v11),
              c0_2 = c0 * c0,
              c1_2 = c1 * c1;

        masked(sample.x(), abs(c0 - c1) > 1e-4f * (c0 + c1)) =
            (c0 - safe_sqrt(c0_2 + sample.x() * (c1_2 - c0_2))) /
            (c0 - c1);

        return {
            (Vector2f(offset) + sample) * m_patch_size,
            fmadd(1.f - sample.x(), c0, sample.x() * c1)
        };
    }

    /// Inverse of the mapping implemented in \c sample()
    template <typename Vector2f, typename Value = value_t<Vector2f>>
    std::pair<Vector2f, Value> invert(Vector2f sample, const Value *param = nullptr,
                                      mask_t<Value> active = true) const {
        using Vector2u = uint32_array_t<Vector2f>;
        using Vector2i = int32_array_t<Vector2f>;
        using UInt32 = value_t<Vector2u>;
        using Mask = mask_t<Value>;

        const Level &level0 = m_levels[0];

        // Look up parameter-related indices and weights (if Dimension != 0)
        Value param_weight[2 * ArraySize];
        UInt32 slice_offset = zero<UInt32>();
        for (size_t dim = 0; dim < Dimension; ++dim) {
            if (unlikely(m_param_size[dim] == 1)) {
                param_weight[2 * dim] = 1.f;
                param_weight[2 * dim + 1] = 0.f;
                continue;
            }

            UInt32 param_index = math::find_interval(
                m_param_size[dim],
                [&](UInt32 idx, Mask active_) {
                    return gather<Value>(m_param_values[dim].data(), idx,
                                         active_) <= param[dim];
                },
                active);

            Value p0 = gather<Value>(m_param_values[dim].data(), param_index, active),
                  p1 = gather<Value>(m_param_values[dim].data(), param_index + 1, active);

            param_weight[2 * dim + 1] =
                clamp((param[dim] - p0) / (p1 - p0), 0.f, 1.f);
            param_weight[2 * dim] = 1.f - param_weight[2 * dim + 1];
            slice_offset += m_param_strides[dim] * param_index;
        }

        // Fetch values at corners of bilinear patch
        sample *= m_inv_patch_size;
        Vector2u offset = min(Vector2u(Vector2i(sample)), m_max_patch_index);
        UInt32 offset_i = offset.x() + offset.y() * level0.width;
        if (Dimension != 0)
            offset_i += slice_offset * level0.size;

        Value v00 = level0.template lookup<Dimension>(
            offset_i, m_param_strides, param_weight, active);

        Value v10 = level0.template lookup<Dimension>(
            offset_i + 1, m_param_strides, param_weight, active);

        Value v01 = level0.template lookup<Dimension>(
            offset_i + level0.width, m_param_strides, param_weight, active);

        Value v11 = level0.template lookup<Dimension>(
            offset_i + level0.width + 1, m_param_strides, param_weight, active);

        sample -= Vector2f(Vector2i(offset));

        Vector2f w1 = sample, w0 = 1.f - w1;

        Value c0   = fmadd(w0.y(), v00, w1.y() * v01),
              c1   = fmadd(w0.y(), v10, w1.y() * v11),
              pdf  = fmadd(w0.x(), c0, w1.x() * c1),
              r0   = v00 + v10,
              r1   = v01 + v11;

        masked(sample.x(), abs(c1 - c0) > 1e-4f * (c0 + c1)) *=
            (2.f * c0 + sample.x() * (c1 - c0)) / (c0 + c1);

        masked(sample.y(), abs(r1 - r0) > 1e-4f * (r0 + r1)) *=
            (2.f * r0 + sample.y() * (r1 - r0)) / (r0 + r1);

        // Hierarchical sample warping -- reverse direction
        for (int l = 1; l < (int) m_levels.size() - 1; ++l) {
            const Level &level = m_levels[l];

            // Fetch values from next MIP level
            offset_i = level.index(offset & ~1u);
            if (Dimension != 0)
                offset_i += slice_offset * level.size;

            v00 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);
            offset_i += 1u;

            v10 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);
            offset_i += 1u;

            v01 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);
            offset_i += 1u;

            v11 = level.template lookup<Dimension>(
                offset_i, m_param_strides, param_weight, active);

            Mask x_mask = neq(offset.x() & 1u, 0u),
                 y_mask = neq(offset.y() & 1u, 0u);

            r0 = v00 + v10;
            r1 = v01 + v11;
            c0 = select(y_mask, v01, v00);
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
    template <typename Vector2f, typename Value = value_t<Vector2f>>
    Value eval(Vector2f pos, const Value *param = nullptr,
               mask_t<Value> active = true) const {
        using Vector2u = uint32_array_t<Vector2f>;
        using Vector2i = int32_array_t<Vector2f>;
        using UInt32 = value_t<Vector2u>;
        using Mask = mask_t<Value>;

        // Look up parameter-related indices and weights (if Dimension != 0)
        Value param_weight[2 * ArraySize];
        UInt32 slice_offset = zero<UInt32>();
        for (size_t dim = 0; dim < Dimension; ++dim) {
            if (unlikely(m_param_size[dim] == 1)) {
                param_weight[2 * dim] = 1.f;
                param_weight[2 * dim + 1] = 0.f;
                continue;
            }

            UInt32 param_index = math::find_interval(
                m_param_size[dim],
                [&](UInt32 idx, Mask active_) {
                    return gather<Value>(m_param_values[dim].data(), idx,
                                         active_) <= param[dim];
                },
                active);

            Value p0 = gather<Value>(m_param_values[dim].data(), param_index, active),
                  p1 = gather<Value>(m_param_values[dim].data(), param_index + 1, active);

            param_weight[2 * dim + 1] =
                clamp((param[dim] - p0) / (p1 - p0), 0.f, 1.f);
            param_weight[2 * dim] = 1.f - param_weight[2 * dim + 1];
            slice_offset += m_param_strides[dim] * param_index;
        }

        // Compute linear interpolation weights
        pos *= m_inv_patch_size;
        Vector2u offset = min(Vector2u(pos), m_max_patch_index);
        Vector2f w1 = pos - Vector2f(Vector2i(offset)),
                 w0 = 1.f - w1;

        const Level &level0 = m_levels[0];
        UInt32 offset_i = offset.x() + offset.y() * level0.width;
        if (Dimension != 0)
            offset_i += slice_offset * level0.size;

        Value v00 = level0.template lookup<Dimension>(
            offset_i, m_param_strides, param_weight, active);

        Value v10 = level0.template lookup<Dimension>(
            offset_i + 1, m_param_strides, param_weight, active);

        Value v01 = level0.template lookup<Dimension>(
            offset_i + level0.width, m_param_strides, param_weight, active);

        Value v11 = level0.template lookup<Dimension>(
            offset_i + level0.width + 1, m_param_strides, param_weight, active);

        return fmadd(w0.y(),  fmadd(w0.x(), v00, w1.x() * v10),
                     w1.y() * fmadd(w0.x(), v01, w1.x() * v11));
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "Hierarchical2D<" << Dimension << ">[" << std::endl
            << "  size = [" << m_levels[0].width << ", "
            << m_levels[0].size / m_levels[0].width << "]," << std::endl
            << "  levels = " << m_levels.size() << "," << std::endl;
        size_t n_slices = 1, size = 0;
        if (Dimension > 0) {
            oss << "  param_size = [";
            for (size_t i = 0; i<Dimension; ++i) {
                if (i != 0)
                    oss << ", ";
                oss << m_param_size[i];
                n_slices *= m_param_size[i];
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
        oss << "  storage = { " << n_slices << " slice" << (n_slices > 1 ? "s" : "")
            << ", ";
        for (size_t i = 0; i < m_levels.size(); ++i)
            size += m_levels[i].size * n_slices;
        oss << util::mem_string(size * sizeof(ScalarFloat)) << " }" << std::endl
            << "]";
        return oss.str();
    }

private:
    struct Level {
        uint32_t size;
        uint32_t width;
        FloatStorage data;

        Level() { }
        Level(ScalarVector2u res, uint32_t slices) : size(hprod(res)), width(res.x()) {
            uint32_t alloc_size = size  * slices;
            data.resize(alloc_size);
            memset(data.data(), 0, alloc_size * sizeof(ScalarFloat));
        }

        /**
         * \brief Convert from 2D pixel coordinates to an index indicating how the
         * data is laid out in memory.
         *
         * The implementation stores 2x2 patches contigously in memory to
         * improve cache locality during hierarchical traversals
         */
        template <typename Vector2i>
        MTS_INLINE value_t<Vector2i> index(const Vector2i &p) const {
            return ((p.x() & 1u) | sl<1>((p.x() & ~1u) | (p.y() & 1u))) +
                   ((p.y() & ~1u) * width);
        }

        MTS_INLINE ScalarFloat *ptr(const ScalarVector2i &p) {
            return data.data() + index(p);
        }
        MTS_INLINE const ScalarFloat *ptr(const ScalarVector2i &p) const {
            return data.data() + index(p);
        }

        template <size_t Dim, typename Index, typename Value,
                  std::enable_if_t<Dim != 0, int> = 0>
        MTS_INLINE Value lookup(Index i0,
                                const uint32_t *param_strides,
                                const Value *param_weight,
                                mask_t<Value> active) const {
            Index i1 = i0 + param_strides[Dim - 1] * size;

            Value w0 = param_weight[2 * Dim - 2],
                  w1 = param_weight[2 * Dim - 1],
                  v0 = lookup<Dim - 1>(i0, param_strides, param_weight, active),
                  v1 = lookup<Dim - 1>(i1, param_strides, param_weight, active);

            return fmadd(v0, w0, v1 * w1);
        }

        template <size_t Dim, typename Index, typename Value,
                  std::enable_if_t<Dim == 0, int> = 0>
        MTS_INLINE Value lookup(Index index, const uint32_t *,
                                const Value *, mask_t<Value> active) const {
            return gather<Value>(data.data(), index, active);
        }
    };

    /// MIP hierarchy over linearly interpolated patches
    std::vector<Level> m_levels;

    /// Size of a bilinear patch in the unit square
    ScalarVector2f m_patch_size;

    /// Inverse of the above
    ScalarVector2f m_inv_patch_size;

    /// Resolution of the fine-resolution PDF data
    ScalarVector2f m_vertex_count;

    /// Number of bilinear patches in the X/Y dimension - 1
    ScalarVector2u m_max_patch_index;

    /// Resolution of each parameter (optional)
    uint32_t m_param_size[ArraySize];

    /// Stride per parameter in units of sizeof(ScalarFloat)
    uint32_t m_param_strides[ArraySize];

    /// Discretization of each parameter domain
    FloatStorage m_param_values[ArraySize];
};

#if 0
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
 * class named Marginal2D0, Marginal2D1, and Marginal2D2 for data that depends
 * on 0, 1, and 2 parameters, respectively.
 */
template <typename Float, size_t Dimension = 0> class Marginal2D {
private:
    MTS_IMPORT_CORE_TYPES()
    using FloatStorage = host_vector<Float>;

#if !defined(_MSC_VER)
    static constexpr size_t ArraySize = Dimension;
#else
    static constexpr size_t ArraySize = (Dimension != 0) ? Dimension : 1;
#endif

public:
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
    Marginal2D(const ScalarVector2u &size, const ScalarFloat *data,
               std::array<uint32_t, Dimension> param_res = { },
               std::array<const ScalarFloat *, Dimension> param_values = { },
               bool normalize = true, bool build_cdf = true)
        : m_size(size), m_patch_size(1.f / (m_size - 1u)),
          m_inv_patch_size(size - 1u) {

        if (any(size < 2))
            Throw("Marginal2D(): input array resolution must be >= 2!");

        if (build_cdf && !normalize)
            Throw("Marginal2D: build_cdf implies normalize=true");

        // Keep track of the dependence on additional parameters (optional)
        uint32_t slices = 1;
        for (int i = (int) Dimension - 1; i >= 0; --i) {
            if (param_res[i] < 1)
                Throw("Marginal2D(): parameter resolution must be >= 1!");

            m_param_size[i] = param_res[i];
            m_param_values[i].resize(param_res[i]);
            memcpy(m_param_values[i].data(), param_values[i],
                   sizeof(ScalarFloat) * param_res[i]);
            m_param_strides[i] = param_res[i] > 1 ? slices : 0;
            slices *= m_param_size[i];
        }

        uint32_t n_values = hprod(size);

        m_data.resize(slices * n_values);

        if (build_cdf) {
            m_marginal_cdf.resize(slices * m_size.y());
            m_conditional_cdf.resize(slices * n_values);

            ScalarFloat *marginal_cdf = m_marginal_cdf.data(),
                  *conditional_cdf = m_conditional_cdf.data(),
                  *data_out = m_data.data();

            for (uint32_t slice = 0; slice < slices; ++slice) {
                // Construct conditional CDF
                for (uint32_t y = 0; y < m_size.y(); ++y) {
                    double sum = 0.0;
                    size_t i = y * size.x();
                    conditional_cdf[i] = 0.f;
                    for (uint32_t x = 0; x < m_size.x() - 1; ++x, ++i) {
                        sum += .5 * ((double) data[i] + (double) data[i + 1]);
                        conditional_cdf[i + 1] = (ScalarFloat) sum;
                    }
                }

                // Construct marginal CDF
                marginal_cdf[0] = 0.f;
                double sum = 0.0;
                for (uint32_t y = 0; y < m_size.y() - 1; ++y) {
                    sum += .5 * ((double) conditional_cdf[(y + 1) * size.x() - 1] +
                                 (double) conditional_cdf[(y + 2) * size.x() - 1]);
                    marginal_cdf[y + 1] = (ScalarFloat) sum;
                }

                // Normalize CDFs and PDF (if requested)
                ScalarFloat normalization = 1.f / marginal_cdf[m_size.y() - 1];
                for (size_t i = 0; i < n_values; ++i)
                    conditional_cdf[i] *= normalization;
                for (size_t i = 0; i < m_size.y(); ++i)
                    marginal_cdf[i] *= normalization;
                for (size_t i = 0; i < n_values; ++i)
                    data_out[i] = data[i] * normalization;

                marginal_cdf += m_size.y();
                conditional_cdf += n_values;
                data_out += n_values;
                data += n_values;
            }
        } else {
            ScalarFloat *data_out = m_data.data();

            for (uint32_t slice = 0; slice < slices; ++slice) {
                ScalarFloat normalization = 1.f / hprod(m_inv_patch_size);
                if (normalize) {
                    double sum = 0.0;
                    for (uint32_t y = 0; y < m_size.y() - 1; ++y) {
                        size_t i = y * size.x();
                        for (uint32_t x = 0; x < m_size.x() - 1; ++x, ++i) {
                            ScalarFloat v00 = data[i],
                                  v10 = data[i + 1],
                                  v01 = data[i + size.x()],
                                  v11 = data[i + 1 + size.x()],
                                  avg = .25f * (v00 + v10 + v01 + v11);
                            sum += (double) avg;
                        }
                    }
                    normalization = ScalarFloat(1.0 / sum);
                }

                for (uint32_t k = 0; k < n_values; ++k)
                    data_out[k] = data[k] * normalization;

                data += n_values;
                data_out += n_values;
            }
        }
    }


    /**
     * \brief Given a uniformly distributed 2D sample, draw a sample from the
     * distribution (parameterized by \c param if applicable)
     *
     * Returns the warped sample and associated probability density.
     */
    template <typename Vector2f, typename Value = value_t<Vector2f>>
    std::pair<Vector2f, Value> sample(Vector2f sample, const Value *param = nullptr,
                                      mask_t<Value> active = true) const {
        using Vector2u = uint32_array_t<Vector2f>;
        using UInt32 = value_t<Vector2u>;
        using Mask = mask_t<Value>;

        // Avoid degeneracies at the extrema
        sample = clamp(sample, math::Epsilon<Value>, math::OneMinusEpsilon<Value>);

        // Look up parameter-related indices and weights (if Dimension != 0)
        Value param_weight[2 * ArraySize];
        UInt32 slice_offset = zero<UInt32>();
        for (size_t dim = 0; dim < Dimension; ++dim) {
            if (unlikely(m_param_size[dim] == 1)) {
                param_weight[2 * dim] = 1.f;
                param_weight[2 * dim + 1] = 0.f;
                continue;
            }

            UInt32 param_index = math::find_interval(
                m_param_size[dim],
                [&](UInt32 idx, Mask active_) {
                    return gather<Value>(m_param_values[dim].data(), idx,
                                         active_) <= param[dim];
                },
                active);

            Value p0 = gather<Value>(m_param_values[dim].data(), param_index, active),
                  p1 = gather<Value>(m_param_values[dim].data(), param_index + 1, active);

            param_weight[2 * dim + 1] =
                clamp((param[dim] - p0) / (p1 - p0), 0.f, 1.f);
            param_weight[2 * dim] = 1.f - param_weight[2 * dim + 1];
            slice_offset += m_param_strides[dim] * param_index;
        }

        // Sample the row first
        UInt32 offset = 0;
        if (Dimension != 0)
            offset = slice_offset * m_size.y();

        auto fetch_marginal = [&](UInt32 idx, Mask active_) ENOKI_INLINE_LAMBDA -> Value {
            return lookup<Dimension>(m_marginal_cdf.data(), offset + idx,
                                     m_size.y(), param_weight, active_);
        };

        UInt32 row = math::find_interval(
            m_size.y(),
            [&](UInt32 idx, Mask active_) ENOKI_INLINE_LAMBDA -> Mask {
                return fetch_marginal(idx, active_) < sample.y();
            },
            active);

        sample.y() -= fetch_marginal(row, active);

        uint32_t slice_size = hprod(m_size);
        offset = row * m_size.x();
        if (Dimension != 0)
            offset += slice_offset * slice_size;

        Value r0 = lookup<Dimension>(m_conditional_cdf.data(),
                                     offset + m_size.x() - 1, slice_size,
                                     param_weight, active),
              r1 = lookup<Dimension>(m_conditional_cdf.data(),
                                     offset + (m_size.x() * 2 - 1), slice_size,
                                     param_weight, active);

        Mask is_const = abs(r0 - r1) < 1e-4f * (r0 + r1);
        sample.y() = select(is_const, 2.f * sample.y(),
            r0 - safe_sqrt(r0 * r0 - 2.f * sample.y() * (r0 - r1)));
        sample.y() /= select(is_const, r0 + r1, r0 - r1);

        // Sample the column next
        sample.x() *= (1.f - sample.y()) * r0 + sample.y() * r1;

        auto fetch_conditional = [&](UInt32 idx, Mask active_) ENOKI_INLINE_LAMBDA -> Value {
            Value v0 = lookup<Dimension>(m_conditional_cdf.data(), offset + idx,
                                         slice_size, param_weight, active_),
                  v1 = lookup<Dimension>(m_conditional_cdf.data() + m_size.x(),
                                         offset + idx, slice_size, param_weight, active_);

            return (1.f - sample.y()) * v0 + sample.y() * v1;
        };

        UInt32 col = math::find_interval(
            m_size.x(),
            [&](UInt32 idx, Mask active_) ENOKI_INLINE_LAMBDA -> Mask {
                return fetch_conditional(idx, active_) < sample.x();
            },
            active);

        sample.x() -= fetch_conditional(col, active);

        offset += col;

        Value v00 = lookup<Dimension>(m_data.data(), offset, slice_size,
                                      param_weight, active),
              v10 = lookup<Dimension>(m_data.data() + 1, offset, slice_size,
                                      param_weight, active),
              v01 = lookup<Dimension>(m_data.data() + m_size.x(), offset,
                                      slice_size, param_weight, active),
              v11 = lookup<Dimension>(m_data.data() + m_size.x() + 1, offset,
                                      slice_size, param_weight, active),
              c0  = fmadd((1.f - sample.y()), v00, sample.y() * v01),
              c1  = fmadd((1.f - sample.y()), v10, sample.y() * v11);

        is_const = abs(c0 - c1) < 1e-4f * (c0 + c1);
        sample.x() = select(is_const, 2.f * sample.x(),
            c0 - safe_sqrt(c0 * c0 - 2.f * sample.x() * (c0 - c1)));
        Value divisor = select(is_const, c0 + c1, c0 - c1);
        masked(sample.x(), neq(divisor, 0.f)) /= divisor;

        return {
            (Vector2u(col, row) + sample) * m_patch_size,
            ((1.f - sample.x()) * c0 + sample.x() * c1) * hprod(m_inv_patch_size)
        };
    }

    /// Inverse of the mapping implemented in \c sample()
    template <typename Vector2f, typename Value = value_t<Vector2f>>
    std::pair<Vector2f, Value> invert(Vector2f sample, const Value *param = nullptr,
                                      mask_t<Value> active = true) const {
        using Vector2u = uint32_array_t<Vector2f>;
        using Vector2i = int32_array_t<Vector2f>;
        using UInt32 = value_t<Vector2u>;
        using Mask = mask_t<Value>;

        // Look up parameter-related indices and weights (if Dimension != 0)
        Value param_weight[2 * ArraySize];
        UInt32 slice_offset = zero<UInt32>();
        for (size_t dim = 0; dim < Dimension; ++dim) {
            if (unlikely(m_param_size[dim] == 1)) {
                param_weight[2 * dim] = 1.f;
                param_weight[2 * dim + 1] = 0.f;
                continue;
            }

            UInt32 param_index = math::find_interval(
                m_param_size[dim],
                [&](UInt32 idx, Mask active_) {
                    return gather<Value>(m_param_values[dim].data(), idx,
                                         active_) <= param[dim];
                },
                active);

            Value p0 = gather<Value>(m_param_values[dim].data(), param_index, active),
                  p1 = gather<Value>(m_param_values[dim].data(), param_index + 1, active);

            param_weight[2 * dim + 1] =
                clamp((param[dim] - p0) / (p1 - p0), 0.f, 1.f);
            param_weight[2 * dim] = 1.f - param_weight[2 * dim + 1];
            slice_offset += m_param_strides[dim] * param_index;
        }

        // Fetch values at corners of bilinear patch
        sample *= m_inv_patch_size;
        Vector2u pos = min(Vector2u(sample), m_size - 2u);
        sample -= Vector2f(Vector2i(pos));

        UInt32 offset = pos.x() + pos.y() * m_size.x();
        uint32_t slice_size = hprod(m_size);
        if (Dimension != 0)
            offset += slice_offset * slice_size;

        // Invert the X component
        Value v00 = lookup<Dimension>(m_data.data(), offset, slice_size,
                                      param_weight, active),
              v10 = lookup<Dimension>(m_data.data() + 1, offset, slice_size,
                                      param_weight, active),
              v01 = lookup<Dimension>(m_data.data() + m_size.x(), offset, slice_size,
                                      param_weight, active),
              v11 = lookup<Dimension>(m_data.data() + m_size.x() + 1, offset, slice_size,
                                      param_weight, active);

        Vector2f w1 = sample, w0 = 1.f - w1;

        Value c0  = fmadd(w0.y(), v00, w1.y() * v01),
              c1  = fmadd(w0.y(), v10, w1.y() * v11),
              pdf = fmadd(w0.x(), c0, w1.x() * c1);

        sample.x() *= c0 + .5f * sample.x() * (c1 - c0);

        Value v0 = lookup<Dimension>(m_conditional_cdf.data(), offset,
                                     slice_size, param_weight, active),
              v1 = lookup<Dimension>(m_conditional_cdf.data() + m_size.x(),
                                     offset, slice_size, param_weight, active);

        sample.x() += (1.f - sample.y()) * v0 + sample.y() * v1;

        offset = pos.y() * m_size.x();
        if (Dimension != 0)
            offset += slice_offset * slice_size;

        Value r0 = lookup<Dimension>(m_conditional_cdf.data(),
                                     offset + m_size.x() - 1, slice_size,
                                     param_weight, active),
              r1 = lookup<Dimension>(m_conditional_cdf.data(),
                                     offset + (m_size.x() * 2 - 1), slice_size,
                                     param_weight, active);

        sample.x() /= (1.f - sample.y()) * r0 + sample.y() * r1;

        // Invert the Y component
        sample.y() *= r0 + .5f * sample.y() * (r1 - r0);

        offset = pos.y();
        if (Dimension != 0)
            offset += slice_offset * m_size.y();

        sample.y() += lookup<Dimension>(m_marginal_cdf.data(), offset,
                                        m_size.y(), param_weight, active);

        return { sample, pdf * hprod(m_inv_patch_size) };
    }

    /**
     * \brief Evaluate the density at position \c pos. The distribution is
     * parameterized by \c param if applicable.
     */
    template <typename Vector2f, typename Value = value_t<Vector2f>>
    Value eval(Vector2f pos, const Value *param = nullptr,
               mask_t<Value> active = true) const {
        using Vector2u = uint32_array_t<Vector2f>;
        using Vector2i = int32_array_t<Vector2f>;
        using UInt32 = value_t<Vector2u>;
        using Mask = mask_t<Value>;

        // Look up parameter-related indices and weights (if Dimension != 0)
        Value param_weight[2 * ArraySize];
        UInt32 slice_offset = zero<UInt32>();
        for (size_t dim = 0; dim < Dimension; ++dim) {
            if (unlikely(m_param_size[dim] == 1)) {
                param_weight[2 * dim] = 1.f;
                param_weight[2 * dim + 1] = 0.f;
                continue;
            }

            UInt32 param_index = math::find_interval(
                m_param_size[dim],
                [&](UInt32 idx, Mask active_) {
                    return gather<Value>(m_param_values[dim].data(), idx,
                                         active_) <= param[dim];
                },
                active);

            Value p0 = gather<Value>(m_param_values[dim].data(), param_index, active),
                  p1 = gather<Value>(m_param_values[dim].data(), param_index + 1, active);

            param_weight[2 * dim + 1] =
                clamp((param[dim] - p0) / (p1 - p0), 0.f, 1.f);
            param_weight[2 * dim] = 1.f - param_weight[2 * dim + 1];
            slice_offset += m_param_strides[dim] * param_index;
        }

        // Compute linear interpolation weights
        pos *= m_inv_patch_size;
        Vector2u offset = min(Vector2u(pos), m_size - 2u);
        Vector2f w1 = pos - Vector2f(Vector2i(offset)),
                 w0 = 1.f - w1;

        UInt32 index = offset.x() + offset.y() * m_size.x();

        uint32_t size = hprod(m_size);
        if (Dimension != 0)
            index += slice_offset * size;

        Value v00 = lookup<Dimension>(m_data.data(), index, size,
                                      param_weight, active),
              v10 = lookup<Dimension>(m_data.data() + 1, index, size,
                                      param_weight, active),
              v01 = lookup<Dimension>(m_data.data() + m_size.x(), index, size,
                                      param_weight, active),
              v11 = lookup<Dimension>(m_data.data() + m_size.x() + 1, index, size,
                                      param_weight, active);

        return fmadd(w0.y(),  fmadd(w0.x(), v00, w1.x() * v10),
                     w1.y() * fmadd(w0.x(), v01, w1.x() * v11)) * hprod(m_inv_patch_size);
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "Marginal2D<" << Dimension << ">[" << std::endl
            << "  size = " << m_size << "," << std::endl;
        size_t n_slices = 1;
        if (Dimension > 0) {
            oss << "  param_size = [";
            for (size_t i = 0; i<Dimension; ++i) {
                if (i != 0)
                    oss << ", ";
                oss << m_param_size[i];
                n_slices *= m_param_size[i];
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
        oss << "  storage = { " << n_slices << " slice" << (n_slices > 1 ? "s" : "")
            << ", ";
        size_t size = n_slices * (hprod(m_size) * 2 + m_size.y());
        oss << util::mem_string(size * sizeof(ScalarFloat)) << " }" << std::endl
            << "]";
        return oss.str();
    }

private:
        template <size_t Dim, typename Index, typename Value,
                  std::enable_if_t<Dim != 0, int> = 0>
        MTS_INLINE Value lookup(const ScalarFloat *data, Index i0,
                                uint32_t size,
                                const Value *param_weight,
                                mask_t<Value> active) const {
            Index i1 = i0 + m_param_strides[Dim - 1] * size;

            Value w0 = param_weight[2 * Dim - 2],
                  w1 = param_weight[2 * Dim - 1],
                  v0 = lookup<Dim - 1>(data, i0, size, param_weight, active),
                  v1 = lookup<Dim - 1>(data, i1, size, param_weight, active);

            return fmadd(v0, w0, v1 * w1);
        }

        template <size_t Dim, typename Index, typename Value,
                  std::enable_if_t<Dim == 0, int> = 0>
        MTS_INLINE Value lookup(const ScalarFloat *data, Index index, uint32_t,
                                const Value *, mask_t<Value> active) const {
            return gather<Value>(data, index, active);
        }

private:
    /// Resolution of the discretized density function
    ScalarVector2u m_size;

    /// Size of a bilinear patch in the unit square
    ScalarVector2f m_patch_size, m_inv_patch_size;

    /// Resolution of each parameter (optional)
    uint32_t m_param_size[ArraySize];

    /// Stride per parameter in units of sizeof(ScalarFloat)
    uint32_t m_param_strides[ArraySize];

    /// Discretization of each parameter domain
    FloatStorage m_param_values[ArraySize];

    /// Density values
    FloatStorage m_data;

    /// Marginal and conditional PDFs
    FloatStorage m_marginal_cdf;
    FloatStorage m_conditional_cdf;
};
#endif

//! @}
// =======================================================================

NAMESPACE_END(mitsuba)

