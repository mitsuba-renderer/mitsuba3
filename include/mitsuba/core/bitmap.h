#pragma once

#include <mitsuba/core/struct.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/rfilter.h>

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
    /// Boundary conditions for image resampling
    using EBoundaryCondition = ReconstructionFilter::EBoundaryCondition;

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
        EY,

        /// Two-channel luminance + alpha bitmap
        EYA,

        /// RGB bitmap
        ERGB,

        /// RGB bitmap + alpha channel
        ERGBA,

        /// RGB bitmap + alpha channel + weight
        ERGBAW,

        /// XYZ tristimulus bitmap
        EXYZ,

        /// XYZ tristimulus + alpha channel
        EXYZA,

        /// XYZ tristimulus + alpha channel + weight
        EXYZAW,

        /// Arbitrary multi-channel bitmap without a fixed interpretation
        EMultiChannel
    };

    /* Replicate Struct::EType here for convenience */
    static constexpr Struct::EType EInt8 = Struct::EInt8;
    static constexpr Struct::EType EUInt8 = Struct::EUInt8;
    static constexpr Struct::EType EInt16 = Struct::EInt16;
    static constexpr Struct::EType EUInt16 = Struct::EUInt16;
    static constexpr Struct::EType EInt32 = Struct::EInt32;
    static constexpr Struct::EType EUInt32 = Struct::EUInt32;
    static constexpr Struct::EType EInt64 = Struct::EInt64;
    static constexpr Struct::EType EUInt64 = Struct::EUInt64;
    static constexpr Struct::EType EFloat16 = Struct::EFloat16;
    static constexpr Struct::EType EFloat32 = Struct::EFloat32;
    static constexpr Struct::EType EFloat64 = Struct::EFloat64;
    static constexpr Struct::EType EFloat = Struct::EFloat;
    static constexpr Struct::EType EInvalid = Struct::EInvalid;

    /// Supported file formats
    enum EFileFormat {
        /**
         * \brief Portable network graphics
         *
         * The following is supported:
         * <ul>
         * <li> Loading and saving of 8/16-bit per component bitmaps for
         *   all pixel formats (EY, EYA, ERGB, ERGBA)</li>
         * <li> Loading and saving of 1-bit per component mask bitmaps</li>
         * <li> Loading and saving of string-valued metadata fields</li>
         * </ul>
         */
        EPNG,

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
     * \param pixel_format
     *    Specifies the pixel format (e.g. RGBA or Luminance-only)
     *
     * \param component_format
     *    Specifies how the per-pixel components are encoded
     *    (e.g. unsigned 8 bit integers or 32-bit floating point values).
     *    The component format Struct::EFloat will be translated to the
     *    corresponding compile-time precision type (EFloat32 or EFloat64).
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
    Bitmap(EPixelFormat pixel_format, Struct::EType component_format,
           const Vector2s &size, size_t channel_count = 0,
           uint8_t *data = nullptr);

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

    /**
     * \brief Load a bitmap from a given filename
     *
     * \param path
     *    Name of the file to be loaded
     *
     * \param format
     *    File format to be read (PNG/EXR/Auto-detect ...)
     */
    Bitmap(const fs::path &path, EFileFormat = EAuto);

    /// Copy constructor (copies the image contents)
    Bitmap(const Bitmap &bitmap);

    /// Move constructor
    Bitmap(Bitmap &&bitmap);

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
        return m_pixel_format == EYA
               || m_pixel_format == ERGBA || m_pixel_format == ERGBAW
               || m_pixel_format == EXYZA || m_pixel_format == EXYZAW;
    }

    /// Return the number bytes of storage used per pixel
    size_t bytes_per_pixel() const;

    /// Return the bitmap size in bytes (excluding metadata)
    size_t buffer_size() const;

    /// Return whether the bitmap uses an sRGB gamma encoding
    bool srgb_gamma() const { return m_srgb_gamma; }

    /// Specify whether the bitmap uses an sRGB gamma encoding
    void set_srgb_gamma(bool value);

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

    /**
     * Write an encoded form of the bitmap to a stream using the specified file format
     *
     * \param stream
     *    Target stream that will receive the encoded output
     *
     * \param format
     *    Target file format (\ref EOpenEXR, \ref EPNG, \ref EOpenEXR, etc.)
     *    Detected from the filename by default.
     *
     * \param quality
     *    Depending on the file format, this parameter takes on a slightly
     *    different meaning:
     *    <ul>
     *        <li>PNG images: Controls how much libpng will attempt to compress
     *            the output (with 1 being the lowest and 9 denoting the
     *            highest compression). The default argument uses the
     *            compression level 5. </li>
     *        <li>JPEG images: denotes the desired quality (between 0 and 100).
     *            The default argument (-1) uses the highest quality (100).</li>
     *        <li>OpenEXR images: denotes the quality level of the DWAB
     *            compressor, with higher values corresponding to a lower quality.
     *            A value of 45 is recommended as the default for lossy compression.
     *            The default argument (-1) causes the implementation to switch
     *            to the lossless PIZ compressor.</li>
     *    </ul>
     */
    void write(Stream *stream, EFileFormat format = EAuto,
               int quality = -1) const;

    /**
     * Write an encoded form of the bitmap to a file using the specified file format
     *
     * \param stream
     *    Target stream that will receive the encoded output
     *
     * \param format
     *    Target file format (\ref EOpenEXR, \ref EPNG, \ref EOpenEXR, etc.)
     *    Detected from the filename by default.
     *
     * \param quality
     *    Depending on the file format, this parameter takes on a slightly
     *    different meaning:
     *    <ul>
     *        <li>PNG images: Controls how much libpng will attempt to compress
     *            the output (with 1 being the lowest and 9 denoting the
     *            highest compression). The default argument uses the
     *            compression level 5. </li>
     *        <li>JPEG images: denotes the desired quality (between 0 and 100).
     *            The default argument (-1) uses the highest quality (100).</li>
     *        <li>OpenEXR images: denotes the quality level of the DWAB
     *            compressor, with higher values corresponding to a lower quality.
     *            A value of 45 is recommended as the default for lossy compression.
     *            The default argument (-1) causes the implementation to switch
     *            to the lossless PIZ compressor.</li>
     *    </ul>
     */
    void write(const fs::path &path, EFileFormat format = EAuto,
               int quality = -1) const;

    /**
     * \brief Up- or down-sample this image to a different resolution
     *
     * Uses the provided reconstruction filter and accounts for the requested
     * horizontal and vertical boundary conditions when looking up data outside
     * of the input domain.
     *
     * A minimum and maximum image value can be specified to prevent to prevent
     * out-of-range values that are created by the resampling process.
     *
     * The optional \c temp parameter can be used to pass an image of
     * resolution <tt>Vector2s(target->width(), this->height())</tt> to avoid
     * intermediate memory allocations.
     *
     * \param target
     *     Pre-allocated bitmap of the desired target resolution
     *
     * \param rfilter
     *     A separable image reconstruction filter (default: 2-lobe Lanczos filter)
     *
     * \param bch
     *     Horizontal and vertical boundary conditions (default: clamp)
     *
     * \param clamp
     *     Filtered image pixels will be clamped to the following
     *     range. Default: -infinity..infinity (i.e. no clamping is used)
     *
     * \param temp
     *     Optional: image for intermediate computations
     */
    void resample(Bitmap *target,
                  const ReconstructionFilter *rfilter = nullptr,
                  const std::pair<EBoundaryCondition, EBoundaryCondition> &bc =
                      { EBoundaryCondition::EClamp, EBoundaryCondition::EClamp },
                  const std::pair<Float, Float> &bound =
                      { -math::Infinity, math::Infinity },
                  Bitmap *temp = nullptr) const;

    /**
     * \brief Up- or down-sample this image to a different resolution
     *
     * This version is similar to the above \ref resample() function -- the
     * main difference is that it does not work with preallocated bitmaps and
     * takes the desired output resolution as first argument.
     *
     * Uses the provided reconstruction filter and accounts for the requested
     * horizontal and vertical boundary conditions when looking up data outside
     * of the input domain.
     *
     * A minimum and maximum image value can be specified to prevent to prevent
     * out-of-range values that are created by the resampling process.
     *
     * \param res
     *     Desired output resolution
     *
     * \param rfilter
     *     A separable image reconstruction filter (default: 2-lobe Lanczos filter)
     *
     * \param bch
     *     Horizontal and vertical boundary conditions (default: clamp)
     *
     * \param clamp
     *     Filtered image pixels will be clamped to the following
     *     range. Default: -infinity..infinity (i.e. no clamping is used)
     */
    ref<Bitmap> resample(const Vector2s &res,
                         const ReconstructionFilter *rfilter = nullptr,
                         const std::pair<EBoundaryCondition, EBoundaryCondition> &bc =
                             { EBoundaryCondition::EClamp, EBoundaryCondition::EClamp },
                         const std::pair<Float, Float> &bound =
                             { -math::Infinity, math::Infinity }) const;

    /**
     * \brief Convert the bitmap into another pixel and/or component format
     *
     * This helper function can be used to efficiently convert a bitmap
     * between different underlying representations. For instance, it can
     * translate a uint8 sRGB bitmap to a linear float32 XYZ bitmap
     * based on half-, single- or double-precision floating point-backed storage.
     *
     * This function roughly does the following:
     *
     * <ul>
     * <li>For each pixel and channel, it converts the associated value
     *   into a normalized linear-space form (any gamma of the source
     *   bitmap is removed)</li>
     * <li>The multiplier and gamma correction specified in
     *      \c targetGamma is applied</li>
     * <li>The corrected value is clamped against the representable range
     *   of the desired component format.</li>
     * <li>The clamped gamma-corrected value is then written to
     *   the new bitmap</li>
     *
     * If the pixel formats differ, this function will also perform basic
     * conversions (e.g. spectrum to rgb, luminance to uniform spectrum
     * values, etc.)
     *
     * Note that the alpha channel is assumed to be linear in both
     * the source and target bitmap, hence it won't be affected by
     * any gamma-related transformations.
     *
     * \remark This <tt>convert()</tt> variant usually returns a new
     * bitmap instance. When the conversion would just involve copying
     * the original bitmap, the function becomes a no-op and returns
     * the current instance.
     *
     * \ref pixel_format
     *      Specifies the desired pixel format
     * \ref component_format
     *      Specifies the desired component format
     * \ref gamma
     *      Specifies the desired gamma value.
     *      Special values: \c 1.0 denotes a linear space, and
     *      \c -1.0 corresponds to sRGB.
     */
    ref<Bitmap> convert(EPixelFormat pixel_format,
                        Struct::EType component_format,
                        bool srgb_gamma = true) const;

    void convert(Bitmap *target) const;

    /**
     * \brief Accumulate the contents of another bitmap into the
     * region with the specified offset
     *
     * Out-of-bounds regions are safely ignored. It is assumed that
     * <tt>bitmap != this</tt>.
     *
     * \remark This function throws an exception when the bitmaps
     * use different component formats or channels, or when the
     * component format is \ref EBitmask.
     */
    void accumulate(const Bitmap *bitmap,
                    Point2i source_offset,
                    Point2i target_offset,
                    Vector2i size);

    /**
     * \brief Accumulate the contents of another bitmap into the
     * region with the specified offset
     *
     * This convenience function calls the main <tt>accumulate()</tt>
     * implementation with <tt>size</tt> set to <tt>bitmap->size()</tt>
     * and <tt>source_offset</tt> set to zero. Out-of-bounds regions are
     * ignored. It is assumed that <tt>bitmap != this</tt>.
     *
     * \remark This function throws an exception when the bitmaps
     * use different component formats or channels, or when the
     * component format is \ref EBitmask.
     */
    void accumulate(const Bitmap *bitmap, const Point2i &target_offset) {
        accumulate(bitmap, Point2i(0), target_offset, bitmap->size());
    }

    /**
     * \brief Accumulate the contents of another bitmap into the
     * region with the specified offset
     *
     * This convenience function calls the main <tt>accumulate()</tt>
     * implementation with <tt>size</tt> set to <tt>bitmap->size()</tt>
     * and <tt>source_offset</tt> and <tt>target_offset</tt>tt> set to zero.
     * Out-of-bounds regions are ignored. It is assumed
     * that <tt>bitmap != this</tt>.
     *
     * \remark This function throws an exception when the bitmaps
     * use different component formats or channels, or when the
     * component format is \ref EBitmask.
     */
    void accumulate(const Bitmap *bitmap) {
        accumulate(bitmap, Point2i(0), Point2i(0), bitmap->size());
    }

    /// Vertically flip the bitmap
    void vflip();

    /// Equality comparison operator
    bool operator==(const Bitmap &bitmap) const;

    /// Inequality comparison operator
    bool operator!=(const Bitmap &bitmap) const { return !operator==(bitmap); }

    /// Return a human-readable summary of this bitmap
    virtual std::string to_string() const override;

    /// Return a pointer to the underlying data
    uint8_t *uint8_data() { return m_data.get(); }

    /// Return a pointer to the underlying data (const)
    const uint8_t *uint8_data() const { return m_data.get(); }

    /// Static initialization of bitmap-related data structures (thread pools, etc.)
    static void static_initialization();

    /// Free the resources used by static_initialization()
    static void static_shutdown();

    MTS_DECLARE_CLASS()

 protected:
     /// Protected destructor
     virtual ~Bitmap();

     /// Rebuild the 'm_struct' field based on the pixel format etc.
     void rebuild_struct(size_t channel_count = 0);

     /// Read a file from a stream
     void read(Stream *stream, EFileFormat format);

     /// Read a file encoded using the OpenEXR file format
     void read_openexr(Stream *stream);

     /// Write a file using the OpenEXR file format
     void write_openexr(Stream *stream, int compression = -1) const;

     /// Read a file encoded using the JPEG file format
     void read_jpeg(Stream *stream);

     /// Save a file using the JPEG file format
     void write_jpeg(Stream *stream, int quality) const;

     /// Read a file encoded using the PNG file format
     void read_png(Stream *stream);

     /// Save a file using the PNG file format
     void write_png(Stream *stream, int quality) const;

     /// Read a file encoded using the PPM file format
     void read_ppm(Stream *stream);

     /// Save a file using the PPM file format
     void write_ppm(Stream *stream) const;

     /// Read a file encoded using the BMP file format
     void read_bmp(Stream *stream);

     /// Read a file encoded using the TGA file format
     void read_tga(Stream *stream);

     /// Read a file encoded using the RGBE file format
     void read_rgbe(Stream *stream);

     /// Save a file using the RGBE file format
     void write_rgbe(Stream *stream) const;

     /// Read a file encoded using the PFM file format
     void read_pfm(Stream *stream);

     /// Save a file using the PFM file format
     void write_pfm(Stream *stream) const;
 protected:
     std::unique_ptr<uint8_t[], enoki::aligned_deleter> m_data;
     EPixelFormat m_pixel_format;
     Struct::EType m_component_format;
     Vector2s m_size;
     ref<Struct> m_struct;
     bool m_srgb_gamma;
     bool m_owns_data;
     Properties m_metadata;
};

extern MTS_EXPORT_CORE std::ostream &operator<<(std::ostream &os, Bitmap::EPixelFormat value);
extern MTS_EXPORT_CORE std::ostream &operator<<(std::ostream &os, Bitmap::EFileFormat value);

NAMESPACE_END(mitsuba)
