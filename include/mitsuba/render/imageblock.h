#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/render/fwd.h>
#include <drjit/dynamic.h>
#include <drjit/tensor.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Intermediate storage for an image or image sub-region being rendered
 *
 * This class facilitates parallel rendering of images in both scalar and
 * JIT-based variants of Mitsuba.
 *
 * In scalar mode, image blocks represent independent rectangular image regions
 * that are simultaneously processed by worker threads. They are finally merged
 * into a master \ref ImageBlock controlled by the \ref Film instance via the
 * \ref put_block() method. The smaller image blocks can include a border
 * region storing contributions that are slightly outside of the block, which
 * is required to correctly account for image reconstruction filters.
 *
 * In JIT variants there is only a single \ref ImageBlock, whose contents are
 * computed in parallel. A border region is usually not needed in this case.
 *
 * In addition to receiving samples via the \ref put() method, the image block
 * can also be queried via the \ref read() method, in which case the
 * reconstruction filter is used to compute suitable interpolation weights.
 * This is feature is useful for differentiable rendering, where one needs to
 * evaluate the reverse-mode derivative of the \ref put() method.
 */

template <typename Float, typename Spectrum>
class MI_EXPORT_LIB ImageBlock : public Object {
public:
    MI_IMPORT_TYPES(ReconstructionFilter)

    /**
     * \brief Construct a zero-initialized image block with the desired shape
     * and channel count
     *
     * \param size
     *    Specifies the desired horizontal and vertical block size. This value
     *    excludes additional border pixels that \c ImageBlock will internally
     *    add to support image reconstruction filters (if <tt>border=true</tt>
     *    and a reconstruction filter is furthermore specified)
     *
     * \param offset
     *    Specifies the offset in case this block represents sub-region of a
     *    larger image. Otherwise, this can be set to zero.
     *
     * \param channel_count
     *    Specifies the desired number of image channels.
     *
     * \param rfilter
     *    The desired reconstruction filter to be used in \ref read() and \ref
     *    put(). A box filter will be used if when <tt>rfilter==nullptr</tt>.
     *
     * \param border
     *    Should \c ImageBlock add an additional border region around around
     *    the image boundary to capture contributions to neighboring pixels
     *    caused by a nontrivial (non-box) reconstruction filter? This is
     *    mainly useful when a larger image is partitioned into smaller blocks
     *    that are rendered in parallel. Enabled by default in scalar variants.
     *
     * \param normalize
     *    This parameter affects the behavior of \ref read() and \ref put().
     *
     *    If set to \c true, each call to \ref put() explicitly normalizes the
     *    computed filter weights so that the operation adds a unit amount of
     *    energy to the image. This is useful for methods like particle tracing
     *    that invoke \ref put() with an arbitrary distribution of image
     *    positions. Other methods (e.g., path tracing) that uniformly sample
     *    positions in image space should set this parameter to \c false, since
     *    the image block contents will eventually be divided by a dedicated
     *    channel tracking the accumulated sample weight to remove any
     *    non-uniformity.
     *
     *    Furthermore, if \c normalize is set to \c true, the \ref read()
     *    operation will normalize the filter weights to compute a convex
     *    combination of pixel values.
     *
     *    Disabled by default.
     *
     * \param coalesce
     *   This parameter is only relevant for JIT variants, where it subtly
     *   affects the behavior of the performance-critical \ref put() method.
     *
     *   In coalesced mode, \ref put() conservatively bounds the footprint
     *   and traverses it in lockstep across the whole wavefront. This causes
     *   unnecessary atomic memory operations targeting pixels with a zero
     *   filter weight. At the same time, this greatly reduces thread
     *   divergence and can lead to significant speedups when \ref put()
     *   writes to pixels in a regular (e.g., scanline) order. Coalesced
     *   mode is preferable for rendering algorithms like path tracing that
     *   uniformly generate samples within pixels on the sensor.
     *
     *   In contrast, non-coalesced mode is preferable when the input positions
     *   are random and will in any case be subject to thread divergence (e.g.
     *   in a particle tracer that makes random connections to the sensor).
     *
     * \param compensate
     *   If set to \c true, the implementation internally switches to
     *   Kahan-style error-compensated floating point accumulation. This is
     *   useful when accumulating many samples into a single precision image
     *   block. Note that this is currently only supported in JIT modes, and
     *   that it can make the accumulation quite a bit more expensive. The
     *   default is \c false.
     *
     * \param warn_negative
     *    If set to \c true, \ref put() will warn when writing samples with
     *    negative components. This test is only enabled in scalar variants by
     *    default, since checking/error reporting is relatively costly in
     *    JIT-compiled modes.
     *
     * \param warn_invalid
     *    If set to \c true, \ref put() will warn when writing samples with
     *    NaN (not a number) or positive/negative infinity component values.
     *    This test is only enabled in scalar variants by default, since
     *    checking/error reporting is relatively costly in JIT-compiled modes.
     */
    ImageBlock(const ScalarVector2u &size,
               const ScalarPoint2i &offset,
               uint32_t channel_count,
               const ReconstructionFilter *rfilter = nullptr,
               bool border = std::is_scalar_v<Float>,
               bool normalize = false,
               bool coalesce = dr::is_jit_v<Float>,
               bool compensate = false,
               bool warn_negative = std::is_scalar_v<Float>,
               bool warn_invalid = std::is_scalar_v<Float>);

    /**
     * \brief Construct an image block from an existing image tensor
     *
     * In contrast to the above constructor, this one infers the block size and
     * channel count from a provided 3D image tensor of shape <tt>(height,
     * width, channels)</tt>. It then initializes the image block contents with
     * a copy of the tensor. This is useful for differentiable rendering phases
     * that want to query an existing image using a pixel filter via the \ref
     * read() function.
     *
     * See the other constructor for an explanation of the parameters.
     */
    ImageBlock(const TensorXf &tensor,
               const ScalarPoint2i &offset = ScalarPoint2i(0),
               const ReconstructionFilter *rfilter = nullptr,
               bool border = std::is_scalar_v<Float>,
               bool normalize = false,
               bool coalesce = dr::is_jit_v<Float>,
               bool compensate = false,
               bool warn_negative = std::is_scalar_v<Float>,
               bool warn_invalid = std::is_scalar_v<Float>);

    /// Accumulate another image block into this one
    void put_block(const ImageBlock *block);

    /**
     * \brief Accumulate a single sample or a wavefront of samples into the
     * image block.
     *
     * \remark This variant of the put() function assumes that the ImageBlock
     * has a standard layout, namely: \c RGB, potentially \c alpha, and a \c
     * weight channel. Use the other variant if the channel configuration
     * deviations from this default.
     *
     * \param pos
     *    Denotes the sample position in fractional pixel coordinates
     *
     * \param wavelengths
     *    Sample wavelengths in nanometers
     *
     * \param value
     *    Sample value associated with the specified wavelengths
     *
     * \param alpha
     *    Alpha value associated with the sample
     */
    void put(const Point2f &pos,
             const Wavelength &wavelengths,
             const Spectrum &value,
             Float alpha = 1.f,
             Float weight = 1.f,
             Mask active = true) {
        DRJIT_MARK_USED(wavelengths);

        UnpolarizedSpectrum spec_u = unpolarized_spectrum(value);

        Color3f rgb;
        if constexpr (is_spectral_v<Spectrum>)
            rgb = spectrum_to_srgb(spec_u, wavelengths, active);
        else if constexpr (is_monochromatic_v<Spectrum>)
            rgb = spec_u.x();
        else
            rgb = spec_u;

        Float values[5] = { rgb.x(), rgb.y(), rgb.z(), 0, 0 };

        if (m_channel_count == 4) {
            values[3] = weight;
        } else if (m_channel_count == 5) {
            values[3] = alpha;
            values[4] = weight;
        } else {
            Throw("ImageBlock::put(): non-standard image block configuration! (AOVs?)");
        }

        put(pos, values, active);
    }

    /**
     * \brief Accumulate a single sample or a wavefront of samples into
     * the image block.
     *
     * \param pos
     *    Denotes the sample position in fractional pixel coordinates
     *
     * \param values
     *    Points to an array of length \ref channel_count(), which specifies
     *    the sample value for each channel.
     */
    void put(const Point2f &pos, const Float *values, Mask active = true);

    /**
     * \brief Fetch a single sample or a wavefront of samples from the image
     * block.
     *
     * This function is the opposite of \ref put(): instead of performing a
     * weighted accumulation of sample values based on the reconstruction
     * filter, it performs a weighted interpolation using gather operations.
     *
     * \param pos
     *    Denotes the sample position in fractional pixel coordinates
     *
     * \param values
     *    Points to an array of length \ref channel_count(), which will
     *    receive the result of the read operation for each channel. In
     *    Python, the function returns these values as a list.
     */
    void read(const Point2f &pos, Float *values, Mask active = true) const;

    /// Clear the image block contents to zero.
    void clear();

    // =============================================================
    //! @{ \name Accessors
    // =============================================================

    /**\brief Set the current block offset.
     *
     * This corresponds to the offset from the top-left corner of a larger
     * image (e.g. a Film) to the top-left corner of this ImageBlock instance.
     */
    void set_offset(const ScalarPoint2i &offset) { m_offset = offset; }

    /// Set the block size. This potentially destroys the block's content.
    void set_size(const ScalarVector2u &size);

    /// Return the current block offset
    const ScalarPoint2i &offset() const { return m_offset; }

    /// Return the current block size
    const ScalarVector2u &size() const { return m_size; }

    /// Return the bitmap's width in pixels
    uint32_t width() const { return m_size.x(); }

    /// Return the bitmap's height in pixels
    uint32_t height() const { return m_size.y(); }

    /// Warn when writing invalid (NaN, +/- infinity) sample values?
    void set_warn_invalid(bool value) { m_warn_invalid = value; }

    /// Warn when writing invalid (NaN, +/- infinity) sample values?
    bool warn_invalid() const { return m_warn_invalid; }

    /// Warn when writing negative sample values?
    void set_warn_negative(bool value) { m_warn_negative = value; }

    /// Warn when writing negative sample values?
    bool warn_negative() const { return m_warn_negative; }

    /// Re-normalize filter weights in \ref put() and \ref read()
    void set_normalize(bool value) { m_normalize = value; }

    /// Re-normalize filter weights in \ref put() and \ref read()
    bool normalize() const { return m_normalize; }

    /// Try to coalesce reads/writes in JIT modes?
    void set_coalesce(bool value) { m_coalesce = value; }

    /// Try to coalesce reads/writes in JIT modes?
    bool coalesce() const { return m_coalesce; }

    /// Use Kahan-style error-compensated floating point accumulation?
    void set_compensate(bool value) { m_compensate = value; }

    /// Use Kahan-style error-compensated floating point accumulation?
    bool compensate() const { return m_compensate; }

    /// Return the number of channels stored by the image block
    uint32_t channel_count() const { return m_channel_count; }

    /// Return the border region used by the reconstruction filter
    uint32_t border_size() const { return m_border_size; }

    /// Does the image block have a border region?
    bool has_border() const { return m_border_size != 0; }

    /// Return the image reconstruction filter underlying the ImageBlock
    const ReconstructionFilter *rfilter() const { return m_rfilter; }

    /// Return the underlying image tensor
    TensorXf &tensor();

    /// Return the underlying image tensor (const version)
    const TensorXf &tensor() const;

    //! @}
    // =============================================================

    std::string to_string() const override;

    MI_DECLARE_CLASS()
protected:
    /// Virtual destructor
    virtual ~ImageBlock();

    // Implementation detail to atomically accumulate a value into the image block
    void accum(Float value, UInt32 index, Bool active);
protected:
    ScalarPoint2i m_offset;
    ScalarVector2u m_size;
    uint32_t m_channel_count;
    uint32_t m_border_size;
    TensorXf m_tensor;
    mutable TensorXf m_tensor_compensation;
    ref<const ReconstructionFilter> m_rfilter;
    bool m_normalize;
    bool m_coalesce;
    bool m_compensate;
    bool m_warn_negative;
    bool m_warn_invalid;
};

MI_EXTERN_CLASS(ImageBlock)
NAMESPACE_END(mitsuba)
