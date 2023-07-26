#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/core/spline.h>
#include <mitsuba/core/math.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>
#include <mutex>


NAMESPACE_BEGIN(mitsuba)

/**!

.. _texture-color ramp:

ColorRamp texture (:monosp:`color_ramp`)
---------------------------------

.. pluginparameters::
 :extra-rows: 10

 * - filename
   - |string|
   - Filename of the bitmap to be loaded

 * - bitmap
   - :monosp:`Bitmap object`
   - When creating a Bitmap texture at runtime, e.g. from Python or C++,
     an existing Bitmap image instance can be passed directly rather than
     loading it from the filesystem with :paramtype:`filename`.

 * - filter_type
   - |string|
   - Specifies how pixel values are interpolated and filtered when queried over larger
     UV regions. The following options are currently available:

     - ``bilinear`` (default): perform bilinear interpolation, but no filtering.

     - ``nearest``: disable filtering and interpolation. In this mode, the plugin
       performs nearest neighbor lookups of texture values.

 * - num_band
   - |integer|
   - Specifies the number of color bands used in ColorRamp

* - posx
   - |float|
   - Specifies the position of color bands used in ColorRamp
   - x starts from 0 to num_band - 1
   - Used for interpolation factor

* - colorx
   - |Color3f|
   - Specifies the 3 channels of color bands used in ColorRamp
   - Used for interpolation color results

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

 * - data
   - |tensor|
   - Tensor array containing the texture data.
   - |exposed|, |differentiable|

This plugin provides an interpolator based on the bitmap texture factor. It currently supports RGB and monochrome modes.
The interpolation for Spectral mode will be added.

.. tabs::
    .. code-tab:: xml
        :name: color-ramp

        <texture type="color_ramp">
            <string name="filename" value="texture.png"/>
            <string name="mode" value="linear"/>
            <float name="pos0" value="0.040000"/>
            <rgb value="0.602237 0.482636 0.000000" name="color0"/>
            <float name="pos1" value="0.084091"/>
            <rgb value="0.019626 0.077920 0.174928" name="color1"/>
        </texture>

    .. code-tab:: python

        'type': 'color_ramp',
        'filename': 'texture.png',
        "mode": "linear",
        "num_band": 2,
        "pos0": 0.0,
        "pos1": 1.0,
        "color0": {
            'type': 'rgb',
            'value': [0.0, 0.0, 0.0]
        },
        "color1": {
            'type': 'rgb',
            'value': [1.0, 1.0, 1.0]
        },

*/

template <typename Float, typename Spectrum>
class ColorRamp final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture, TexturePtr)

    ColorRamp(const Properties &props) : Texture(props) {
        m_num_band = int(props.get<ScalarFloat>("num_band"));

        std::unique_ptr<ScalarFloat[]> m_pos(new ScalarFloat[m_num_band]);
        ScalarFloat *m_pos_ptr = m_pos.get();

        for (int i=0; i < m_num_band; i++) {
            m_color_band.push_back(props.texture<Texture>("color" + std::to_string(i), 0.0f));
            m_pos_ptr[i] = props.get<ScalarFloat>("pos" + std::to_string(i));
        }

        m_color_band_dr = dr::load<DynamicBuffer<TexturePtr>>(m_color_band.data(), m_color_band.size());
        m_pos_dr = dr::load<DynamicBuffer<Float>>(m_pos.get(), m_num_band);

        m_mode = props.string("mode");

        if (props.has_property("factor")) { // Single factor with multiple color (array) interpolation

            if (props.has_property("filename"))
                Throw("Bitmap factor and single factor for ColorRamp can not be used at the same time!");

            // Factor type
            m_bitmap_factor = props.has_property("filename");

            // Single factor
            m_factor = props.get<ScalarFloat>("factor");

        } else if (props.has_property("filename")) {
            // Factor type
            m_bitmap_factor = props.has_property("filename");

            m_transform = props.get<ScalarTransform4f>("to_uv", ScalarTransform4f()).extract();
            if (m_transform != ScalarTransform3f())
                dr::make_opaque(m_transform);

            if (props.has_property("bitmap")) {
                // Creates a Bitmap texture directly from an existing Bitmap object
                if (props.has_property("filename"))
                    Throw("Cannot specify both \"bitmap\" and \"filename\".");
                Log(Debug, "Loading bitmap texture from memory...");
                // Note: ref-counted, so we don't have to worry about lifetime
                ref<Object> other = props.object("bitmap");
                Bitmap *b = dynamic_cast<Bitmap *>(other.get());
                if (!b)
                    Throw("Property \"bitmap\" must be a Bitmap instance.");
                m_bitmap = b;
            } else {
                // Creates a Bitmap texture by loading an image from the filesystem
                FileResolver* fs = Thread::thread()->file_resolver();
                fs::path file_path = fs->resolve(props.string("filename"));
                m_name = file_path.filename().string();
                Log(Debug, "Loading bitmap texture from \"%s\" ..", m_name);
                m_bitmap = new Bitmap(file_path);
            }

            std::string filter_mode_str = props.string("filter_type", "bilinear");
            dr::FilterMode filter_mode;
            if (filter_mode_str == "nearest")
                filter_mode = dr::FilterMode::Nearest;
            else if (filter_mode_str == "bilinear")
                filter_mode = dr::FilterMode::Linear;
            else
                Throw("Invalid filter type \"%s\", must be one of: \"nearest\", or "
                      "\"bilinear\"!", filter_mode_str);

            std::string wrap_mode_str = props.string("wrap_mode", "repeat");
            typename dr::WrapMode wrap_mode;
            if (wrap_mode_str == "repeat")
                wrap_mode = dr::WrapMode::Repeat;
            else if (wrap_mode_str == "mirror")
                wrap_mode = dr::WrapMode::Mirror;
            else if (wrap_mode_str == "clamp")
                wrap_mode = dr::WrapMode::Clamp;
            else
                Throw("Invalid wrap mode \"%s\", must be one of: \"repeat\", "
                      "\"mirror\", or \"clamp\"!", wrap_mode_str);

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

            /* Should Mitsuba disable transformations to the stored color data?
               (e.g. sRGB to linear, spectral upsampling, etc.) */
            m_raw = props.get<bool>("raw", false);
            if (m_raw) {
                /* Don't undo gamma correction in the conversion below.
                   This is needed, e.g., for normal maps. */
                m_bitmap->set_srgb_gamma(false);
            }

            m_accel = props.get<bool>("accel", true);

            // Convert the image into the working floating point representation
            m_bitmap =
                m_bitmap->convert(pixel_format, struct_type_v<ScalarFloat>, false);

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

            ScalarFloat *ptr = (ScalarFloat *) m_bitmap->data();
            size_t pixel_count = m_bitmap->pixel_count();
            bool exceed_unit_range = false;

            double mean = 0.0;
            if (m_bitmap->channel_count() == 3) {
                if (is_spectral_v<Spectrum> && !m_raw) {
                    for (size_t i = 0; i < pixel_count; ++i) {
                        ScalarColor3f value = dr::load<ScalarColor3f>(ptr);
                        if (!all(value >= 0 && value <= 1))
                            exceed_unit_range = true;
                        value = srgb_model_fetch(value);
                        mean += (double) srgb_model_mean(value);
                        dr::store(ptr, value);
                        ptr += 3;
                    }
                } else {
                    for (size_t i = 0; i < pixel_count; ++i) {
                        ScalarColor3f value = dr::load<ScalarColor3f>(ptr);
                        if (!all(value >= 0 && value <= 1))
                            exceed_unit_range = true;
                        mean += (double) luminance(value);
                        ptr += 3;
                    }
                }
            } else if (m_bitmap->channel_count() == 1) {
                for (size_t i = 0; i < pixel_count; ++i) {
                    ScalarFloat value = ptr[i];
                    if (!(value >= 0 && value <= 1))
                        exceed_unit_range = true;
                    mean += (double) value;
                }
            } else {
                Throw("Unsupported channel count: %d (expected 1 or 3)",
                      m_bitmap->channel_count());
            }

            if (exceed_unit_range && !m_raw)
                Log(Warn,
                    "BitmapTexture: texture named \"%s\" contains pixels that "
                    "exceed the [0, 1] range!",
                    m_name);

            m_mean = Float(mean / pixel_count);

            size_t channels = m_bitmap->channel_count();
            ScalarVector2i res = ScalarVector2i(m_bitmap->size());
            size_t shape[3] = { (size_t) res.y(), (size_t) res.x(), channels };
            m_texture = Texture2f(TensorXf(m_bitmap->data(), 3, shape), m_accel, m_accel, filter_mode, wrap_mode);
        } else {
            Throw("Color Ramp should at least have a factor or bitmap as the input!");
        }
    }

    ~ColorRamp() {}

    void traverse(TraversalCallback *callback) override {
        if (m_bitmap_factor) {
            callback->put_parameter("data",  m_texture.tensor(), +ParamFlags::Differentiable);
            callback->put_parameter("to_uv", m_transform,        +ParamFlags::NonDifferentiable);
        }

    }

    void
    parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (m_bitmap_factor) {
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
                rebuild_internals(true, m_distr2d != nullptr);
            }
        }

    }

    // Interpolate RGB
    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (m_bitmap_factor) {
            const size_t channels = m_texture.shape()[2];
            if (channels == 3 && is_spectral_v<Spectrum> && m_raw) {
                DRJIT_MARK_USED(si);
                Throw("The bitmap factor in color ramp %s was queried for a spectrum, but "
                      "texture conversion into spectra was explicitly disabled! "
                      "(raw=true)",
                      to_string());
            } else {
                if (dr::none_or<false>(active))
                    return dr::zeros<UnpolarizedSpectrum>();

                if constexpr (is_monochromatic_v<Spectrum>) {
                    if (channels == 1)
                        return interpolate_1dim_bitmap<Color1f>(si, active);
                    else
                        return interpolate_3dim_bitmap<Color1f>(si, active);
                }
                else if constexpr (is_spectral_v<Spectrum>) {
                    // This function is equivalent to removing ColorRamp node between bitmap texture and bsdf
                    // TODO: Add spectral mode for ColorRamp
                    return interpolate_spectral(si, active);
                }
                else {
                    if (channels == 1)
                        return interpolate_1dim_bitmap<Color3f>(si, active);
                    else
                        return interpolate_3dim_bitmap<Color3f>(si, active);
                }
            }
        }
        else {
            if constexpr (is_monochromatic_v<Spectrum>)
                return interpolate_single_factor<Color1f>(si, active);
            else if constexpr (is_spectral_v<Spectrum>)
                return interpolate_spectral(si, active);
            else
                return interpolate_single_factor<Color3f>(si, active);
        }
    }

    // Interpolate Float properties such as roughness, specular, etc.
    Float eval_1(const SurfaceInteraction3f &si,
                 Mask active = true) const override {

        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if (m_bitmap_factor) {
            const size_t channels = m_texture.shape()[2];
            if (channels == 3 && is_spectral_v<Spectrum> && !m_raw) {
                DRJIT_MARK_USED(si);
                Throw("eval_1(): The bitmap factor in color ramp %s was queried for a spectrum, but "
                      "texture conversion into spectra was explicitly disabled! (raw=true)",
                      to_string());
            } else {
                if (dr::none_or<false>(active))
                    return dr::zeros<Float>();

                if constexpr (is_monochromatic_v<Spectrum>) {
                    if (channels == 1)
                        return interpolate_1dim_bitmap<Color1f>(si, active).r();
                    else
                        return interpolate_3dim_bitmap<Color1f>(si, active).r();
                }
                else if constexpr (is_spectral_v<Spectrum>)
                    return luminance(interpolate_spectral(si, active), si.wavelengths);
                else {
                    if (channels == 1)
                        return luminance(interpolate_1dim_bitmap<Color3f>(si, active));
                    else
                        return luminance(interpolate_3dim_bitmap<Color3f>(si, active));
                }
            }
        } else {
            if constexpr (is_monochromatic_v<Spectrum>)
                return interpolate_single_factor<Color1f>(si, active).r();
            else if constexpr (is_spectral_v<Spectrum>)
                return luminance(interpolate_spectral(si, active), si.wavelengths);
            else
                return luminance(interpolate_single_factor<Color3f>(si, active));
        }

    }

    // no color ramp support for bumpmap.cpp
    Vector2f eval_1_grad(const SurfaceInteraction3f &si,
                         Mask active = true) const override {
        Throw("Color Ramp is not supported for bumpmap surface gradient currently");
    }

    // no color ramp support for normalmap.cpp
    Color3f eval_3(const SurfaceInteraction3f &si,
                   Mask active = true) const override {
        Throw("Color Ramp is not supported for normalmap surface gradient currently");
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

    Float mean() const override {
        return m_mean;
    }

    bool is_spatially_varying() const override { return true; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "ColorRamp[" << std::endl
            << "  name = \"" << m_name << "\"," << std::endl
            << "  resolution = \"" << resolution() << "\"," << std::endl
            << "  raw = " << (int) m_raw << "," << std::endl
            << "  mean = " << m_mean << "," << std::endl
            << "  transform = " << string::indent(m_transform) << std::endl
            << "  number of band = " << string::indent(m_num_band) << std::endl
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
                m_texture.eval_fetch(uv, fetch_values, active);
            else
                m_texture.eval_fetch_nonaccel(uv, fetch_values, active);

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
                m_texture.eval(uv, out.data(), active);
            else
                m_texture.eval_nonaccel(uv, out.data(), active);

            return srgb_model_eval<UnpolarizedSpectrum>(out, si.wavelengths);
        }
    }

    /**
     * \brief Recompute mean and 2D sampling distribution (if requested)
     * following an update
     */
    void rebuild_internals(bool init_mean, bool init_distr) {
        auto&& data = dr::migrate(m_texture.value(), AllocType::Host);

        if constexpr (dr::is_jit_v<Float>)
            dr::sync_thread();

        if (m_transform != ScalarTransform3f())
            dr::make_opaque(m_transform);

        const ScalarFloat *ptr = data.data();

        double mean = 0.0;
        size_t pixel_count = (size_t) dr::prod(resolution());
        bool exceed_unit_range = false;

        const size_t channels = m_texture.shape()[2];
        if (channels == 3) {
            std::unique_ptr<ScalarFloat[]> importance_map(
                init_distr ? new ScalarFloat[pixel_count] : nullptr);

            for (size_t i = 0; i < pixel_count; ++i) {
                ScalarColor3f value = dr::load<ScalarColor3f>(ptr);
                ScalarFloat tmp;
                if (is_spectral_v<Spectrum> && !m_raw ) {
                    tmp = srgb_model_mean(value);
                } else {
                    if (!all(value >= 0 && value <= 1))
                        exceed_unit_range = true;
                    tmp = luminance(value);
                }
                if (init_distr)
                    importance_map[i] = tmp;
                mean += (double) tmp;
                ptr += 3;
            }

            if (init_distr)
                m_distr2d = std::make_unique<DiscreteDistribution2D<Float>>(
                    importance_map.get(), resolution());
        } else {
            for (size_t i = 0; i < pixel_count; ++i) {
                ScalarFloat value = ptr[i];
                if (!(value >= 0 && value <= 1))
                    exceed_unit_range = true;
                mean += (double) value;
            }

            if (init_distr)
                m_distr2d = std::make_unique<DiscreteDistribution2D<Float>>(
                    ptr, resolution());
        }

        if (init_mean)
            m_mean = dr::opaque<Float>(ScalarFloat(mean / pixel_count));

        if (exceed_unit_range && !m_raw)
            Log(Warn,
                "ColorRampBitmap: texture named \"%s\" contains pixels that "
                "exceed the [0, 1] range!",
                m_name);
    }

    // Construct 2D distribution upon first access, avoid races
    MI_INLINE void init_distr() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_distr2d) {
            auto self = const_cast<ColorRamp *>(this);
            self->rebuild_internals(false, true);
        }
    }

    // Calculate the weight of cardinal and B-spline interpolation
    // Borrow from Blender
    MI_INLINE void key_curve_position_weights(Float t, Float *data, std::string type) const {
        if(type == "cardinal") {
            Float t2 = t * t;
            Float t3 = t2 * t;
            Float fc = 0.71f;

            data[0] = -fc * t3 + 2.0f * fc * t2 - fc * t;
            data[1] = (2.0f - fc) * t3 + (fc - 3.0f) * t2 + 1.0f;
            data[2] = (fc - 2.0f) * t3 + (3.0f - 2.0f * fc) * t2 + fc * t;
            data[3] = fc * t3 - fc * t2;
        }
        else if(type == "b_spline") {
             Float t2= t * t;
             Float t3= t2 * t;

             data[0]= -0.16666666f * t3 + 0.5f * t2 - 0.5f * t + 0.16666666f;
             data[1]= 0.5f * t3 - t2 + 0.6666666f;
             data[2]= -0.5f * t3 + 0.5f * t2 + 0.5f * t + 0.16666666f;
             data[3]= 0.16666666f * t3;
        }
        else if (type == "cr_spline") {
            Float t2 = t * t;
            Float t3 = t2 * t;
            Float fc = 0.5f;

            data[0] = -fc * t3 + 2.0f * fc * t2 - fc * t;
            data[1] = (2.0f - fc) * t3 + (fc - 3.0f) * t2 + 1.0f;
            data[2] = (fc - 2.0f) * t3 + (3.0f - 2.0f * fc) * t2 + fc * t;
            data[3] = fc * t3 - fc * t2;
        }
    }

    MI_INLINE void key_curve_tangent_weights(Float t, Float *data, std::string type) const {
        if(type == "cardinal") {
            Float t2 = t * t;
            Float fc = 0.71f;

            data[0] = -3.0f * fc * t2 + 4.0f * fc * t - fc;
            data[1] = 3.0f * (2.0f - fc) * t2 + 2.0f * (fc - 3.0f) * t;
            data[2] = 3.0f * (fc - 2.0f) * t2 + 2.0f * (3.0f - 2.0f * fc) * t + fc;
            data[3] = 3.0f * fc * t2 - 2.0f * fc * t;
        }
        else if(type == "b_spline") {
            Float t2 = t * t;

            data[0] = -0.5f * t2 + t - 0.5f;
            data[1] = 1.5f * t2 - t * 2.0f;
            data[2] = -1.5f * t2 + t + 0.5f;
            data[3] = 0.5f * t2;
        }
        else if (type == "cr_spline") {
            Float t2 = t * t;
            Float fc = 0.5f;

            data[0] = -3.0f * fc * t2 + 4.0f * fc * t - fc;
            data[1] = 3.0f * (2.0f - fc) * t2 + 2.0f * (fc - 3.0f) * t;
            data[2] = 3.0f * (fc - 2.0f) * t2 + 2.0f * (3.0f - 2.0f * fc) * t + fc;
            data[3] = 3.0f * fc * t2 - 2.0f * fc * t;
        }
    }

    MI_INLINE void key_curve_normal_weights(Float t, Float *data, std::string type) const {
        if(type == "cardinal") {
            Float fc = 0.71f;

            data[0] = -6.0f * fc * t + 4.0f * fc;
            data[1] = 6.0f * (2.0f - fc) * t + 2.0f * (fc - 3.0f);
            data[2] = 6.0f * (fc - 2.0f) * t + 2.0f * (3.0f - 2.0f * fc);
            data[3] = 6.0f * fc * t - 2.0f * fc;
        }
        else if(type == "b_spline") {
            data[0] = -1.0f * t + 1.0f;
            data[1] = 3.0f * t - 2.0f;
            data[2] = -3.0f * t + 1.0f;
            data[3] = 1.0f * t;
        }
        else if (type == "cr_spline") {
            Float fc = 0.5f;

            data[0] = -6.0f * fc * t + 4.0f * fc;
            data[1] = 6.0f * (2.0f - fc) * t + 2.0f * (fc - 3.0f);
            data[2] = 6.0f * (fc - 2.0f) * t + 2.0f * (3.0f - 2.0f * fc);
            data[3] = 6.0f * fc * t - 2.0f * fc;
        }
    }

    /**
     * \brief Evaluates the color ramp at the given surface interaction
     *
     * Should only be used when the bitmap has exactly 3 channels.
     */
    template<typename color>
    MI_INLINE color interpolate_3dim_bitmap(const SurfaceInteraction3f &si, Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform.transform_affine(si.uv);

        Color3f factor;

        if (m_accel)
            m_texture.eval(uv, factor.data(), active);
        else
            m_texture.eval_nonaccel(uv, factor.data(), active);

        if (m_mode == "linear") {
            Float fac = luminance(factor);

            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex1= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos0 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = dr::select(dr::neq(pos0, pos1), (fac - pos0) / (pos1 - pos0), 0.);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                tex0->eval(si, active),
                tex0->eval(si, active) * (1 - relative_pos) + tex1->eval(si, active) * relative_pos
            );
        }
        else if (m_mode == "ease") {
            Float fac = luminance(factor);

            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex1= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos0 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = dr::select(dr::neq(pos1, pos0), (luminance(factor) - pos0) / (pos1 - pos0), 0.0);
            Float ease_fac = relative_pos * relative_pos * (3.0 - 2.0 * relative_pos);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) < fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) > fac,
                tex0->eval(si, active),
                tex0->eval(si, active) * (1 - ease_fac) + tex1->eval(si, active) * ease_fac
            );
        }
        else if (m_mode == "constant") {
            Float fac = luminance(factor);

            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );
            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );
            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);

            return dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active)->eval(si, active);
        }
        else if (m_mode == "cardinal") {
            Float fac = luminance(factor);

            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex2= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos2 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex1 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = (luminance(factor) - pos1) / (pos2 - pos1);
            Float weight[4];
            key_curve_position_weights(relative_pos, weight, m_mode);

            UInt32 lower_boundary_left = dr::select(lower_boundary > 0, lower_boundary - 1, lower_boundary);
            lower_boundary_left = dr::clamp(lower_boundary_left, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary_left, active);

            UInt32 higher_boundary_right = dr::select(higher_boundary < UInt32(m_num_band - 1), higher_boundary + 1, higher_boundary);
            higher_boundary_right = dr::clamp(higher_boundary_right, 0, m_num_band - 1);
            TexturePtr tex3 = dr::gather<TexturePtr>(m_color_band_dr, higher_boundary_right, active);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                tex1->eval(si, active),
                tex0->eval(si, active) * weight[0] +
                    tex1->eval(si, active) * weight[1] +
                    tex2->eval(si, active) * weight[2] +
                    tex3->eval(si, active) * weight[3]
            );
        }
        else if (m_mode == "b_spline") {
            Float fac = luminance(factor);

            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex2= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos2 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex1 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = (luminance(factor) - pos1) / (pos2 - pos1);
            Float weight[4];
            key_curve_position_weights(relative_pos, weight, m_mode);

            UInt32 lower_boundary_left = dr::select(lower_boundary > 0, lower_boundary - 1, lower_boundary);
            lower_boundary_left = dr::clamp(lower_boundary_left, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary_left, active);

            UInt32 higher_boundary_right = dr::select(higher_boundary < UInt32(m_num_band - 1), higher_boundary + 1, higher_boundary);
            higher_boundary_right = dr::clamp(higher_boundary_right, 0, m_num_band - 1);
            TexturePtr tex3 = dr::gather<TexturePtr>(m_color_band_dr, higher_boundary_right, active);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                tex1->eval(si, active),
                tex0->eval(si, active) * weight[0] +
                    tex1->eval(si, active) * weight[1] +
                    tex2->eval(si, active) * weight[2] +
                    tex3->eval(si, active) * weight[3]
            );
        }
        else
            Throw("Unknown interpolation type. Please specific interpolation mode (linear, ease, constant, cardinal, b_spline) in ColorRamp!");
    }

    /**
     * \brief Evaluates the color ramp at the given surface interaction
     *
     * Should only be used when the bitmap has exactly 1 channels.
     */
    template<typename color>
    MI_INLINE color interpolate_1dim_bitmap(const SurfaceInteraction3f &si, Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform.transform_affine(si.uv);

        Float fac;

        if (m_accel)
            m_texture.eval(uv, &fac, active);
        else
            m_texture.eval_nonaccel(uv, &fac, active);

        if (m_mode == "linear") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex1= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos0 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = dr::select(dr::neq(pos0, pos1), (fac - pos0) / (pos1 - pos0), 0.);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                tex0->eval(si, active),
                tex0->eval(si, active) * (1 - relative_pos) + tex1->eval(si, active) * relative_pos
            );
        }
        else if (m_mode == "ease") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex1= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos0 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = dr::select(dr::neq(pos1, pos0), (fac - pos0) / (pos1 - pos0), 0.0);
            Float ease_fac = relative_pos * relative_pos * (3.0 - 2.0 * relative_pos);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) < fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) > fac,
                tex0->eval(si, active),
                tex0->eval(si, active) * (1 - ease_fac) + tex1->eval(si, active) * ease_fac
            );
        }
        else if (m_mode == "constant") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);

            return dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active)->eval(si, active);
        }
        else if (m_mode == "cardinal") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex2= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos2 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex1 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = (fac - pos1) / (pos2 - pos1);
            Float weight[4];
            key_curve_position_weights(relative_pos, weight, m_mode);

            UInt32 lower_boundary_left = dr::select(lower_boundary > 0, lower_boundary - 1, lower_boundary);
            lower_boundary_left = dr::clamp(lower_boundary_left, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary_left, active);

            UInt32 higher_boundary_right = dr::select(higher_boundary < UInt32(m_num_band - 1), higher_boundary + 1, higher_boundary);
            higher_boundary_right = dr::clamp(higher_boundary_right, 0, m_num_band - 1);
            TexturePtr tex3 = dr::gather<TexturePtr>(m_color_band_dr, higher_boundary_right, active);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                tex1->eval(si, active),
                tex0->eval(si, active) * weight[0] +
                    tex1->eval(si, active) * weight[1] +
                    tex2->eval(si, active) * weight[2] +
                    tex3->eval(si, active) * weight[3]
            );
        }
        else if (m_mode == "b_spline") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= fac;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex2= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos2 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex1 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = (fac - pos1) / (pos2 - pos1);
            Float weight[4];
            key_curve_position_weights(relative_pos, weight, m_mode);

            UInt32 lower_boundary_left = dr::select(lower_boundary > 0, lower_boundary - 1, lower_boundary);
            lower_boundary_left = dr::clamp(lower_boundary_left, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary_left, active);

            UInt32 higher_boundary_right = dr::select(higher_boundary < UInt32(m_num_band - 1), higher_boundary + 1, higher_boundary);
            higher_boundary_right = dr::clamp(higher_boundary_right, 0, m_num_band - 1);
            TexturePtr tex3 = dr::gather<TexturePtr>(m_color_band_dr, higher_boundary_right, active);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= fac ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= fac,
                tex1->eval(si, active),
                tex0->eval(si, active) * weight[0] +
                    tex1->eval(si, active) * weight[1] +
                    tex2->eval(si, active) * weight[2] +
                    tex3->eval(si, active) * weight[3]
            );
        }
        else
            Throw("Unknown interpolation type. Please specific interpolation mode (linear, ease, constant, cardinal, b_spline) in ColorRamp!");
    }

    /**
     * \brief Evaluates the color ramp at the given surface interaction
     *
     * Should only be used when there is only single factor for interpolation
     */
    template<typename color>
    MI_INLINE color interpolate_single_factor(const SurfaceInteraction3f &si,Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        if (m_mode == "linear") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= m_factor;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex1= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos0 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = dr::select(dr::neq(pos0, pos1), (m_factor - pos0) / (pos1 - pos0), 0.);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                tex0->eval(si, active),
                tex0->eval(si, active) * (1 - relative_pos) + tex1->eval(si, active) * relative_pos
            );
        }
        else if (m_mode == "ease") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= m_factor;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex1= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos0 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = dr::select(dr::neq(pos1, pos0), (m_factor - pos0) / (pos1 - pos0), 0.0);
            Float ease_fac = relative_pos * relative_pos * (3.0 - 2.0 * relative_pos);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) < m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) > m_factor,
                tex0->eval(si, active),
                tex0->eval(si, active) * (1 - ease_fac) + tex1->eval(si, active) * ease_fac
            );
        }
        else if (m_mode == "constant") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= m_factor;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);

            return dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active)->eval(si, active);
        }
        else if (m_mode == "cardinal") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= m_factor;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex2= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos2 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex1 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = (m_factor - pos1) / (pos2 - pos1);
            Float weight[4];
            key_curve_position_weights(relative_pos, weight, m_mode);

            UInt32 lower_boundary_left = dr::select(lower_boundary > 0, lower_boundary - 1, lower_boundary);
            lower_boundary_left = dr::clamp(lower_boundary_left, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary_left, active);

            UInt32 higher_boundary_right = dr::select(higher_boundary < UInt32(m_num_band - 1), higher_boundary + 1, higher_boundary);
            higher_boundary_right = dr::clamp(higher_boundary_right, 0, m_num_band - 1);
            TexturePtr tex3 = dr::gather<TexturePtr>(m_color_band_dr, higher_boundary_right, active);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                tex1->eval(si, active),
                tex0->eval(si, active) * weight[0] +
                    tex1->eval(si, active) * weight[1] +
                    tex2->eval(si, active) * weight[2] +
                    tex3->eval(si, active) * weight[3]
            );
        }
        else if (m_mode == "b_spline") {
            UInt32 higher_boundary = dr::binary_search<UInt32>(
                0, m_num_band - 1, [&](UInt32 idx) DRJIT_INLINE_LAMBDA {
                    return dr::gather<Float>(m_pos_dr, idx, active) <= m_factor;
                }
            );

            higher_boundary = dr::clamp(higher_boundary, 0, m_num_band - 1);
            TexturePtr tex2= dr::gather<TexturePtr>(m_color_band_dr, higher_boundary, active);
            Float pos2 = dr::gather<Float>(m_pos_dr, higher_boundary, active);

            UInt32 lower_boundary = dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                higher_boundary,
                higher_boundary - 1
            );

            lower_boundary = dr::clamp(lower_boundary, 0, m_num_band - 1);
            TexturePtr tex1 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary, active);
            Float pos1 = dr::gather<Float>(m_pos_dr, lower_boundary, active);

            Float relative_pos = (m_factor - pos1) / (pos2 - pos1);
            Float weight[4];
            key_curve_position_weights(relative_pos, weight, m_mode);

            UInt32 lower_boundary_left = dr::select(lower_boundary > 0, lower_boundary - 1, lower_boundary);
            lower_boundary_left = dr::clamp(lower_boundary_left, 0, m_num_band - 1);
            TexturePtr tex0 = dr::gather<TexturePtr>(m_color_band_dr, lower_boundary_left, active);

            UInt32 higher_boundary_right = dr::select(higher_boundary < UInt32(m_num_band - 1), higher_boundary + 1, higher_boundary);
            higher_boundary_right = dr::clamp(higher_boundary_right, 0, m_num_band - 1);
            TexturePtr tex3 = dr::gather<TexturePtr>(m_color_band_dr, higher_boundary_right, active);

            return dr::select(
                dr::gather<Float>(m_pos_dr, UInt32(m_num_band - 1), active) <= m_factor ||
                    dr::gather<Float>(m_pos_dr, UInt32(0), active) >= m_factor,
                tex1->eval(si, active),
                tex0->eval(si, active) * weight[0] +
                    tex1->eval(si, active) * weight[1] +
                    tex2->eval(si, active) * weight[2] +
                    tex3->eval(si, active) * weight[3]
            );
        }
        else
            Throw("Unknown interpolation type. Please specific interpolation mode (linear, ease, constant, cardinal, b_spline) in ColorRamp!");
    }

protected:
    Texture2f m_texture;
    ScalarTransform3f m_transform;
    bool m_accel;
    bool m_raw;
    Float m_mean;
    ref<Bitmap> m_bitmap;
    std::string m_name;

    // Single factor
    ScalarFloat m_factor;
    // Bitmap or color interpolator
    bool m_bitmap_factor;
    // Interpolate mode
    std::string m_mode;

    // Multiple color band
    int m_num_band;
    std::vector<ref<Texture>> m_color_band;
    DynamicBuffer<TexturePtr> m_color_band_dr;
    DynamicBuffer<Float> m_pos_dr;

    // Optional: distribution for importance sampling
    mutable std::mutex m_mutex;
    std::unique_ptr<DiscreteDistribution2D<Float>> m_distr2d;
};

MI_IMPLEMENT_CLASS_VARIANT(ColorRamp, Texture)
MI_EXPORT_PLUGIN(ColorRamp, "Color Ramp")

NAMESPACE_END(mitsuba)