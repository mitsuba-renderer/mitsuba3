#pragma once

#include <mitsuba/core/struct.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/properties.h>

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
     * \brief Describes a sub-layer of a multilayer bitmap (e.g. OpenEXR)
     *
     * A layer is defined as a named collection of bitmap channels along with a
     * pixel format. This data structure is used by \ref Bitmap::getLayers().
     */
    struct Layer {
        /// Descriptive name of the bitmap layer
        std::string name;
        /// Pixel format of the layer
        EPixelFormat pixel_format;
        /// Data structure listing channels and component formats
        ref<Struct> struct_;
    };

    /**
     * \brief Create a bitmap of the specified type and allocate
     * the necessary amount of memory
     *
     * \param pfmt
     *    Specifies the pixel format (e.g. RGBA or Luminance-only)
     *
     * \param cfmt
     *    Specifies how the per-pixel components are encoded
     *    (e.g. unsigned 8 bit integers or 32-bit floating point values)
     *
     * \param size
     *    Specifies the horizontal and vertical bitmap size in pixels
     *
     * \param channel_count
     *    Channel count of the image. This parameter is only required when
     *    \c pfmt = \ref EMultiChannel
     *
     * \param data
     *    External pointer to the image data. If set to \c nullptr, the
     *    implementation will allocate memory itself.
     */
    Bitmap(EPixelFormat pfmt, Struct::EType cfmt, const Vector2s &size,
        size_t channel_count = 0, uint8_t *data = nullptr);

    /**
     * \brief Load a bitmap from an arbitrary stream data source
     *
     * \param stream
     *    Pointer to an arbitrary stream data source
     *
     * \param format
     *    File format to be read (PNG/EXR/Auto-detect ...)
     */
    Bitmap(Stream *stream, EFileFormat format = EAuto);

    /// Copy constructor (copies the image contents)
    Bitmap(const Bitmap &bitmap);

    /// Return the pixel format of this bitmap
    EPixelFormat pixel_format() const { return m_pixel_format; }

    /// Return the component format of this bitmap
    Struct::EType component_format() const { return m_component_format; }

    /// Return a pointer to the underlying bitmap storage
    void *data() { return m_data.get(); }

    /// Return a pointer to the underlying bitmap storage
    const void *data() const { return m_data.get(); }

    /// Return the bitmap dimensions in pixels
    const Vector2s &size() const { return m_size; }

    /// Return the bitmap's width in pixels
    size_t width() const { return m_size.x(); }

    /// Return the bitmap's height in pixels
    size_t height() const { return m_size.y(); }

    /// Return the total number of pixels
    size_t pixel_count() const { return hprod(m_size); }

    /// Return the number of channels used by this bitmap
    size_t channel_count() const { return m_struct->field_count(); }

    /// Return whether this image has an alpha channel
    bool has_alpha() const {
        return m_pixel_format == ELuminanceAlpha || m_pixel_format == ERGBA ||
               m_pixel_format == EXYZA;
    }

    /// Return the number bytes of storage used per pixel
    size_t bytes_per_pixel() const;

    /// Return the bitmap size in bytes (excluding metadata)
    size_t buffer_size() const;

    /// Return the bitmap's gamma identifier (-1: sRGB)
    Float gamma() const { return m_gamma; }

    /// Set the bitmap's gamma identifier (-1: sRGB)
    void set_gamma(Float gamma) { m_gamma = gamma; }

    /// Return a \ref Properties object containing the image metadata
    Properties &metadata() { return m_metadata; }

    /// Return a \ref Properties object containing the image metadata (const version)
    const Properties &metadata() const { return m_metadata; }

    /// Set the a \ref Properties object containing the image metadata
    void set_metadata(const Properties &metadata) { m_metadata = metadata; }

    /// Clear the bitmap to zero
    void clear();

    /// Return a \c Struct instance describing the contents of the bitmap
    const Struct *struct_() const { return m_struct.get(); }

    /// Return a human-readable summary of this bitmap
    virtual std::string to_string() const override;

    MTS_DECLARE_CLASS()

protected:

    /// Protected destructor
    virtual ~Bitmap();

    /// Rebuild the 'm_struct' field based on the pixel format etc.
    void rebuild_struct(size_t channel_count);

protected:
    std::unique_ptr<uint8_t[], enoki::aligned_deleter> m_data;
    EPixelFormat m_pixel_format;
    Struct::EType m_component_format;
    Vector2s m_size;
    ref<Struct> m_struct;
    Float m_gamma;
    bool m_owns_data;
    Properties m_metadata;
};

extern MTS_EXPORT_CORE std::ostream &operator<<(std::ostream &os, Bitmap::EPixelFormat value);

NAMESPACE_END(mitsuba)
