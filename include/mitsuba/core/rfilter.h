#pragma once

#include <limits>
#include <mitsuba/core/object.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/logger.h>

NAMESPACE_BEGIN(mitsuba)

/// Reconstruction filters will be tabulated at this resolution
#define MI_FILTER_RESOLUTION 31

/**
 * \brief When resampling data to a different resolution using \ref
 * Resampler::resample(), this enumeration specifies how lookups
 * <em>outside</em> of the input domain are handled.
 *
 * \see Resampler
 */
enum class FilterBoundaryCondition {
    /// Clamp to the outermost sample position (default)
    Clamp = 0,

    /// Assume that the input repeats in a periodic fashion
    Repeat,

    /// Assume that the input is mirrored along the boundary
    Mirror,

    /// Assume that the input function is zero outside of the defined domain
    Zero,

    /// Assume that the input function is equal to one outside of the defined domain
    One
};

/**
 * \brief Generic interface to separable image reconstruction filters
 *
 * When resampling bitmaps or adding samples to a rendering in progress,
 * Mitsuba first convolves them with a image reconstruction filter. Various
 * kinds are implemented as subclasses of this interface.
 *
 * Because image filters are generally too expensive to evaluate for each
 * sample, the implementation of this class internally precomputes an discrete
 * representation, whose resolution given by \ref MI_FILTER_RESOLUTION.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB ReconstructionFilter : public Object {
public:
    MI_IMPORT_CORE_TYPES()

    /// Return the filter's width
    ScalarFloat radius() const { return m_radius; }

    /// Return the block border size required when rendering with this filter
    uint32_t border_size() const { return m_border_size; }

    /// Evaluate the filter function
    virtual Float eval(Float x, Mask active = true) const = 0;

    /// Check whether this is a box filter?
    bool is_box_filter() const;

    /// Evaluate a discretized version of the filter (generally faster than 'eval')
    MI_INLINE Float eval_discretized(Float x, Mask active = true) const {
        if constexpr (!dr::is_jit_v<Float>) {
            UInt32 index = dr::minimum(UInt32(dr::abs(x * m_scale_factor)),
                                   MI_FILTER_RESOLUTION);
            return dr::gather<Float>(m_values.data(), index, active);
        } else {
            Throw("ReconstructionFilter::eval_discretized(): not supported in "
                  "JIT modes, use the regular eval() function!");
        }
    }

    MI_DECLARE_CLASS()
protected:
    /// Create a new reconstruction filter
    ReconstructionFilter(const Properties &props);

    /// Virtual destructor
    virtual ~ReconstructionFilter();

    /// Mandatory initialization prior to calls to \ref eval_discretized()
    void init_discretization();

protected:
    ScalarFloat m_radius, m_scale_factor;
    std::vector<ScalarFloat> m_values;
    uint32_t m_border_size;
};

/**
 * \brief Utility class for efficiently resampling discrete datasets to different resolutions
 * \tparam Scalar
 *      Denotes the underlying floating point data type (i.e. <tt>half</tt>, <tt>float</tt>,
 *      or <tt>double</tt>)
 */
template <typename Scalar_> struct Resampler {
    using Scalar = Scalar_;
    using Float = float;
    using ReconstructionFilter = mitsuba::ReconstructionFilter<Float, Color<Float, 3>>;

    /**
     * \brief Create a new Resampler object that transforms between the specified resolutions
     *
     * This constructor precomputes all information needed to efficiently perform the
     * desired resampling operation. For that reason, it is most efficient if it can
     * be used over and over again (e.g. to resample the equal-sized rows of a bitmap)
     *
     * \param source_res
     *      Source resolution
     * \param target_res
     *      Desired target resolution
     */
    Resampler(const ReconstructionFilter *rfilter,
              uint32_t source_res, uint32_t target_res)
        : m_source_res(source_res), m_target_res(target_res) {
        if (source_res == 0 || target_res == 0)
            Throw("Resampler::Resampler(): source or target resolution == 0!");

        Float filter_radius_orig = rfilter->radius(),
              filter_radius = filter_radius_orig,
              scale = 1, inv_scale = 1;

        /* Low-pass filter: scale reconstruction filters when downsampling */
        if (target_res < source_res) {
            scale = (Float) source_res / (Float) target_res;
            inv_scale = dr::rcp(scale);
            filter_radius *= scale;
        }

        m_taps = dr::ceil2int<uint32_t>(filter_radius * 2);
        if (source_res == target_res && (m_taps % 2) != 1)
            --m_taps;

        if (filter_radius_orig < 1)
            m_taps = std::min(m_taps, source_res);

        if (source_res != target_res) { /* Resampling mode */
            m_start = std::unique_ptr<int32_t[]>(new int32_t[target_res]);
            m_weights = std::unique_ptr<Scalar[]>(new Scalar[m_taps * target_res]);
            m_fast_start = 0;
            m_fast_end = m_target_res;

            for (uint32_t i = 0; i < target_res; i++) {
                /* Compute the fractional coordinates of the new sample i
                   in the original coordinates */
                Float center = (i + Float(0.5)) / target_res * source_res;

                /* Determine the index of the first original sample
                   that might contribute */
                m_start[i] = dr::floor2int<int32_t>(center - filter_radius + Float(0.5));

                /* Determine the size of center region, on which to run
                   the fast non condition-aware code */
                if (m_start[i] < 0)
                    m_fast_start = std::max(m_fast_start, i + 1);
                else if (m_start[i] + m_taps - 1 >= m_source_res)
                    m_fast_end = std::min(m_fast_end, i);

                double sum = 0.0;
                for (uint32_t j = 0; j < m_taps; j++) {
                    /* Compute the position where the filter should be evaluated */
                    Float pos = m_start[i] + (int32_t) j + Float(0.5) - center;

                    /* Perform the evaluation and record the weight */
                    auto weight = rfilter->eval(pos * inv_scale);
                    m_weights[i * m_taps + j] = static_cast<Scalar>(weight);
                    sum += double(weight);
                }

                Assert(sum != 0, "Resampler(): filter footprint is too small; the "
                                 "support of some output samples does not contain "
                                 "any input samples!");

                /* Normalize the contribution of each sample */
                double normalization = 1.0 / sum;
                for (uint32_t j = 0; j < m_taps; j++) {
                    Scalar &value = m_weights[i * m_taps + j];
                    value = Scalar(double(value) * normalization);
                }
            }
        } else { /* Filtering mode */
            uint32_t half_taps = m_taps / 2;
            m_weights = std::unique_ptr<Scalar[]>(new Scalar[m_taps]);

            double sum = 0.0;
            for (uint32_t i = 0; i < m_taps; i++) {
                auto weight = rfilter->eval(Float((int32_t) i - (int32_t) half_taps));
                m_weights[i] = Scalar(weight);
                sum += double(weight);
            }

            Assert(sum != 0, "Resampler(): filter footprint is too small; the "
                             "support of some output samples does not contain "
                             "any input samples!");

            double normalization = 1.0 / sum;
            for (uint32_t i = 0; i < m_taps; i++) {
                Scalar &value = m_weights[i];
                value = Scalar(double(value) * normalization);
            }
            m_fast_start = std::min(half_taps, m_target_res - 1);
            m_fast_end   = (uint32_t) std::max(
                (dr::ssize_t) m_target_res - (dr::ssize_t) half_taps - 1, (dr::ssize_t) 0);
        }

        /* Avoid overlapping fast start/end intervals when the
           target image is very small compared to the source image */
        m_fast_start = std::min(m_fast_start, m_fast_end);
    }

    /// Return the reconstruction filter's source resolution
    uint32_t source_resolution() const { return m_source_res; }

    /// Return the reconstruction filter's target resolution
    uint32_t target_resolution() const { return m_target_res; }

    /// Return the number of taps used by the reconstruction filter
    uint32_t taps() const { return m_taps; }

    /**
     * \brief Set the boundary condition that should be used when
     * looking up samples outside of the defined input domain
     *
     * The default is \ref FilterBoundaryCondition::Clamp
     */
    void set_boundary_condition(FilterBoundaryCondition bc) { m_bc = bc; }

    /**
     * \brief Return the boundary condition that should be used when
     * looking up samples outside of the defined input domain
     */
    FilterBoundaryCondition boundary_condition() const { return m_bc; }

    /**
     * \brief Returns the range to which resampled values will be clamped
     *
     * The default is -infinity to infinity (i.e. no clamping is used)
     */
    const std::pair<Scalar, Scalar> &clamp() { return m_clamp; }

    /// If specified, resampled values will be clamped to the given range
    void set_clamp(const std::pair<Scalar, Scalar> &value) { m_clamp = value; }

    /**
     * \brief Resample a multi-channel array and clamp the results
     * to a specified valid range
     *
     * \param source
     *     Source array of samples
     * \param target
     *     Target array of samples
     * \param source_stride
     *     Stride of samples in the source array. A value
     *     of '1' implies that they are densely packed.
     * \param target_stride
     *     Stride of samples in the source array. A value
     *     of '1' implies that they are densely packed.
     * \param channels
     *     Number of channels to be resampled
     */
    void resample(const Scalar *source, uint32_t source_stride,
                  Scalar *target, uint32_t target_stride, uint32_t channels) const {

        using ResampleFunctor = void (Resampler::*)(
            const Scalar *, uint32_t, Scalar *, uint32_t, uint32_t) const;

        ResampleFunctor f;

        if (m_clamp != std::make_pair(-std::numeric_limits<Scalar>::infinity(),
                                       std::numeric_limits<Scalar>::infinity())) {
            if (m_start)
                f = &Resampler::resample_internal<true /* Clamp */, true /* Resample */>;
            else
                f = &Resampler::resample_internal<true /* Clamp */, false /* Resample */>;
        } else {
            if (m_start)
                f = &Resampler::resample_internal<false /* Clamp */, true /* Resample */>;
            else
                f = &Resampler::resample_internal<false /* Clamp */, false /* Resample */>;
        }

        (this->*f)(source, source_stride, target, target_stride, channels);
    }


    /// Return a human-readable summary
    std::string to_string() const {
        return tfm::format("Resampler[source_res=%i, target_res=%i]",
                           m_source_res, m_target_res);
    }

private:
    template <bool Clamp, bool Resample>
    void resample_internal(const Scalar *source, uint32_t source_stride,
                           Scalar *target, uint32_t target_stride,
                           uint32_t channels) const {
        const uint32_t taps = m_taps, half_taps = m_taps / 2;
        const Scalar *weights = m_weights.get();
        const int32_t *start = m_start.get();
        const Scalar min = std::get<0>(m_clamp);
        const Scalar max = std::get<1>(m_clamp);

        target_stride = channels * (target_stride - 1);
        source_stride *= channels;

        /* Resample the left border region, while accounting for the boundary conditions */
        for (uint32_t i = 0; i < m_fast_start; ++i) {
            const int32_t offset =
                Resample ? (*start++) : ((int32_t) i - half_taps);

            for (uint32_t ch = 0; ch < channels; ++ch) {
                Scalar result = 0;
                for (uint32_t j = 0; j < taps; ++j)
                    result += lookup(source, offset + (int32_t) j,
                                     source_stride, ch) * weights[j];

                *target++ = Clamp ? dr::template clamp<Scalar>(result, min, max) : result;
            }

            target += target_stride;

            if (Resample)
                weights += taps;
        }

        /* Use a faster branch-free loop for resampling the main portion */
        for (uint32_t i = m_fast_start; i < m_fast_end; ++i) {
            const int32_t offset =
                Resample ? (*start++) : ((int32_t) i - half_taps);

            for (uint32_t ch = 0; ch < channels; ++ch) {
                Scalar result = 0;
                for (uint32_t j = 0; j < taps; ++j)
                    result +=
                        source[source_stride * (offset + (int32_t) j) + ch] *
                        weights[j];

                *target++ = Clamp ? dr::template clamp<Scalar>(result, min, max) : result;
            }

            target += target_stride;

            if (Resample)
                weights += taps;
        }

        /* Resample the right border region, while accounting for the boundary conditions */
        for (uint32_t i = m_fast_end; i < m_target_res; ++i) {
            const int32_t offset =
                Resample ? (*start++) : ((int32_t) i - half_taps);

            for (uint32_t ch = 0; ch < channels; ++ch) {
                Scalar result = 0;
                for (uint32_t j = 0; j < taps; ++j)
                    result += lookup(source, offset + (int32_t) j,
                                     source_stride, ch) * weights[j];

                *target++ = Clamp ? dr::template clamp<Scalar>(result, min, max) : result;
            }

            target += target_stride;

            if (Resample)
                weights += taps;
        }
    }

    Scalar lookup(const Scalar *source, int32_t pos, uint32_t stride, uint32_t ch) const {
        if (unlikely(pos < 0 || pos >= (int32_t) m_source_res)) {
            switch (m_bc) {
                case FilterBoundaryCondition::Clamp:
                    pos = dr::clamp(pos, 0, (int32_t) m_source_res - 1);
                    break;

                case FilterBoundaryCondition::Repeat:
                    pos = math::modulo(pos, (int32_t) m_source_res);
                    break;

                case FilterBoundaryCondition::Mirror:
                    pos = math::modulo(pos, 2 * (int32_t) m_source_res - 2);
                    if (pos >= (int32_t) m_source_res - 1)
                        pos = 2 * m_source_res - 2 - pos;
                    break;

                case FilterBoundaryCondition::One:
                    return Scalar(1);

                case FilterBoundaryCondition::Zero:
                    return Scalar(0);
            }
        }

        return source[pos * stride + ch];
    }

private:
    std::unique_ptr<int32_t[]> m_start;
    std::unique_ptr<Scalar[]> m_weights;
    uint32_t m_source_res;
    uint32_t m_target_res;
    uint32_t m_fast_start;
    uint32_t m_fast_end;
    uint32_t m_taps;
    FilterBoundaryCondition m_bc = FilterBoundaryCondition::Clamp;
    std::pair<Scalar, Scalar> m_clamp {
        -std::numeric_limits<Scalar>::infinity(),
         std::numeric_limits<Scalar>::infinity()
    };
};

extern MI_EXPORT_LIB std::ostream &operator<<(std::ostream &os,
                                                const FilterBoundaryCondition &value);

MI_EXTERN_CLASS(ReconstructionFilter)

NAMESPACE_END(mitsuba)
