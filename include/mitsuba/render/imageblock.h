#pragma once

#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/spectrum.h>
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
    MTS_DECLARE_CLASS_VARIANT(ImageBlock, Object)
    MTS_IMPORT_TYPES()
    using ReconstructionFilter = typename RenderAliases::ReconstructionFilter;

    /**
     * Construct a new image block of the requested properties
     *
     * \param fmt
     *    Specifies the pixel format -- see \ref PixelFormat
     *    for a list of possibilities
     *
     * \param size
     *    Specifies the block dimensions (not accounting for additional
     *    border pixels required to support image reconstruction filters)
     *
     * \param filter
     *    Pointer to the film's reconstruction filter. If passed, it is used to
     *    compute and store reconstruction weights. Note that it is mandatory
     *    when any of the block's \ref put operations are used, except for
     *    \c put(const ImageBlock*).
     *
     * \param channels
     *    Specifies the number of output channels. This is only valid
     *    when \ref MultiChannel is chosen as the pixel format,
     *    otherwise pass 0 so that channels are set automatically from the
     *    pixel format.
     *
     * \param border
     *    Allocate a border region around the image block to support
     *    contributions to adjacent pixels when using wide (i.e. non-box)
     *    reconstruction filters?
     *
     * \param warn
     *    Warn when writing bad sample values?
     *
     * \param normalize
     *    Ensure that splats created via ``ImageBlock::put()`` add a
     *    unit amount of energy?
     */
    ImageBlock(PixelFormat fmt,
               const ScalarVector2i &size,
               const ReconstructionFilter *filter = nullptr,
               size_t channels = 0,
               bool warn = true,
               bool border = true,
               bool normalize = false);

    /// Accumulate another image block into this one
    void put(const ImageBlock *block) {
        ScalarPoint2i offset = block->offset() - m_offset -
                         ScalarVector2i(block->border_size() - m_border_size);
        m_bitmap->accumulate(block->bitmap(), offset);
    }

    /**
     * \brief Store a single sample / packets of samples inside the
     * image block.
     *
     * \note This method is only valid if a reconstruction filter was given at
     * the construction of the block.
     *
     * This variant assumes that the ImageBlock's internal storage format
     * is XYZAW. The given Spectrum will be converted to XYZ color space
     * for storage.
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
     *    NaN or negative. A warning is also printed if \c m_warn is enabled.
     */
    Mask put(const Point2f &pos,
             const Wavelength &wavelengths,
             const Spectrum &value,
             const Float &alpha,
             Mask active = true) {
        Assert(m_bitmap->pixel_format() == PixelFormat::XYZAW,
               "This `put` variant requires XYZAW internal storage format.");

        Array<Float, 3> xyz;
        if constexpr (is_monochrome_v<Spectrum>) {
            xyz = depolarize(value).x();
        } else if constexpr (is_rgb_v<Spectrum>) {
            xyz = srgb_to_xyz(depolarize(value), active);
        } else {
            static_assert(is_spectral_v<Spectrum>);
            xyz = spectrum_to_xyz(depolarize(value), wavelengths, active);
        }
        Array<Float, 5> values(xyz.x(), xyz.y(), xyz.z(), alpha, 1.f);
        return put(pos, values.data(), active);
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
     *    NaN or negative. A warning is also printed if \c m_warn is enabled.
     */
    Mask put(const Point2f &pos, const Float *value, Mask active = true);

    /// Clear everything to zero.
    void clear() { m_bitmap->clear(); }
#if defined(MTS_ENABLE_AUTODIFF)
    void clear_d();
#endif

    // =============================================================
    //! @{ \name Accesors
    // =============================================================

    /// Set the current block offset. This corresponds to the offset
    /// from a larger image's (e.g. a Film) corner to this block's corner.
    void set_offset(const ScalarPoint2i &offset) { m_offset = offset; }

    /// Return the current block offset
    const ScalarPoint2i &offset() const { return m_offset; }

    /// Return the current block size
    const ScalarVector2i &size() const { return m_size; }

    /// Return the bitmap's width in pixels
    size_t width() const { return m_size.x(); }

    /// Return the bitmap's height in pixels
    size_t height() const { return m_size.y(); }

    /// Warn when writing bad sample values?
    bool warns() const { return m_warn; }

    /// Warn when writing bad sample values?
    void set_warn(bool warn) { m_warn = warn; }

    /// Return the border region used by the reconstruction filter
    size_t border_size() const { return m_border_size; }

    /// Return the number of channels stored by the image block
    size_t channel_count() const { return m_bitmap->channel_count(); }

    /// Return the underlying pixel format
    PixelFormat pixel_format() const { return m_bitmap->pixel_format(); }

    /// Return a pointer to the underlying bitmap representation
    Bitmap *bitmap() { return m_bitmap; }

    /// Return a pointer to the underlying bitmap representation (const version)
    const Bitmap *bitmap() const { return m_bitmap.get(); }

#if defined(MTS_ENABLE_AUTODIFF)
    /// Return the differentiable variables representing the image (one per channel)
    std::vector<FloatD> &bitmap_d() { return m_bitmap_d; }

    /// Return the differentiable variable representing the image (one per channel)
    const std::vector<FloatD> &bitmap_d() const { return m_bitmap_d; }
#endif

    //! @}
    // =============================================================

    std::string to_string() const override;

protected:
    /// Virtual destructor
    virtual ~ImageBlock();
protected:
    ref<Bitmap> m_bitmap;
    ScalarPoint2i m_offset;
    ScalarVector2i m_size;
    int m_border_size;
    const ReconstructionFilter *m_filter;
    Float  *m_weights_x  , *m_weights_y;
    bool m_warn, m_normalize;

#if defined(MTS_ENABLE_AUTODIFF)
    std::vector<FloatD> m_bitmap_d;
#endif
};

MTS_EXTERN_CLASS(ImageBlock)
NAMESPACE_END(mitsuba)
