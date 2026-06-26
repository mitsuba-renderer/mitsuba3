#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>
#include <mutex>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _texture-bitmap:

Bitmap texture (:monosp:`bitmap`)
---------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the bitmap to be loaded

 * - bitmap
   - :monosp:`Bitmap object`
   - When creating a Bitmap texture at runtime, e.g. from Python or C++,
     an existing Bitmap image instance can be passed directly rather than
     loading it from the filesystem with :paramtype:`filename`.

 * - data
   - |tensor|
   - Tensor array containing the texture data. Similarly to the
     :paramtype:`bitmap` parameter, this field can only be used at runtime. The
     :paramtype:`raw` parameter must also be set to :monosp:`true`.
   - |exposed|, |differentiable|

 * - filter_type
   - |string|
   - Specifies how pixel values are interpolated and filtered when queried over larger
     UV regions. The following options are currently available:

     - ``bilinear`` (default): perform bilinear interpolation, but no filtering.

     - ``nearest``: disable filtering and interpolation. In this mode, the plugin
       performs nearest neighbor lookups of texture values.

 * - wrap_mode
   - |string|
   - Controls the behavior of texture evaluations that fall outside of the
     :math:`[0, 1]` range. The following options are currently available:

     - ``repeat`` (default): tile the texture infinitely.

     - ``mirror``: mirror the texture along its boundaries.

     - ``clamp``: clamp coordinates to the edge of the texture.

 * - format
   - |string|
   - Specifies the underlying texture storage format. The following options are
     currently available:

     - ``auto`` (default): Match the storage precision to the source: use 8 bits
         per channel for 8-bit images, half precision for 16-bit images, and
         otherwise the native floating point representation of the Mitsuba
         variant. For variants using a spectral color representation this option
         is the same as `variant`. Note that 8-bit storage is not differentiable;
         request ``float16`` or ``variant`` to optimize such textures.

     - ``variant``: Use the corresponding native floating point representation
         of the Mitsuba variant

     - ``float16``: Store the texture in half precision

     - ``uint8``: Store the texture using 8 bits per channel. When the source
         image is sRGB-encoded and :paramtype:`raw` is :monosp:`false`, the
         values are linearized on each lookup. This mode is the most memory
         efficient, but not that the texture is *not* differentiable. It is also
         incompatible with spectral variants of Mitsuba.

 * - raw
   - |bool|
   - Should the transformation to the stored color data (e.g. sRGB to linear,
     spectral upsampling) be disabled? You will want to enable this when working
     with bitmaps storing normal maps that use a linear encoding. (Default: false)

 * - to_uv
   - |transform|
   - Specifies an optional 3x3 transformation matrix that will be applied to UV
     values. A 4x4 matrix can also be provided, in which case the extra row and
     column are ignored.
   - |exposed|

 * - accel
   - |bool|
   - Hardware acceleration features can be used in CUDA mode. These features can
     cause small differences as hardware interpolation methods typically have a
     loss of precision (not exactly 32-bit arithmetic). (Default: true)

This plugin provides a bitmap texture that performs interpolated lookups given
a JPEG, PNG, OpenEXR, RGBE, TGA, or BMP input file.

When loading the plugin, the data is first converted into a usable color representation
for the renderer:

* In :monosp:`rgb` modes, sRGB textures are converted into a linear color space.
* In :monosp:`spectral` modes, sRGB textures are *spectrally upsampled* to plausible
  smooth spectra :cite:`Jakob2019Spectral` and stored an intermediate representation
  that enables efficient queries at render time.
* In :monosp:`monochrome` modes, sRGB textures are converted to grayscale.

These conversions can alternatively be disabled with the :paramtype:`raw` flag,
e.g. when textured data is already in linear space or does not represent colors
at all.

.. tabs::
    .. code-tab:: xml
        :name: bitmap-texture

        <texture type="bitmap">
            <string name="filename" value="texture.png"/>
            <string name="wrap_mode" value="mirror"/>
        </texture>

    .. code-tab:: python

        'type': 'bitmap',
        'filename': 'texture.png',
        'wrap_mode': 'mirror'

*/

// Forward declaration of specialized bitmap texture
template <typename Float, typename Spectrum, typename StoredType>
class BitmapTextureImpl;

NAMESPACE_BEGIN(detail)
/// Class name tagged with the storage precision (for diagnostics / RTTI), since
/// it depends on a template parameter that MI_DECLARE_CLASS() cannot capture.
template <typename StoredType>
constexpr const char *bitmap_class_name() {
    using StoredScalar = dr::scalar_t<StoredType>;
    if constexpr (std::is_same_v<StoredScalar, double>)
        return "BitmapTextureImpl[float64]";
    else if constexpr (std::is_same_v<StoredScalar, float>)
        return "BitmapTextureImpl[float32]";
    else if constexpr (std::is_same_v<StoredScalar, dr::half>)
        return "BitmapTextureImpl[float16]";
    else if constexpr (std::is_same_v<StoredScalar, uint8_t>)
        return "BitmapTextureImpl[uint8]";
    else
        return "BitmapTextureImpl[?]";
}
NAMESPACE_END(detail)

template <typename Float, typename Spectrum>
class BitmapTexture final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    /// Storage precision of the texture (resolved from the `format` property)
    enum class Format {
        Auto,     ///< 8-bit for 8-bit sources, half for 16-bit, native otherwise
        Variant,  ///< The variant's native floating point precision
        Float16,  ///< Half precision
        UInt8     ///< 8 bits per channel (optionally sRGB-decoded on lookup)
    };

    BitmapTexture(const Properties &props) : Texture(props) {
        m_transform = props.get<ScalarAffineTransform3f>("to_uv", ScalarAffineTransform3f());

        // Should Mitsuba disable transformations to the stored color data?
        // (e.g. sRGB to linear, spectral upsampling, etc.)
        m_raw = props.get<bool>("raw", false);
        m_accel = props.get<bool>("accel", true);

        // Filter mode
        {
            std::string_view filter_mode_str = props.get<std::string_view>("filter_type", "bilinear");
            if (filter_mode_str == "nearest")
                m_filter_mode = dr::FilterMode::Nearest;
            else if (filter_mode_str == "bilinear")
                m_filter_mode = dr::FilterMode::Linear;
            else
                Throw("Invalid filter type \"%s\", must be one of: \"nearest\", or "
                      "\"bilinear\"!", filter_mode_str);
        }

        // Wrap mode
        {
            std::string_view wrap_mode_str = props.get<std::string_view>("wrap_mode", "repeat");
            if (wrap_mode_str == "repeat")
                m_wrap_mode = dr::WrapMode::Repeat;
            else if (wrap_mode_str == "mirror")
                m_wrap_mode = dr::WrapMode::Mirror;
            else if (wrap_mode_str == "clamp")
                m_wrap_mode = dr::WrapMode::Clamp;
            else
                Throw("Invalid wrap mode \"%s\", must be one of: \"repeat\", "
                      "\"mirror\", or \"clamp\"!", wrap_mode_str);
        }

        // Format
        {
            std::string_view format_str = props.get<std::string_view>("format", "auto");
            if (format_str == "auto")
                m_format = Format::Auto;
            else if (format_str == "variant")
                m_format = Format::Variant;
            else if (format_str == "float16" || format_str == "fp16")
                m_format = Format::Float16; // "fp16" kept for backwards compat
            else if (format_str == "uint8")
                m_format = Format::UInt8;
            else
                Throw("Invalid format \"%s\", must be one of: \"auto\", "
                      "\"variant\", \"float16\", or \"uint8\"!", format_str);

            if constexpr (is_spectral_v<Spectrum>)
                if (m_format == Format::UInt8)
                    Throw("format=\"uint8\" is not supported in spectral variants "
                          "(8-bit storage cannot hold spectral upsampling "
                          "coefficients).");
        }

        // Store
        {
            if (props.has_property("bitmap")) {
                // Creates a Bitmap texture directly from an existing Bitmap
                if (props.has_property("filename"))
                    Throw("Cannot specify both \"bitmap\" and \"filename\".");
                Log(Debug, "Loading bitmap texture from memory...");
                // Note: ref-counted, so we don't have to worry about lifetime
                ref<Object> other = props.get<ref<Object>>("bitmap");
                Bitmap *b = dynamic_cast<Bitmap *>(other.get());
                if (!b)
                    Throw("Property \"bitmap\" must be a Bitmap instance.");
                m_bitmap = b;
            } else if (props.has_property("filename")) {
                // Creates a Bitmap texture by loading an image from the filesystem
                FileResolver* fs = file_resolver();
                fs::path file_path = fs->resolve(props.get<std::string_view>("filename"));
                m_name = file_path.filename().string();
                Log(Debug, "Loading bitmap texture from \"%s\" ..", m_name);
                m_bitmap = new Bitmap(file_path);
            } else if (props.has_property("data")) {
                m_tensor = std::move(const_cast<TensorXf&>(props.get_any<TensorXf>("data")));
                if (m_tensor.ndim() != 3)
                    Throw("Bitmap raw tensor has dimension %lu, expected 3",
                        m_tensor.ndim());

                size_t channel_count = m_tensor.shape(2);
                if (channel_count != 1 && channel_count != 3)
                    Throw("Unsupported tensor channel count: %d"
                          "(expected 1 or 3)", channel_count);
            }
        }
    }

    std::vector<ref<Object>> expand() const override {
        return { ref<Object>(expand_impl()) };
    }

    MI_DECLARE_CLASS(BitmapTexture)

protected:
    Object *expand_impl() const {
        // The `data` tensor path: native float storage, already linear
        if (!m_bitmap)
            return instantiate<Float>(std::move(m_tensor), /* srgb = */ false);

        // Pick a storage precision and instantiate the matching implementation
        switch (resolve_format()) {
            case Format::UInt8:
                return load_bitmap<dr::replace_scalar_t<Float, uint8_t>>();
            case Format::Float16:
                return load_bitmap<dr::replace_scalar_t<Float, dr::half>>();
            default: // Format::Variant
                return load_bitmap<Float>();
        }
    }

    /**
     * \brief Resolve \ref Format::Auto to a concrete storage precision
     *
     * ``auto`` matches the storage to the source bit depth: 8-bit sources are
     * stored losslessly as 8-bit (and decoded from sRGB on lookup), 16-bit
     * sources as half precision, and everything else at the variant's native
     * precision. Spectral variants always use the native precision required for
     * upsampling.
     */
    Format resolve_format() const {
        if (m_format != Format::Auto)
            return m_format;
        if constexpr (is_spectral_v<Spectrum>) {
            return Format::Variant;
        } else {
            size_t bytes_per_channel =
                m_bitmap->bytes_per_pixel() / m_bitmap->channel_count();
            if (bytes_per_channel == 1)
                return Format::UInt8;
            else if (bytes_per_channel == 2)
                return Format::Float16;
            else
                return Format::Variant;
        }
    }

    /**
     * \brief Load the bitmap into a tensor of the given storage type and build
     * the implementation object
     *
     * ``StoredType`` fixes the storage precision; the sRGB strategy follows from
     * it and the source encoding: 8-bit storage keeps the sRGB-encoded bytes and
     * lets the texture decode them on lookup, while float/half storage is
     * decoded to linear here, at load time.
     */
    template <typename StoredType> Object *load_bitmap() const {
        using StoredScalar = dr::scalar_t<StoredType>;
        constexpr bool IsUInt8 = std::is_same_v<StoredScalar, uint8_t>;

        // `raw` data carries no color transform: treat it as already linear
        if (m_raw)
            m_bitmap->set_srgb_gamma(false);

        Bitmap::PixelFormat pf = target_pixel_format(m_bitmap->pixel_format());
        bool srgb = IsUInt8 && m_bitmap->srgb_gamma();

        // Bring the bitmap into the storage format (skipped when already matching)
        m_bitmap = prepare_bitmap(pf, struct_type_v<StoredScalar>,
                                  /* keep_srgb_gamma = */ srgb);

        // Spectral variants store smooth-spectrum coefficients (float/half only)
        if constexpr (is_spectral_v<Spectrum>)
            if (!m_raw)
                upsample_spectral<StoredScalar>(m_bitmap.get());

        ScalarVector2i res(m_bitmap->size());
        size_t shape[3] = { (size_t) res.y(), (size_t) res.x(),
                            m_bitmap->channel_count() };
        dr::replace_scalar_t<TensorXf, StoredScalar> tensor(m_bitmap->data(), 3,
                                                            shape);

        return instantiate<StoredType>(std::move(tensor), srgb);
    }

    /**
     * \brief Bring the source bitmap into the exact format the texture needs
     *
     * The (potentially expensive) \ref Bitmap::convert() is skipped entirely
     * when the bitmap already matches the target pixel format, component type,
     * and gamma -- e.g. an sRGB 8-bit PNG stored as ``uint8``. Sub-2x2 images are
     * up-sampled so that bilinear interpolation has at least one cell.
     */
    ref<Bitmap> prepare_bitmap(Bitmap::PixelFormat pf, sj::Type ct,
                               bool keep_srgb_gamma) const {
        ref<Bitmap> bitmap = m_bitmap;

        if (bitmap->pixel_format()     != pf ||
            bitmap->component_format() != ct ||
            bitmap->srgb_gamma()       != keep_srgb_gamma)
            bitmap = bitmap->convert(pf, ct, keep_srgb_gamma);

        if (dr::any(bitmap->size() < 2)) {
            Log(Warn, "Image must be at least 2x2 pixels in size, up-sampling..");
            bitmap = bitmap->pad_to(ScalarVector2u(2));
        }
        return bitmap;
    }

    /// Map the source pixel format to the 1- or 3-channel layout the texture
    /// stores (alpha is dropped and XYZ converted to RGB by \ref convert()).
    Bitmap::PixelFormat target_pixel_format(Bitmap::PixelFormat pf) const {
        switch (pf) {
            case Bitmap::PixelFormat::Y:
            case Bitmap::PixelFormat::YA:
                return Bitmap::PixelFormat::Y;
            case Bitmap::PixelFormat::RGB:
            case Bitmap::PixelFormat::RGBA:
            case Bitmap::PixelFormat::XYZ:
            case Bitmap::PixelFormat::XYZA:
                return Bitmap::PixelFormat::RGB;
            default:
                Throw("The texture needs a known pixel format "
                      "(Y[A], RGB[A], XYZ[A] are supported).");
        }
    }

    /// Construct the concrete \ref BitmapTextureImpl for the chosen storage type
    template <typename StoredType, typename Tensor>
    Object *instantiate(Tensor &&tensor, bool srgb) const {
        Properties props;
        return new BitmapTextureImpl<Float, Spectrum, StoredType>(
            props, m_name, m_transform, m_filter_mode, m_wrap_mode, m_raw,
            m_accel, srgb, std::forward<Tensor>(tensor));
    }

private:
    /// Convert linear RGB pixels to smooth-spectrum coefficients in place
    template <typename StoredScalar> void upsample_spectral(Bitmap *bitmap) const {
        if (bitmap->channel_count() != 3)
            return;
        StoredScalar *ptr = (StoredScalar *) bitmap->data();
        size_t pixel_count = bitmap->pixel_count();
        for (size_t i = 0; i < pixel_count; ++i, ptr += 3) {
            ScalarColor3f rgb((float) ptr[0], (float) ptr[1], (float) ptr[2]);
            ScalarColor3f coeff = srgb_model_fetch(rgb);
            ptr[0] = (StoredScalar) coeff[0];
            ptr[1] = (StoredScalar) coeff[1];
            ptr[2] = (StoredScalar) coeff[2];
        }
    }

    Format m_format;

    bool m_accel;
    bool m_raw;
    ScalarAffineTransform3f m_transform;
    std::string m_name;
    dr::FilterMode m_filter_mode;
    dr::WrapMode m_wrap_mode;
    mutable ref<Bitmap> m_bitmap;
    TensorXf m_tensor;

    MI_TRAVERSE_CB(Texture, m_bitmap, m_tensor)
};

template <typename Float, typename Spectrum, typename StoredType>
class BitmapTextureImpl final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    using StoredScalar           = dr::scalar_t<StoredType>;
    using StoredColor3f          = Color<StoredType, 3>;
    using StoredTensorXf         = dr::replace_scalar_t<TensorXf, StoredScalar>;
    using StoredTexture2f        = dr::Texture<StoredType, 2>;
    static constexpr bool IsUInt8 = std::is_same_v<StoredScalar, uint8_t>;

    template <typename Tensor>
    BitmapTextureImpl(const Properties &props,
                      const std::string& name,
                      const ScalarAffineTransform3f& transform,
                      dr::FilterMode filter_mode,
                      dr::WrapMode wrap_mode,
                      bool raw,
                      bool accel,
                      bool srgb,
                      Tensor&& tensor) :
        Texture(props),
        m_name(name),
        m_transform(transform),
        m_raw(raw),
        m_srgb(srgb) {

        /* Compute the mean without migrating texture data, i.e. avoid the
           m_texture.tensor() call that would trigger a migration. On CUDA we
           ideally keep the data solely as a GPU texture. */
        rebuild_internals(tensor, true, false);

        m_texture = StoredTexture2f(std::forward<Tensor>(tensor), accel, accel,
                                    filter_mode, wrap_mode, srgb);
    }

    void traverse(TraversalCallback *cb) override {
        // 8-bit textures store integers and are therefore not differentiable
        cb->put("data", m_texture.tensor(),
                IsUInt8 ? ParamFlags::NonDifferentiable
                        : ParamFlags::Differentiable);
        cb->put("to_uv", m_transform, ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (keys.empty() || string::contains(keys, "data")) {
            const size_t channels = m_texture.channel_count();
            if (channels != 1 && channels != 3)
                Throw("parameters_changed(): The bitmap texture %s was changed "
                      "to have %d channels, only textures with 1 or 3 channels "
                      "are supported!",
                      to_string(), channels);
            else if (m_texture.shape()[0] < 2 || m_texture.shape()[1] < 2)
                Throw("parameters_changed(): The bitmap texture %s was changed,"
                      " it must be at least 2x2 pixels in size!",
                      to_string());

            m_texture.update_inplace();
            rebuild_internals(m_texture.tensor(), true, m_distr2d != nullptr);
        }
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.channel_count();
        if (channels == 3 && is_spectral_v<Spectrum> && m_raw)
            Throw("eval(): The bitmap texture %s was queried for a spectrum, "
                  "but texture conversion into spectra was explicitly "
                  "disabled! (raw=true)",
                  to_string());

        if (dr::none_or<false>(active))
            return dr::zeros<UnpolarizedSpectrum>();

        if constexpr (is_monochromatic_v<Spectrum>) {
            // Identical to eval_1() in this variant
            return eval_1(si, active);
        } else {
            if (channels == 1)
                return interpolate_1(si, active);
            else if constexpr (is_spectral_v<Spectrum>)
                return interpolate_spectral(si, active);
            else
                return interpolate_3(si, active);
        }
    }

    Float eval_1(const SurfaceInteraction3f &si,
                 Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.channel_count();
        if (stores_spectral_coeffs(channels))
            Throw("eval_1(): The bitmap texture %s was queried for a "
                  "monochromatic value, but texture conversion to color "
                  "spectra had previously been requested! (raw=false)",
                  to_string());

        if (dr::none_or<false>(active))
            return dr::zeros<Float>();

        if (channels == 1)
            return interpolate_1(si, active);
        else // 3 channels
            return luminance(interpolate_3(si, active));
    }

    Vector2f eval_1_grad(const SurfaceInteraction3f &si,
                         Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.channel_count();
        if (stores_spectral_coeffs(channels))
            Throw(
                "eval_1_grad(): The bitmap texture %s was queried for a "
                "monochromatic gradient value, but texture conversion to color "
                "spectra had previously been requested! (raw=false)",
                to_string());

        if (dr::none_or<false>(active))
            return dr::zeros<Vector2f>();

        // The gradient of a nearest-neighbor lookup is zero almost everywhere
        if (m_texture.filter_mode() != dr::FilterMode::Linear)
            return Vector2f(0.f);

        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform * si.uv;

        Float f00, f10, f01, f11;
        if (channels == 1) {
            using Data1 = dr::Array<Float, 1>;
            dr::Array<Data1, 4> c =
                m_texture.template eval_fetch<Data1>(uv, active);
            f00 = c[0].x(); f10 = c[1].x();
            f01 = c[2].x(); f11 = c[3].x();
        } else { // 3 channels
            dr::Array<Color3f, 4> c =
                m_texture.template eval_fetch<Color3f>(uv, active);
            f00 = luminance(c[0]);
            f10 = luminance(c[1]);
            f01 = luminance(c[2]);
            f11 = luminance(c[3]);
        }

        BilinearWeights bw = bilinear_weights(uv);
        const Point2f &w0 = bw.w0, &w1 = bw.w1;

        // Partials w.r.t. pixel coordinate x and y
        Vector2f df_xy{
            dr::fmadd(w0.y(), f10 - f00, w1.y() * (f11 - f01)),
            dr::fmadd(w0.x(), f01 - f00, w1.x() * (f11 - f10))
        };

        // Partials w.r.t. u and v (include uv transform by transpose multiply)
        Matrix3f uv_tm = m_transform.matrix;
        Vector2f df_uv{ uv_tm.entry(0, 0) * df_xy.x() +
                            uv_tm.entry(1, 0) * df_xy.y(),
                        uv_tm.entry(0, 1) * df_xy.x() +
                            uv_tm.entry(1, 1) * df_xy.y() };
        return resolution() * df_uv;
    }

    Color3f eval_3(const SurfaceInteraction3f &si,
                   Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.channel_count();
        if (channels != 3)
            Throw("eval_3(): The bitmap texture %s was queried for a RGB "
                  "value, but it is monochromatic!",
                  to_string());
        if (stores_spectral_coeffs(channels))
            Throw("eval_3(): The bitmap texture %s was queried for a RGB "
                  "value, but texture conversion to color spectra had "
                  "previously been requested! (raw=false)",
                  to_string());

        if (dr::none_or<false>(active))
            return dr::zeros<Color3f>();

        return interpolate_3(si, active);
    }

    std::pair<Point2f, Float>
    sample_position(const Point2f &sample, Mask active = true) const override {
        if (dr::none_or<false>(active))
            return { dr::zeros<Point2f>(), dr::zeros<Float>() };

        if (!m_distr2d)
            init_distr();

        auto [pos, pdf, sample2] = m_distr2d->sample(sample, active);

        ScalarVector2i res = resolution();
        ScalarVector2f inv_resolution = dr::rcp(ScalarVector2f(res));

        if (m_texture.filter_mode() == dr::FilterMode::Nearest) {
            sample2 = (Point2f(pos) + sample2) * inv_resolution;
        } else {
            sample2 = (Point2f(pos) + 0.5f + warp::square_to_tent(sample2)) *
                      inv_resolution;

            switch (m_texture.wrap_mode()) {
                case dr::WrapMode::Repeat:
                    sample2[sample2 < 0.f] += 1.f;
                    sample2[sample2 > 1.f] -= 1.f;
                    break;

                /* Texel sampling is restricted to [0, 1] and only interpolation
                   with one row/column of pixels beyond that is considered, so
                   both clamp/mirror effectively use the same strategy. No such
                   distinction is needed for the pdf() method. */
                case dr::WrapMode::Clamp:
                case dr::WrapMode::Mirror:
                    sample2[sample2 < 0.f] = -sample2;
                    sample2[sample2 > 1.f] = 2.f - sample2;
                    break;
            }
        }

        return { sample2, pdf_position(sample2) };
    }

    Float pdf_position(const Point2f &pos_, Mask active = true) const override {
        if (dr::none_or<false>(active))
            return dr::zeros<Float>();

        if (!m_distr2d)
            init_distr();

        ScalarVector2i res = resolution();
        if (m_texture.filter_mode() == dr::FilterMode::Linear) {
            BilinearWeights bw = bilinear_weights(pos_);
            const Vector2i &uv_i = bw.i;
            const Point2f &w0 = bw.w0, &w1 = bw.w1;

            Float v00 = m_distr2d->pdf(m_texture.wrap(uv_i + Point2i(0, 0)),
                                       active),
                  v10 = m_distr2d->pdf(m_texture.wrap(uv_i + Point2i(1, 0)),
                                       active),
                  v01 = m_distr2d->pdf(m_texture.wrap(uv_i + Point2i(0, 1)),
                                       active),
                  v11 = m_distr2d->pdf(m_texture.wrap(uv_i + Point2i(1, 1)),
                                       active);

            Float v0 = dr::fmadd(w0.x(), v00, w1.x() * v10),
                  v1 = dr::fmadd(w0.x(), v01, w1.x() * v11);

            return dr::fmadd(w0.y(), v0, w1.y() * v1) * dr::prod(res);
        } else {
            // Scale to bitmap resolution, no shift
            Point2f uv = pos_ * res;

            // Integer pixel positions for nearest-neighbor interpolation
            Vector2i uv_i = m_texture.wrap(dr::floor2int<Vector2i>(uv));

            return m_distr2d->pdf(uv_i, active) * dr::prod(res);
        }
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &_si, const Wavelength &sample,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureSample, active);

        if (dr::none_or<false>(active))
            return { dr::zeros<Wavelength>(), dr::zeros<UnpolarizedSpectrum>() };

        if constexpr (is_spectral_v<Spectrum>) {
            SurfaceInteraction3f si(_si);
            si.wavelengths = MI_CIE_MIN + (MI_CIE_MAX - MI_CIE_MIN) * sample;
            return { si.wavelengths,
                     eval(si, active) * (MI_CIE_MAX - MI_CIE_MIN) };
        } else {
            DRJIT_MARK_USED(sample);
            UnpolarizedSpectrum value = eval(_si, active);
            return { dr::empty<Wavelength>(), value };
        }
    }

    ScalarVector2i resolution() const override {
        const size_t *shape = m_texture.shape();
        return { (int) shape[1], (int) shape[0] };
    }

    Float mean() const override { return m_mean; }

    bool is_spatially_varying() const override { return true; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "BitmapTexture[" << std::endl
            << "  name = \"" << m_name << "\"," << std::endl
            << "  resolution = \"" << resolution() << "\"," << std::endl
            << "  raw = " << (int) m_raw << "," << std::endl
            << "  mean = " << m_mean << "," << std::endl
            << "  transform = " << string::indent(m_transform) << std::endl
            << "]";
        return oss.str();
    }

    static constexpr const char *ClassName = detail::bitmap_class_name<StoredType>();
    std::string_view class_name() const override { return ClassName; }

protected:
    /**
     * \brief Do the stored values represent spectral upsampling coefficients
     * rather than plain RGB?
     *
     * This is the case for a 3-channel texture in a spectral variant with color
     * conversion enabled (\c raw=false). Such textures can only answer spectral
     * queries; monochromatic and RGB lookups are rejected.
     */
    bool stores_spectral_coeffs(size_t channels) const {
        return is_spectral_v<Spectrum> && !m_raw && channels == 3;
    }

    /// Interpolation weights (and base texel) of a bilinear lookup
    struct BilinearWeights {
        Vector2i i;        ///< Lower-left integer texel coordinate
        Point2f w0, w1;    ///< Weights toward the lower / upper texel
    };

    /**
     * \brief Compute the bilinear interpolation weights for a texture-space
     * coordinate
     *
     * Applies the half-texel shift between the UV and texel-center conventions,
     * then returns the lower-left integer texel and the interpolation weights.
     */
    BilinearWeights bilinear_weights(const Point2f &uv) const {
        Point2f p    = dr::fmadd(uv, resolution(), -0.5f);
        Vector2i i   = dr::floor2int<Vector2i>(p);
        Point2f w1   = p - Point2f(i),
                w0   = 1.f - w1;
        return { i, w0, w1 };
    }

    /**
     * \brief Evaluates the texture at the given surface interaction using
     * spectral upsampling
     */
    MI_INLINE UnpolarizedSpectrum
    interpolate_spectral(const SurfaceInteraction3f &si, Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform * si.uv;

        if (m_texture.filter_mode() == dr::FilterMode::Linear) {
            // Fetch the four enclosing texels and spectrally upsample each
            // *before* interpolating. Upsampling is nonlinear, so blending the
            // RGB values first (e.g. via m_texture.eval()) would be incorrect.
            dr::Array<Color3f, 4> v =
                m_texture.template eval_fetch<Color3f>(uv, active);

            UnpolarizedSpectrum c00, c10, c01, c11, c0, c1;
            c00 = srgb_model_eval<UnpolarizedSpectrum>(v[0], si.wavelengths);
            c10 = srgb_model_eval<UnpolarizedSpectrum>(v[1], si.wavelengths);
            c01 = srgb_model_eval<UnpolarizedSpectrum>(v[2], si.wavelengths);
            c11 = srgb_model_eval<UnpolarizedSpectrum>(v[3], si.wavelengths);

            BilinearWeights bw = bilinear_weights(uv);
            const Point2f &w0 = bw.w0, &w1 = bw.w1;

            c0 = dr::fmadd(w0.x(), c00, w1.x() * c10);
            c1 = dr::fmadd(w0.x(), c01, w1.x() * c11);

            return dr::fmadd(w0.y(), c0, w1.y() * c1);
        } else {
            Color3f out = m_texture.template eval<Color3f>(uv, active);

            return srgb_model_eval<UnpolarizedSpectrum>(out, si.wavelengths);
        }
    }

    /**
     * \brief Evaluates the texture at the given surface interaction
     *
     * Should only be used when the texture has exactly 1 channel.
     */
    MI_INLINE Float interpolate_1(const SurfaceInteraction3f &si,
                                   Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform * si.uv;

        using Data1 = dr::Array<Float, 1>;
        return m_texture.template eval<Data1>(uv, active).x();
    }

    /**
     * \brief Evaluates the texture at the given surface interaction
     *
     * Should only be used when the texture has exactly 3 channels.
     */
    MI_INLINE Color3f interpolate_3(const SurfaceInteraction3f &si,
                                     Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform * si.uv;

        return m_texture.template eval<Color3f>(uv, active);
    }

    /**
     * \brief Decode a raw stored value to linear, mirroring the texture's own
     * sampling-time conversion
     *
     * For 8-bit storage this normalizes to [0, 1] and undoes the sRGB curve (if
     * enabled); for float/half storage it is a plain cast resolved at compile
     * time. Used to derive the mean and 2D distribution from the stored tensor.
     * Works for a scalar value or a Dr.Jit array of stored values alike.
     */
    template <typename T>
    MI_INLINE dr::replace_scalar_t<T, ScalarFloat> decode(const T &v) const {
        using Result = dr::replace_scalar_t<T, ScalarFloat>;
        if constexpr (IsUInt8) {
            Result f = Result(v) * ScalarFloat(1.0 / 255.0);
            return m_srgb ? dr::srgb_to_linear(f) : f;
        } else {
            return Result(v);
        }
    }

    /**
     * \brief Recompute mean and 2D sampling distribution (if requested)
     * following an update
     */
    void rebuild_internals(const StoredTensorXf& tensor, bool init_mean, bool init_distr) {
        if (m_transform != ScalarAffineTransform3f())
            dr::make_opaque(m_transform);

        const dr::vector<size_t> &shape = tensor.shape();
        size_t pixel_count = shape[0] * shape[1],
               channels    = shape[2];

        bool range_issue = false;
        using FloatStorage = DynamicBuffer<Float>;
        FloatStorage values;

        if constexpr (dr::is_jit_v<Float>) {
            if (channels == 3) {
                StoredColor3f stored = dr::gather<StoredColor3f>(
                    tensor.array(), dr::arange<UInt32>(pixel_count));
                Color<FloatStorage, 3> c3(decode(stored.x()), decode(stored.y()),
                                          decode(stored.z()));

                if (is_spectral_v<Spectrum> && !m_raw)
                    values = srgb_model_mean(c3);
                else
                    values = luminance(c3);
            } else {
                values = decode(tensor.array());
            }

            if (init_mean)
                m_mean = dr::mean(values);
            if (!m_raw)
                range_issue = dr::any(values < 0 || values > 1);
        } else {
            // Reduce each texel to a single value (luminance, or spectral mean)
            // in one pass, accumulating the average, a [0, 1] range check, and —
            // when requested — the per-texel values backing the 2D distribution.
            StoredScalar *ptr = (StoredScalar *) tensor.data();
            ScalarFloat *out = nullptr;
            if (init_distr) {
                values = dr::empty<FloatStorage>(pixel_count);
                out = values.data();
            }

            auto reduce = [&](auto texel_value) {
                ScalarFloat mean = 0;
                for (size_t i = 0; i < pixel_count; ++i) {
                    ScalarFloat v = texel_value(ptr);
                    if (out)
                        *out++ = v;
                    mean += v;
                    range_issue |= v < 0 || v > 1;
                }
                m_mean = mean / pixel_count;
            };

            if (channels == 3) {
                reduce([&](StoredScalar *&p) {
                    Color3f c3(decode(p[0]), decode(p[1]), decode(p[2]));
                    p += 3;
                    if (is_spectral_v<Spectrum> && !m_raw)
                        return ScalarFloat(srgb_model_mean(c3));
                    return ScalarFloat(luminance(c3));
                });
            } else {
                reduce([&](StoredScalar *&p) {
                    return ScalarFloat(decode(*p++));
                });
            }
        }

        if (init_distr) {
            auto&& data = dr::migrate(values, JitBackend::None);

            if constexpr (dr::is_jit_v<Float>)
                dr::sync_thread();

            m_distr2d = std::make_unique<DiscreteDistribution2D<Float>>(
                data.data(), resolution());
        }

        if (!m_raw && range_issue)
            Log(Warn,
                "BitmapTexture: texture named \"%s\" contains pixels that "
                "exceed the [0, 1] range!",
                m_name);
    }

    /// Construct 2D distribution upon first access, avoid races
    MI_INLINE void init_distr() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_distr2d) {
            dr::scoped_disable_symbolic<Float> guard{};
            auto self = const_cast<BitmapTextureImpl *>(this);
            self->rebuild_internals(m_texture.tensor(), false, true);
        }
    }

protected:
    std::string m_name;
    ScalarAffineTransform3f m_transform;
    bool m_raw;
    bool m_srgb;
    Float m_mean;
    StoredTexture2f m_texture;

    // Optional: distribution for importance sampling
    mutable std::mutex m_mutex;
    std::unique_ptr<DiscreteDistribution2D<Float>> m_distr2d;

    MI_TRAVERSE_CB(Texture, m_mean, m_texture, m_distr2d)
};

MI_EXPORT_PLUGIN(BitmapTexture)

NAMESPACE_END(mitsuba)
