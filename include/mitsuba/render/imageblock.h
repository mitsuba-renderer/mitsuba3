#pragma once

#include <mitsuba/mitsuba.h>
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
class MTS_EXPORT_RENDER ImageBlock : public Object {
public:
    /**
     * Construct a new image block of the requested properties
     *
     * \param fmt
     *    Specifies the pixel format -- see \ref Bitmap::EPixelFormat
     *    for a list of possibilities
     * \param size
     *    Specifies the block dimensions (not accounting for additional
     *    border pixels required to support image reconstruction filters)
     * \param filter
     *    Pointer to the film's reconstruction filter. If passed, it is used to
     *    compute and store reconstruction weights. Note that it is mandatory
     *    when any of the block's \ref put operations are used, except for
     *    \c put(const ImageBlock*).
     * \param channels
     *    Specifies the number of output channels. This is only valid
     *    when \ref Bitmap::EMultiChannel is chosen as the pixel format,
     *    otherwise pass 0 so that channels are set automatically from the
     *    pixel format.
     * \param warn
     *    Warn when writing bad sample values?
     */
    ImageBlock(Bitmap::EPixelFormat fmt,
               const Vector2i &size,
               const ReconstructionFilter *filter = nullptr,
               size_t channels = 0,
               bool warn = true);

    /// Accumulate another image block into this one
    void put(const ImageBlock *block) {
        Point2i offset = block->offset() - m_offset
                         - Vector2i(block->border_size() - m_border_size);
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
     * \param spectrum
     *    Spectrum value assocated with the sample
     * \param alpha
     *    Alpha value assocated with the sample
     * \return \c false if one of the sample values was \a invalid, e.g.
     *    NaN or negative. A warning is also printed if \c m_warn is enabled.
     */
    template<typename Point2, typename Spectrum,
             typename Value = typename Point2::Value>
    mask_t<Value> put(const Point2 &pos, const Spectrum &spectrum,
                      const Value &alpha,
                      const mask_t<Value> &active) {
        using ValueVector = std::vector<Value, aligned_allocator<Value>>;
        Assert(m_bitmap->pixel_format() == Bitmap::EXYZAW,
               "This `put` variant requires XYZAW internal storage format.");

        // Convert spectrum to XYZ
        // TODO: proper handling of spectral rendering (use sampled wavelengths)
        static const Spectrum wavelengths(
            MTS_WAVELENGTH_MIN, 517, 673, MTS_WAVELENGTH_MAX
        );
        static const auto responses = cie1931_xyz(wavelengths);

        Value x(0.0f), y(0.0f), z(0.0f);
        for (int li = 0; li < MTS_WAVELENGTH_SAMPLES; ++li) {
            x += spectrum.coeff(li) * std::get<0>(responses).coeff(li);
            y += spectrum.coeff(li) * std::get<1>(responses).coeff(li);
            z += spectrum.coeff(li) * std::get<2>(responses).coeff(li);
        }

        ValueVector temp(m_bitmap->channel_count());
        temp[0] = x;
        temp[1] = y;
        temp[2] = z;
        temp[3] = alpha;
        temp[4] = 1.0f;  // Will be multiplied by the reconstruction weight.
        return put(pos, temp.data(), active);
    }

    /**
     * \brief Store a single sample inside the block.
     *
     * \note This method is only valid if a reconstruction filter was given at
     * the construction of the block.
     *
     * \param _pos
     *    Denotes the sample position in fractional pixel coordinates. It is
     *    not checked, and so must be valid. The block's offset is subtracted
     *    from the given position to obtain the
     * \param value
     *    Pointer to an array containing each channel of the sample values.
     *    The array must match the length given by \ref channel_count()
     * \return \c false if one of the sample values was \a invalid, e.g.
     *    NaN or negative. A warning is also printed if \c m_warn is enabled.
     */
    bool put(const Point2f &_pos, const Float *value, bool /*unused*/ = true);

    /**
     * \brief Store a packet of samples inside the block.
     *
     * \note This method is only valid if a reconstruction filter was given at
     * the construction of the block.
     *
     * \param _pos
     *    Denotes the samples positions in fractional pixel coordinates.
     * \param value
     *    Pointer to an array containing packets for each channel of the sample
     *    values. The array must match the length given by \ref channel_count()
     * \return \c false if one of the sample values was \a invalid, e.g.
     *    NaN or negative. A warning is also printed if \c m_warn is enabled.
     */
    mask_t<FloatP> put(const Point2fP &_pos, const FloatP *value,
                       const mask_t<FloatP> &active = true);

    /// Create a clone of the entire image block
    ref<ImageBlock> clone() const {
        ref<ImageBlock> clone = new ImageBlock(m_bitmap->pixel_format(),
            m_bitmap->size() - Vector2i(2 * m_border_size, 2 * m_border_size), m_filter, m_bitmap->channel_count());
        copy_to(clone);
        return clone;
    }

    /// Copy the contents of this image block to another one with the same
    /// configuration. The reconstruction filter is left as-is.
    void copy_to(ImageBlock *copy) const {
        memcpy(copy->bitmap()->uint8_data(), m_bitmap->uint8_data(), m_bitmap->buffer_size());
        copy->m_size = m_size;
        copy->m_offset = m_offset;
        copy->m_warn = m_warn;
    }

    /// Clear everything to zero
    void clear() { m_bitmap->clear(); }

    // =============================================================
    //! @{ \name Accesors
    // =============================================================

    /// Set the current block offset. This corresponds to the offset
    /// from a larger image's (e.g. a Film) corner to this block's corner.
    void set_offset(const Point2i &offset) { m_offset = offset; }

    /// Return the current block offset
    const Point2i &offset() const { return m_offset; }

    /// Return the current block size
    const Vector2i &size() const { return m_size; }

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
    Bitmap::EPixelFormat pixel_format() const { return m_bitmap->pixel_format(); }

    /// Return a pointer to the underlying bitmap representation
    Bitmap *bitmap() { return m_bitmap; }

    /// Return a pointer to the underlying bitmap representation (const version)
    const Bitmap *bitmap() const { return m_bitmap.get(); }

    //! @}
    // =============================================================

    std::string to_string() const override;

    MTS_DECLARE_CLASS()
protected:
    /// Virtual destructor
    virtual ~ImageBlock();
protected:
    ref<Bitmap> m_bitmap;
    Point2i m_offset;
    Vector2i m_size;
    int m_border_size;
    const ReconstructionFilter *m_filter;
    Float  *m_weights_x  , *m_weights_y;
    FloatP *m_weights_x_p, *m_weights_y_p;
    bool m_warn;
};

NAMESPACE_END(mitsuba)
