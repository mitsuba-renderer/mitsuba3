#pragma once

#include <mitsuba/core/struct.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief General-purpose bitmap class with read and write support
 * for several common file formats.
 *
 * This class handles loading of PNG, JPEG, BMP, TGA, as well as
 * OpenEXR files, and it supports writing of PNG, JPEG and OpenEXR files.
 *
 * PNG and OpenEXR files are optionally annotated with string-valued
 * metadata, and the gamma setting can be stored as well. Please see
 * the class methods and enumerations for further detail.
 */
class MTS_EXPORT_CORE Bitmap : public Object {
public:
    // ======================================================================
    //! @{ \name Constructors and Enumerations
    // ======================================================================

    /**
     * This enumeration lists all pixel format types supported
     * by the \ref Bitmap class. This both determines the
     * number of channels, and how they should be interpreted
     */
    enum EPixelFormat {
        /// Single-channel luminance bitmap
        ELuminance                = 0x00,

        /// Two-channel luminance + alpha bitmap
        ELuminanceAlpha           = 0x01,

        /// RGB bitmap
        ERGB                      = 0x02,

        /// RGB bitmap + alpha channel
        ERGBA                     = 0x03,

        /// XYZ tristimulus bitmap
        EXYZ                      = 0x04,

        /// XYZ tristimulus + alpha channel
        EXYZA                     = 0x05,

        /// Arbitrary multi-channel bitmap without a fixed interpretation
        EMultiChannel             = 0x10
    };

    /// Supported file formats
    enum EFileFormat {
        /**
         * \brief Portable network graphics
         *
         * The following is supported:
         * <ul>
         * <li> Loading and saving of 8/16-bit per component bitmaps for
         *   all pixel formats (ELuminance, ELuminanceAlpha, ERGB, ERGBA)</li>
         * <li> Loading and saving of 1-bit per component mask bitmaps</li>
         * <li> Loading and saving of string-valued metadata fields</li>
         * </ul>
         */
        EPNG = 0,

        /**
         * \brief OpenEXR high dynamic range file format developed by
         * Industrial Light & Magic (ILM)
         *
         * The following is supported:
         * <ul>
         *   <li>Loading and saving of \ref Eloat16 / \ref EFloat32/ \ref
         *   EUInt32 bitmaps with all supported RGB/Luminance/Alpha combinations</li>
         *   <li>Loading and saving of spectral bitmaps</tt>
         *   <li>Loading and saving of XYZ tristimulus bitmaps</tt>
         *   <li>Loading and saving of string-valued metadata fields</li>
         * </ul>
         *
         * The following is <em>not</em> supported:
         * <ul>
         *   <li>Saving of tiled images, tile-based read access</li>
         *   <li>Display windows that are different than the data window</li>
         *   <li>Loading of spectrum-valued bitmaps</li>
         * </ul>
         */
        EOpenEXR,

        /**
         * \brief RGBE image format by Greg Ward
         *
         * The following is supported
         * <ul>
         *   <li>Loading and saving of \ref EFloat32 - based RGB bitmaps</li>
         * </ul>
         */
        ERGBE,

        /**
         * \brief PFM (Portable Float Map) image format
         *
         * The following is supported
         * <ul>
         *   <li>Loading and saving of \ref EFloat32 - based Luminance or RGB bitmaps</li>
         * </ul>
         */
        EPFM,

        /**
         * \brief PPM (Portable Pixel Map) image format
         *
         * The following is supported
         * <ul>
         *   <li>Loading and saving of \ref EUInt8 and \ref EUInt16 - based RGB bitmaps</li>
         * </ul>
         */
        EPPM,

        /**
         * \brief Joint Photographic Experts Group file format
         *
         * The following is supported:
         * <ul><li>
         * Loading and saving of 8 bit per component RGB and
         * luminance bitmaps
         * </li></ul>
         */
        EJPEG,

        /**
         * \brief Truevision Advanced Raster Graphics Array file format
         *
         * The following is supported:
         * <ul><li>Loading of uncompressed 8-bit RGB/RGBA files</ul></li>
         */
        ETGA,

        /**
         * \brief Windows Bitmap file format
         *
         * The following is supported:
         * <ul><li>Loading of uncompressed 8-bit luminance and RGBA bitmaps</ul></li>
         */
        EBMP,

        /**
         * \brief Automatically detect the file format
         *
         * Note: this flag only applies when loading a file. In this case,
         * the source stream must support the \c seek() operation.
         */
        EAuto
    };

    /**
     * \brief Create a bitmap of the specified type and allocate
     * the necessary amount of memory
     *
     * \param pFmt
     *    Specifies the pixel format (e.g. RGBA or Luminance-only)
     *
     * \param cFmt
     *    Specifies how the per-pixel components are encoded
     *    (e.g. unsigned 8 bit integers or 32-bit floating point values)
     *
     * \param size
     *    Specifies the horizontal and vertical bitmap size in pixels
     *
     * \param channelCount
     *    Channel count of the image. This parameter is only required when
     *    \c pFmt = \ref EMultiChannel or \ref EMultiSpectrumAlphaWeight
     *
     * \param data
     *    External pointer to the image data. If set to \c NULL, the
     *    implementation will allocate memory itself.
     */
    Bitmap(EPixelFormat pFmt, Struct::EType cFmt, const Vector2s &size,
        uint32_t channelCount = 0, uint8_t *data = nullptr);

    /// Return the bitmap dimensions in pixels
    const Vector2s &size() const { return m_size; }

    /// Return the number of channels used by this bitmap
    size_t channelCount() const { return (size_t) m_channelCount; }

    /// Return the number bytes of storage used per pixel
    size_t bytesPerPixel() const;

    /// Return the bitmap size in bytes (excluding metadata)
    size_t bufferSize() const;

    MTS_DECLARE_CLASS()

protected:

    /// Protected destructor
    virtual ~Bitmap();

    /// Determine the channel count from the pixel format
    void updateChannelCount();

protected:
    EPixelFormat m_pixelFormat;
    Struct::EType m_componentFormat;
    Vector2s m_size;
    std::unique_ptr<uint8_t[], AlignedAllocator> m_data;
    ref<Struct> m_struct;
    Float m_gamma;
    uint32_t m_channelCount;
    bool m_ownsData;
};

NAMESPACE_END(mitsuba)
