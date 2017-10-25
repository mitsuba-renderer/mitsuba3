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
     *    Specifies the number of output channels. This is only necessary
     *    when \ref Bitmap::EMultiChannel is chosen as the pixel format
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
     * image block
     *
     * This variant assumes that the image block's internal storage format is
     * an (R, G, B) triplet of Floats.
     *
     * \param pos
     *    Denotes the sample position in fractional pixel coordinates
     * \param spec
     *    Spectrum value assocated with the sample
     * \param alpha
     *    Alpha value assocated with the sample
     * \return \c false if one of the sample values was \a invalid, e.g.
     *    NaN or negative. A warning is also printed if \c m_warn is enabled.
     */
    template<typename Point2,
             typename Spectrum,
             typename Value = typename Point2::Value>
    bool put(const Point2 &pos, const Spectrum &spec, Value /*alpha*/) {
        // TODO: which internal storage format should we use?
        // 1. Spectrum samples, alpha, filter weight
        // Value temp[MTS_WAVELENGTH_SAMPLES + 2];
        // for (int i = 0; i < MTS_WAVELENGTH_SAMPLES; ++i)
        //     temp[i] = spec[i];
        // temp[MTS_WAVELENGTH_SAMPLES] = alpha;
        // temp[MTS_WAVELENGTH_SAMPLES + 1] = 1.0f;
        // return put(pos, temp);

        // 2. m first spectrum samples
        // TODO: this was just for debugging, it's not a meaningful format.
        std::vector<Value> temp(m_bitmap->channel_count());
        if (temp.size() > MTS_WAVELENGTH_SAMPLES) {
            Throw("Too many channels in ImageBlock");
        }
        // Value temp[3];
        for (size_t i = 0; i < temp.size(); ++i)
            temp[i] = spec[i];
        return put(pos, temp.data());
    }

    /**
     * \brief Store a single sample inside the block
     *
     * \param _pos
     *    Denotes the sample position in fractional pixel coordinates
     * \param value
     *    Pointer to an array containing each channel of the sample values.
     *    The array must match the length given by \ref channel_count()
     * \return \c false if one of the sample values was \a invalid, e.g.
     *    NaN or negative. A warning is also printed if \c m_warn is enabled.
     */
    bool put(const Point2f &_pos, const Float *value);

    /**
     * \brief Store a packet of samples inside the block
     *
     * \param _pos
     *    Denotes the samples positions in fractional pixel coordinates
     * \param value
     *    Pointer to an array containing packets for each channel of the sample
     *    values. The array must match the length given by \ref channel_count()
     * \return \c false if one of the sample values was \a invalid, e.g.
     *    NaN or negative. A warning is also printed if \c m_warn is enabled.
     */
    bool put(const Point2fP &_pos, const FloatP *value);

    /// Create a clone of the entire image block
    ref<ImageBlock> clone() const {
        ref<ImageBlock> clone = new ImageBlock(m_bitmap->pixel_format(),
            m_bitmap->size() - Vector2i(2 * m_border_size, 2 * m_border_size), m_filter, m_bitmap->channel_count());
        copy_to(clone);
        return clone;
    }

    /// Copy the contents of this image block to another one with the same configuration
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

    /// Set the current block offset
    void set_offset(const Point2i &offset) { m_offset = offset; }

    /// Return the current block offset
    const Point2i &offset() const { return m_offset; }

    /// Set the current block size
    void set_size(const Vector2i &size) { m_size = size; }

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
    Float *m_weights_x, *m_weights_y;
    bool m_warn;
};

NAMESPACE_END(mitsuba)
