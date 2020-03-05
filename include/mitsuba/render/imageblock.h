#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Storage for an image sub-block (a.k.a render bucket)
 *
 * This class is used by image-based parallel processes and encapsulates
 * computed rectangular regions of an image. This allows for easy and efficient
 * distributed rendering of large images. Image blocks usually also include a
 * border region storing contributions that are slightly outside of the block,
 * which is required to support image reconstruction filters.
 */
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER ImageBlock : public Object {
public:
    MTS_IMPORT_TYPES(ReconstructionFilter)

    /**
     * Construct a new image block of the requested properties
     *
     * \param size
     *    Specifies the block dimensions (not accounting for additional
     *    border pixels required to support image reconstruction filters)
     *
     * \param channel_count
     *    Specifies the number of image channels.
     *
     * \param filter
     *    Pointer to the film's reconstruction filter. If passed, it is used to
     *    compute and store reconstruction weights. Note that it is mandatory
     *    when any of the block's \ref put operations are used, except for
     *    \c put(const ImageBlock*).
     *
     * \param warn_negative
     *    Warn when writing samples with negative components?
     *
     * \param warn_invalid
     *    Warn when writing samples with components that are equal to
     *    NaN (not a number) or +/- infinity?
     *
     * \param border
     *    Allocate a border region around the image block to support
     *    contributions to adjacent pixels when using wide (i.e. non-box)
     *    reconstruction filters?
     *
     * \param normalize
     *    Ensure that splats created via ``ImageBlock::put()`` add a
     *    unit amount of energy? Stratified sampling techniques that
     *    sample rays in image space should set this to \c false, since
     *    the samples will eventually be divided by the accumulated
     *    sample weight to remove any non-uniformity.
     */
    ImageBlock(const ScalarVector2i &size,
               size_t channel_count,
               const ReconstructionFilter *filter = nullptr,
               bool warn_negative = true,
               bool warn_invalid = true,
               bool border = true,
               bool normalize = false);

    /// Accumulate another image block into this one
    void put(const ImageBlock *block);

    /**
     * \brief Store a single sample / packets of samples inside the
     * image block.
     *
     * \note This method is only valid if a reconstruction filter was given at
     * the construction of the block.
     *
     * \param pos
     *    Denotes the sample position in fractional pixel coordinates. It is
     *    not checked, and so must be valid. The block's offset is subtracted
     *    from the given position to obtain the
     *
     * \param wavelengths
     *    Sample wavelengths in nanometers
     *
     * \param value
     *    Sample value assocated with the specified wavelengths
     *
     * \param alpha
     *    Alpha value assocated with the sample
     *
     * \return \c false if one of the sample values was \a invalid, e.g.
     *    NaN or negative. A warning is also printed if \c m_warn_negative
     *    or \c m_warn_invalid is enabled.
     */
    Mask put(const Point2f &pos,
             const Wavelength &wavelengths,
             const Spectrum &value,
             const Float &alpha,
             Mask active = true) {
        if (unlikely(m_channel_count != 5))
            Throw("ImageBlock::put(): non-standard image block configuration! (AOVs?)");

        UnpolarizedSpectrum value_u = depolarize(value);

        Color3f xyz;
        if constexpr (is_monochromatic_v<Spectrum>) {
            ENOKI_MARK_USED(wavelengths);
            xyz = value_u.x();
        } else if constexpr (is_rgb_v<Spectrum>) {
            ENOKI_MARK_USED(wavelengths);
            xyz = srgb_to_xyz(value_u, active);
        } else {
            static_assert(is_spectral_v<Spectrum>);
            xyz = spectrum_to_xyz(value_u, wavelengths, active);
        }
        Float values[5] = { xyz.x(), xyz.y(), xyz.z(), alpha, 1.f };
        return put(pos, values, active);
    }

    /**
     * \brief Store a single sample inside the block.
     *
     * \note This method is only valid if a reconstruction filter was provided
     * when the block was constructed.
     *
     * \param pos
     *    Denotes the sample position in fractional pixel coordinates. It is
     *    not checked, and so must be valid. The block's offset is subtracted
     *    from the given position to obtain the
     *
     * \param value
     *    Pointer to an array containing each channel of the sample values.
     *    The array must match the length given by \ref channel_count()
     *
     * \return \c false if one of the sample values was \a invalid, e.g.
     *    NaN or negative. A warning is also printed if \c m_warn_negative
     *    or \c m_warn_invalid is enabled.
     */
    Mask put(const Point2f &pos, const Float *value, Mask active = true);

    /// Clear everything to zero.
    void clear();

    // =============================================================
    //! @{ \name Accesors
    // =============================================================

    /**\brief Set the current block offset.
     *
     * This corresponds to the offset from the top-left corner of a larger
     * image (e.g. a Film) to the top-left corner of this ImageBlock instance.
     */
    void set_offset(const ScalarPoint2i &offset) { m_offset = offset; }

    /// Set the block size. This potentially destroys the block's content.
    void set_size(const ScalarVector2i &size);

    /// Return the current block offset
    const ScalarPoint2i &offset() const { return m_offset; }

    /// Return the current block size
    const ScalarVector2i &size() const { return m_size; }

    /// Return the bitmap's width in pixels
    size_t width() const { return m_size.x(); }

    /// Return the bitmap's height in pixels
    size_t height() const { return m_size.y(); }

    /// Warn when writing invalid (NaN, +/- infinity) sample values?
    void set_warn_invalid(bool value) { m_warn_invalid = value; }

    /// Warn when writing invalid (NaN, +/- infinity) sample values?
    bool warn_invalid() const { return m_warn_invalid; }

    /// Warn when writing negative sample values?
    void set_warn_negative(bool value) { m_warn_negative = value; }

    /// Warn when writing negative sample values?
    bool warn_negative() const { return m_warn_negative; }

    /// Return the number of channels stored by the image block
    size_t channel_count() const { return (size_t) m_channel_count; }

    /// Return the border region used by the reconstruction filter
    int border_size() const { return m_border_size; }

    /// Return the underlying pixel buffer
    DynamicBuffer<Float> &data() { return m_data; }

    /// Return the underlying pixel buffer (const version)
    const DynamicBuffer<Float> &data() const { return m_data; }

    //! @}
    // =============================================================

    std::string to_string() const override;

    MTS_DECLARE_CLASS()
protected:
    /// Virtual destructor
    virtual ~ImageBlock();
protected:
    ScalarPoint2i m_offset;
    ScalarVector2i m_size;
    uint32_t m_channel_count;
    int m_border_size;
    DynamicBuffer<Float> m_data;
    const ReconstructionFilter *m_filter;
    Float *m_weights_x, *m_weights_y;
    bool m_warn_negative;
    bool m_warn_invalid;
    bool m_normalize;
};

MTS_EXTERN_CLASS_RENDER(ImageBlock)
NAMESPACE_END(mitsuba)
