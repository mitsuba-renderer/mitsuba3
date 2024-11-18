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

     - ``variant``(default): Use the corresponding native floating point representation
         of the Mitsuba variant

     - ``fp16``: Forcibly store the texture in half precision

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

template <typename Float, typename Spectrum>
class BitmapTexture final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    BitmapTexture(const Properties &props) : Texture(props) {
        m_transform = props.get<ScalarTransform3f>("to_uv", ScalarTransform3f());

        /* Should Mitsuba disable transformations to the stored color data?
           (e.g. sRGB to linear, spectral upsampling, etc.) */
        m_raw = props.get<bool>("raw", false);
        m_accel = props.get<bool>("accel", true);

        // Filter mode
        {
            std::string filter_mode_str = props.string("filter_type", "bilinear");
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
            std::string wrap_mode_str = props.string("wrap_mode", "repeat");
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
            std::string format_str = props.string("format", "variant");
            if (format_str == "variant")
                m_format = Format::Variant;
            else if (format_str == "fp16")
                m_format = Format::Float16;
            else
                Throw("Invalid format \"%s\", must be one of: "
                      "\"variant\", or \"fp16\"!", format_str);
        }

        // Store
        {
            if (props.has_property("bitmap")) {
                // Creates a Bitmap texture directly from an existing Bitmap
                if (props.has_property("filename"))
                    Throw("Cannot specify both \"bitmap\" and \"filename\".");
                Log(Debug, "Loading bitmap texture from memory...");
                // Note: ref-counted, so we don't have to worry about lifetime
                ref<Object> other = props.object("bitmap");
                Bitmap *b = dynamic_cast<Bitmap *>(other.get());
                if (!b)
                    Throw("Property \"bitmap\" must be a Bitmap instance.");
                m_bitmap = b;
            } else if (props.has_property("filename")) {
                // Creates a Bitmap texture by loading an image from the filesystem
                FileResolver* fs = Thread::thread()->file_resolver();
                fs::path file_path = fs->resolve(props.string("filename"));
                m_name = file_path.filename().string();
                Log(Debug, "Loading bitmap texture from \"%s\" ..", m_name);
                m_bitmap = new Bitmap(file_path);
            } else if (props.has_property("data")) {
                m_tensor = props.tensor<TensorXf>("data");
                if (m_tensor->ndim() != 3)
                    Throw("Bitmap raw tensor has dimension %lu, expected 3",
                        m_tensor->ndim());

                const size_t channel_count = m_tensor->shape(2);
                if (channel_count != 1 && channel_count != 3)
                    Throw("Unsupported tensor channel count: %d"
                          "(expected 1 or 3)", channel_count);
            }
        }
    }

    std::vector<ref<Object>> expand() const override {
        return { ref<Object>(expand_1()) };
    }

    MI_DECLARE_CLASS()

protected:
    Object* expand_1() const {
        if (m_bitmap) {
            Format format = m_format;

// TODO: Temporarily disable this as LLVM FP16 gather/scatter operations are costly
#if 0
            // Format auto means we store texture as FP16 when possible.
            // Skip this conversion for spectral variants as we want to perform
            // spectral upsampling in the variant's native FP representation
            if constexpr (!is_spectral_v<Spectrum>) {
                size_t bytes_p_ch = m_bitmap->bytes_per_pixel()
                    / m_bitmap->channel_count();
                if (m_format == Format::Auto && bytes_p_ch <= 2)
                    format = Format::Float16;
            }
#endif

            if (format == Format::Float16)
                return expand_bitmap<dr::replace_scalar_t<Float, dr::half>>();
            else
               return expand_bitmap<Float>();
        }

        // Otherwise, initializing using tensor
        Properties props;
        TensorXf tensor(*m_tensor);
        return new BitmapTextureImpl<Float, Spectrum, Float>(
            props,
            m_name,
            m_transform,
            m_filter_mode,
            m_wrap_mode,
            m_raw,
            m_accel,
            tensor);
    }

    template <typename StoredType> Object* expand_bitmap() const {
        using StoredScalar           = dr::scalar_t<StoredType>;
        using StoredTensorXf         = dr::replace_scalar_t<TensorXf, StoredScalar>;

        /* Convert to linear RGB float bitmap, will be converted
           into spectral profile coefficients below (in place) */
        Bitmap::PixelFormat pixel_format = m_bitmap->pixel_format();
        switch (pixel_format) {
            case Bitmap::PixelFormat::Y:
            case Bitmap::PixelFormat::YA:
                pixel_format = Bitmap::PixelFormat::Y;
                break;

            case Bitmap::PixelFormat::RGB:
            case Bitmap::PixelFormat::RGBA:
            case Bitmap::PixelFormat::XYZ:
            case Bitmap::PixelFormat::XYZA:
                pixel_format = Bitmap::PixelFormat::RGB;
                break;

            default:
                Throw("The texture needs to have a known pixel "
                      "format (Y[A], RGB[A], XYZ[A] are supported).");
        }

        if (m_raw) {
            /* Don't undo gamma correction in the conversion below.
               This is needed, e.g., for normal maps. */
            m_bitmap->set_srgb_gamma(false);
        }

        // Convert the image into the working floating point representation
        m_bitmap =
            m_bitmap->convert(pixel_format, struct_type_v<StoredScalar>, false);

        if (dr::any(m_bitmap->size() < 2)) {
            Log(Warn,
                "Image must be at least 2x2 pixels in size, up-sampling..");
            using ReconstructionFilter = Bitmap::ReconstructionFilter;
            ref<ReconstructionFilter> rfilter =
                PluginManager::instance()->create_object<ReconstructionFilter>(
                    Properties("tent"));
            m_bitmap =
                m_bitmap->resample(dr::maximum(m_bitmap->size(), 2), rfilter);
        }

        if (is_spectral_v<Spectrum> && !m_raw)
            convert_spectral<StoredScalar>();

        size_t channels = m_bitmap->channel_count();
        ScalarVector2i res = ScalarVector2i(m_bitmap->size());
        size_t shape[3] = { (size_t) res.y(), (size_t) res.x(), channels };
        StoredTensorXf tensor = StoredTensorXf(m_bitmap->data(), 3, shape);

        Properties props;
        return new BitmapTextureImpl<Float, Spectrum, StoredType>(
            props,
            m_name,
            m_transform,
            m_filter_mode,
            m_wrap_mode,
            m_raw,
            m_accel,
            tensor);
    }

private:
    /// Convert RGB values to spectral coefficients and store them
    template <typename StoredScalar> void convert_spectral() const {
        StoredScalar *ptr = (StoredScalar*) m_bitmap->data();
        size_t pixel_count = m_bitmap->pixel_count();

        if (m_bitmap->channel_count() == 3) {
            for (size_t i = 0; i < pixel_count; ++i) {
                ScalarColor3f value = dr::load<ScalarColor3f>(ptr);
                value = srgb_model_fetch(value);
                dr::store(ptr, value);
                ptr += 3;
            }
        }
    }

    enum class Format {
        Variant,
        Float16
    } m_format;

    bool m_accel;
    bool m_raw;
    ScalarTransform3f m_transform;
    std::string m_name;
    dr::FilterMode m_filter_mode;
    dr::WrapMode m_wrap_mode;
    mutable ref<Bitmap> m_bitmap;
    TensorXf* m_tensor;
};

template <typename Float, typename Spectrum, typename StoredType>
class BitmapTextureImpl : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    using StoredScalar           = dr::scalar_t<StoredType>;
    using StoredColor3f          = Color<StoredType, 3>;
    using StoredTensorXf         = dr::replace_scalar_t<TensorXf, StoredScalar>;
    using StoredTexture2f        = dr::Texture<StoredType, 2>;

    BitmapTextureImpl(const Properties &props,
            const std::string& name,
            const ScalarTransform3f& transform,
            dr::FilterMode filter_mode,
            dr::WrapMode wrap_mode,
            bool raw,
            bool accel,
            StoredTensorXf& tensor) :
        Texture(props),
        m_name(name),
        m_transform(transform),
        m_accel(accel),
        m_raw(raw),
        m_texture(tensor, accel, accel, filter_mode, wrap_mode) {

        /* Compute mean without migrating texture data
           i.e. Avoid call to m_texture.tensor() that triggers migration.
           For CUDA-variants, ideally want to solely keep data as CUDA texture
        */
        rebuild_internals(tensor, true, false);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("data",  m_texture.tensor(), +ParamFlags::Differentiable);
        callback->put_parameter("to_uv", m_transform,        +ParamFlags::NonDifferentiable);
    }

    void
    parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (keys.empty() || string::contains(keys, "data")) {
            const size_t channels = m_texture.shape()[2];
            if (channels != 1 && channels != 3)
                Throw("parameters_changed(): The bitmap texture %s was changed "
                      "to have %d channels, only textures with 1 or 3 channels "
                      "are supported!",
                      to_string(), channels);
            else if (m_texture.shape()[0] < 2 || m_texture.shape()[1] < 2)
                Throw("parameters_changed(): The bitmap texture %s was changed,"
                      " it must be at least 2x2 pixels in size!",
                      to_string());

            m_texture.set_tensor(m_texture.tensor());
            rebuild_internals(m_texture.tensor(), true, m_distr2d != nullptr);
        }
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.shape()[2];
        if (channels == 3 && is_spectral_v<Spectrum> && m_raw) {
            DRJIT_MARK_USED(si);
            Throw("The bitmap texture %s was queried for a spectrum, but "
                  "texture conversion into spectra was explicitly disabled! "
                  "(raw=true)",
                  to_string());
        } else {
            if (dr::none_or<false>(active))
                return dr::zeros<UnpolarizedSpectrum>();

            if constexpr (is_monochromatic_v<Spectrum>) {
                if (channels == 1)
                    return interpolate_1(si, active);
                else // 3 channels
                    return luminance(interpolate_3(si, active));
            }
            else{
                if (channels == 1)
                    return interpolate_1(si, active);
                else { // 3 channels
                    if constexpr (is_spectral_v<Spectrum>)
                        return interpolate_spectral(si, active);
                    else
                        return interpolate_3(si, active);
                }
            }
        }
    }

    Float eval_1(const SurfaceInteraction3f &si,
                 Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.shape()[2];
        if (channels == 3 && is_spectral_v<Spectrum> && !m_raw) {
            DRJIT_MARK_USED(si);
            Throw("eval_1(): The bitmap texture %s was queried for a "
                  "monochromatic value, but texture conversion to color "
                  "spectra had previously been requested! (raw=false)",
                  to_string());
        } else {
            if (dr::none_or<false>(active))
                return dr::zeros<Float>();

            if (channels == 1)
                return interpolate_1(si, active);
            else // 3 channels
                return luminance(interpolate_3(si, active));
        }
    }

    Vector2f eval_1_grad(const SurfaceInteraction3f &si,
                         Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.shape()[2];
        if (channels == 3 && is_spectral_v<Spectrum> && !m_raw) {
            DRJIT_MARK_USED(si);
            Throw(
                "eval_1_grad(): The bitmap texture %s was queried for a "
                "monochromatic gradient value, but texture conversion to color "
                "spectra had previously been requested! (raw=false)",
                to_string());
        } else {
            if (dr::none_or<false>(active))
                return dr::zeros<Vector2f>();

            if (m_texture.filter_mode() == dr::FilterMode::Linear) {
                if constexpr (!dr::is_array_v<Mask>)
                    active = true;

                Point2f uv = m_transform.transform_affine(si.uv);

                Float f00, f10, f01, f11;
                if (channels == 1) {
                    dr::Array<Float *, 4> fetch_values;
                    fetch_values[0] = &f00;
                    fetch_values[1] = &f10;
                    fetch_values[2] = &f01;
                    fetch_values[3] = &f11;

                    if (m_accel)
                        m_texture.template eval_fetch<Float>(uv, fetch_values, active);
                    else
                        m_texture.template eval_fetch_nonaccel<Float>(uv, fetch_values, active);
                } else { // 3 channels
                    Color3f v00, v10, v01, v11;
                    dr::Array<Float *, 4> fetch_values;
                    fetch_values[0] = v00.data();
                    fetch_values[1] = v10.data();
                    fetch_values[2] = v01.data();
                    fetch_values[3] = v11.data();

                    if (m_accel)
                        m_texture.template eval_fetch<Float>(uv, fetch_values, active);
                    else
                        m_texture.template eval_fetch_nonaccel<Float>(uv, fetch_values, active);

                    f00 = luminance(v00);
                    f10 = luminance(v10);
                    f01 = luminance(v01);
                    f11 = luminance(v11);
                }

                ScalarVector2i res = resolution();
                uv = dr::fmadd(uv, res, -0.5f);
                Vector2i uv_i = dr::floor2int<Vector2i>(uv);
                Point2f w1 = uv - Point2f(uv_i),
                        w0 = 1.f - w1;

                // Partials w.r.t. pixel coordinate x and y
                Vector2f df_xy{
                    dr::fmadd(w0.y(), f10 - f00, w1.y() * (f11 - f01)),
                    dr::fmadd(w0.x(), f01 - f00, w1.x() * (f11 - f10))
                };

                // Partials w.r.t. u and v (include uv transform by transpose
                // multiply)
                Matrix3f uv_tm = m_transform.matrix;
                Vector2f df_uv{ uv_tm.entry(0, 0) * df_xy.x() +
                                    uv_tm.entry(1, 0) * df_xy.y(),
                                uv_tm.entry(0, 1) * df_xy.x() +
                                    uv_tm.entry(1, 1) * df_xy.y() };
                return res * df_uv;
            }
            // m_filter_type == FilterType::Nearest
            return Vector2f(0.f);
        }
    }

    Color3f eval_3(const SurfaceInteraction3f &si,
                   Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.shape()[2];
        if (channels != 3) {
            DRJIT_MARK_USED(si);
            Throw("eval_3(): The bitmap texture %s was queried for a RGB "
                  "value, but it is monochromatic!",
                  to_string());
        } else if (is_spectral_v<Spectrum> && !m_raw) {
            DRJIT_MARK_USED(si);
            Throw("eval_3(): The bitmap texture %s was queried for a RGB "
                  "value, but texture conversion to color spectra had "
                  "previously been requested! (raw=false)",
                  to_string());
        } else {
            if (dr::none_or<false>(active))
                return dr::zeros<Color3f>();

            return interpolate_3(si, active);
        }
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

        return { sample2, pdf * dr::prod(res) };
    }

    Float pdf_position(const Point2f &pos_, Mask active = true) const override {
        if (dr::none_or<false>(active))
            return dr::zeros<Float>();

        if (!m_distr2d)
            init_distr();

        ScalarVector2i res = resolution();
        if (m_texture.filter_mode() == dr::FilterMode::Linear) {
            // Scale to bitmap resolution and apply shift
            Point2f uv = dr::fmadd(pos_, res, -.5f);

            // Integer pixel positions for bilinear interpolation
            Vector2i uv_i = dr::floor2int<Vector2i>(uv);

            // Interpolation weights
            Point2f w1 = uv - Point2f(uv_i),
                    w0 = 1.f - w1;

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

    MI_DECLARE_CLASS()

protected:
    /**
     * \brief Evaluates the texture at the given surface interaction using
     * spectral upsampling
     */
    MI_INLINE UnpolarizedSpectrum
    interpolate_spectral(const SurfaceInteraction3f &si, Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform.transform_affine(si.uv);

        if (m_texture.filter_mode() == dr::FilterMode::Linear) {
            Color3f v00, v10, v01, v11;
            dr::Array<Float *, 4> fetch_values;
            fetch_values[0] = v00.data();
            fetch_values[1] = v10.data();
            fetch_values[2] = v01.data();
            fetch_values[3] = v11.data();

            if (m_accel)
                m_texture.template eval_fetch<Float>(uv, fetch_values, active);
            else
                m_texture.template eval_fetch_nonaccel<Float>(uv, fetch_values, active);

            UnpolarizedSpectrum c00, c10, c01, c11, c0, c1;
            c00 = srgb_model_eval<UnpolarizedSpectrum>(v00, si.wavelengths);
            c10 = srgb_model_eval<UnpolarizedSpectrum>(v10, si.wavelengths);
            c01 = srgb_model_eval<UnpolarizedSpectrum>(v01, si.wavelengths);
            c11 = srgb_model_eval<UnpolarizedSpectrum>(v11, si.wavelengths);

            ScalarVector2i res = resolution();
            uv = dr::fmadd(uv, res, -.5f);
            Vector2i uv_i = dr::floor2int<Vector2i>(uv);

            // Interpolation weights
            Point2f w1 = uv - Point2f(uv_i), w0 = 1.f - w1;

            c0 = dr::fmadd(w0.x(), c00, w1.x() * c10);
            c1 = dr::fmadd(w0.x(), c01, w1.x() * c11);

            return dr::fmadd(w0.y(), c0, w1.y() * c1);
        } else {
            Color3f out;
            if (m_accel)
                m_texture.template eval<Float>(uv, out.data(), active);
            else
                m_texture.template eval_nonaccel<Float>(uv, out.data(), active);

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

        Point2f uv = m_transform.transform_affine(si.uv);

        Float out;
        if (m_accel)
            m_texture.template eval<Float>(uv, &out, active);
        else
            m_texture.template eval_nonaccel<Float>(uv, &out, active);

        return out;
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

        Point2f uv = m_transform.transform_affine(si.uv);

        Color3f out;
        if (m_accel)
            m_texture.template eval<Float>(uv, out.data(), active);
        else
            m_texture.template eval_nonaccel<Float>(uv, out.data(), active);

        return out;
    }

    /**
     * \brief Recompute mean and 2D sampling distribution (if requested)
     * following an update
     */
    void rebuild_internals(const StoredTensorXf& tensor, bool init_mean, bool init_distr) {
        if (m_transform != ScalarTransform3f())
            dr::make_opaque(m_transform);

        size_t pixel_count = (size_t) dr::prod(resolution());
        const size_t channels = m_texture.shape()[2];

        using FloatStorage = DynamicBuffer<Float>;
        FloatStorage values;

        if (channels == 3) {
            Color<FloatStorage, 3> colors_fl;

            if constexpr (dr::is_jit_v<Float>) {
                StoredColor3f colors = dr::gather<StoredColor3f>(
                    tensor.array(),
                    dr::arange<UInt32>(pixel_count));

                // Potentially upcast values before attempting to compute mean
                colors_fl = colors;
            } else {
                StoredScalar* ptr = (StoredScalar*) tensor.data();
                DynamicBuffer<UInt32> index
                    = dr::arange<DynamicBuffer<UInt32>>(0, pixel_count) * 3;

                using StoredTypeArray= DynamicBuffer<StoredType>;
                auto colors = Color<StoredTypeArray, 3>(
                    dr::gather<StoredTypeArray>(ptr, index + 0),
                    dr::gather<StoredTypeArray>(ptr, index + 1),
                    dr::gather<StoredTypeArray>(ptr, index + 2));

                // Potentially upcast values before attempting to compute mean
                colors_fl = colors;
            }

            if (is_spectral_v<Spectrum> && !m_raw)
                values = srgb_model_mean(colors_fl);
            else
                values = luminance(colors_fl);
        } else {
            if constexpr (dr::is_jit_v<Float>)
                values = tensor.array();
            else {
                using StoredTypeArray= DynamicBuffer<StoredType>;
                values = dr::load<StoredTypeArray>(tensor.data(), pixel_count);
            }
        }

        if (init_mean) {
            m_mean = dr::mean(values);
        }

        if (init_distr) {
            auto&& data = dr::migrate(values, AllocType::Host);

            if constexpr (dr::is_jit_v<Float>)
                dr::sync_thread();

            m_distr2d = std::make_unique<DiscreteDistribution2D<Float>>(
                data.data(), resolution());
        }

        if (!m_raw && any(values < 0 || values > 1))
            Log(Warn,
                "BitmapTexture: texture named \"%s\" contains pixels that "
                "exceed the [0, 1] range!",
                m_name);
    }

    /// Construct 2D distribution upon first access, avoid races
    MI_INLINE void init_distr() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_distr2d) {
            dr::scoped_symbolic_independence<Float> guard{};
            auto self = const_cast<BitmapTextureImpl *>(this);
            self->rebuild_internals(m_texture.tensor(), false, true);
        }
    }

protected:
    std::string m_name;
    ScalarTransform3f m_transform;
    bool m_accel;
    bool m_raw;
    Float m_mean;
    StoredTexture2f m_texture;

    // Optional: distribution for importance sampling
    mutable std::mutex m_mutex;
    std::unique_ptr<DiscreteDistribution2D<Float>> m_distr2d;
};

MI_IMPLEMENT_CLASS_VARIANT(BitmapTexture, Texture)
MI_EXPORT_PLUGIN(BitmapTexture, "Bitmap texture")

/* This class has a name that depends on extra template parameters, so
   the standard MI_IMPLEMENT_CLASS_VARIANT macro cannot be used */

NAMESPACE_BEGIN(detail)
template <typename StoredType>
constexpr const char * bitmap_class_name() {
    if constexpr (std::is_same_v<dr::scalar_t<StoredType>, dr::half>)
        return "BitmapTextureImpl_FP16";

    return "BitmapTextureImpl";
}
NAMESPACE_END(detail)

template <typename Float, typename Spectrum, typename StoredType>
Class *BitmapTextureImpl<Float, Spectrum, StoredType>::m_class
    = new Class(detail::bitmap_class_name<StoredType>(), "Texture",
                ::mitsuba::detail::get_variant<Float, Spectrum>(), nullptr, nullptr);

template <typename Float, typename Spectrum, typename StoredType>
const Class* BitmapTextureImpl<Float, Spectrum, StoredType>::class_() const {
    return m_class;
}

NAMESPACE_END(mitsuba)
