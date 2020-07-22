#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <mutex>
#include <tbb/spin_mutex.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _texture-bitmap:

Bitmap texture (:monosp:`bitmap`)
---------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the bitmap to be loaded

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

*/

enum class FilterType { Nearest, Bilinear };
enum class WrapMode { Repeat, Mirror, Clamp };

// Forward declaration of specialized bitmap texture
template <typename Float, typename Spectrum, uint32_t Channels, bool Raw>
class BitmapTextureImpl;

/// Bilinearly interpolated bitmap texture.
template <typename Float, typename Spectrum>
class BitmapTexture final : public Texture<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Texture)

    BitmapTexture(const Properties &props) : Texture(props) {
        m_transform = props.transform("to_uv", ScalarTransform4f()).extract();

        FileResolver* fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_name = file_path.filename().string();
        Log(Debug, "Loading bitmap texture from \"%s\" ..", m_name);

        std::string filter_type = props.string("filter_type", "bilinear");
        if (filter_type == "nearest")
            m_filter_type = FilterType::Nearest;
        else if (filter_type == "bilinear")
            m_filter_type = FilterType::Bilinear;
        else
            Throw("Invalid filter type \"%s\", must be one of: \"nearest\", or "
                  "\"bilinear\"!", filter_type);

        std::string wrap_mode = props.string("wrap_mode", "repeat");
        if (wrap_mode == "repeat")
            m_wrap_mode = WrapMode::Repeat;
        else if (wrap_mode == "mirror")
            m_wrap_mode = WrapMode::Mirror;
        else if (wrap_mode == "clamp")
            m_wrap_mode = WrapMode::Clamp;
        else
            Throw("Invalid wrap mode \"%s\", must be one of: \"repeat\", "
                  "\"mirror\", or \"clamp\"!", wrap_mode);

        m_bitmap = new Bitmap(file_path);

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
        m_raw = props.bool_("raw", false);
        if (m_raw) {
            /* Don't undo gamma correction in the conversion below.
               This is needed, e.g., for normal maps. */
            m_bitmap->set_srgb_gamma(false);
        }

        // Convert the image into the working floating point representation
        m_bitmap = m_bitmap->convert(pixel_format, struct_type_v<ScalarFloat>, false);

        if (any(m_bitmap->size() < 2)) {
            Log(Warn, "Image must be at least 2x2 pixels in size, up-sampling..");
            using ReconstructionFilter = Bitmap::ReconstructionFilter;
            ref<ReconstructionFilter> rfilter =
                PluginManager::instance()->create_object<ReconstructionFilter>(Properties("tent"));
            m_bitmap = m_bitmap->resample(max(m_bitmap->size(), 2), rfilter);
        }

        ScalarFloat *ptr = (ScalarFloat *) m_bitmap->data();
        size_t pixel_count = m_bitmap->pixel_count();
        bool bad = false;

        double mean = 0.0;
        if (m_bitmap->channel_count() == 3) {
            if (is_spectral_v<Spectrum> && !m_raw) {
                for (size_t i = 0; i < pixel_count; ++i) {
                    ScalarColor3f value = load_unaligned<ScalarColor3f>(ptr);
                    if (!all(value >= 0 && value <= 1))
                        bad = true;
                    value = srgb_model_fetch(value);
                    mean += (double) srgb_model_mean(value);
                    store_unaligned(ptr, value);
                    ptr += 3;
                }
            } else {
                for (size_t i = 0; i < pixel_count; ++i) {
                    ScalarColor3f value = load_unaligned<ScalarColor3f>(ptr);
                    if (!all(value >= 0 && value <= 1))
                        bad = true;
                    mean += (double) luminance(value);
                    ptr += 3;
                }
            }
        } else if (m_bitmap->channel_count() == 1) {
            for (size_t i = 0; i < pixel_count; ++i) {
                ScalarFloat value = ptr[i];
                if (!(value >= 0 && value <= 1))
                    bad = true;
                mean += (double) value;
            }
        } else {
            Throw("Unsupported channel count: %d (expected 1 or 3)",
                  m_bitmap->channel_count());
        }

        if (bad)
            Log(Warn,
                "BitmapTexture: texture named \"%s\" contains pixels that "
                "exceed the [0, 1] range!", m_name);

        m_mean = ScalarFloat(mean / pixel_count);
    }

    /**
     * Recursively expand into an implementation specialized to the
     * actual loaded image.
     */
    std::vector<ref<Object>> expand() const override {
        Properties props;
        props.set_id(this->id());
        return { ref<Object>(expand_1()) };
    }

    MTS_DECLARE_CLASS()

protected:
    Object* expand_1() const {
        return m_bitmap->channel_count() == 1 ? expand_2<1>() : expand_2<3>();
    }

    template <uint32_t Channels> Object* expand_2() const {
        return m_raw ? expand_3<Channels, true>() : expand_3<Channels, false>();
    }

    template <uint32_t Channels, bool Raw> Object* expand_3() const {
        Properties props;
        return new BitmapTextureImpl<Float, Spectrum, Channels, Raw>(
            props, m_bitmap, m_name, m_transform, m_mean, m_filter_type, m_wrap_mode);
    }

protected:
    ref<Bitmap> m_bitmap;
    std::string m_name;
    ScalarTransform3f m_transform;
    bool m_raw;
    ScalarFloat m_mean;
    FilterType m_filter_type;
    WrapMode m_wrap_mode;
};

template <typename Float, typename Spectrum, uint32_t Channels, bool Raw>
class BitmapTextureImpl final : public Texture<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Texture)

    BitmapTextureImpl(const Properties &props,
                      const Bitmap *bitmap,
                      const std::string &name,
                      const ScalarTransform3f &transform,
                      ScalarFloat mean,
                      FilterType filter_type,
                      WrapMode wrap_mode)
        : Texture(props),
          m_resolution(ScalarVector2i(bitmap->size())),
          m_inv_resolution_x((int) bitmap->width()),
          m_inv_resolution_y((int) bitmap->height()),
          m_name(name), m_transform(transform), m_mean(mean),
          m_filter_type(filter_type), m_wrap_mode(wrap_mode){
        m_data = DynamicBuffer<Float>::copy(bitmap->data(),
            hprod(m_resolution) * Channels);
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (Channels == 3 && is_spectral_v<Spectrum> && Raw) {
            ENOKI_MARK_USED(si);
            Throw("The bitmap texture %s was queried for a spectrum, but texture conversion "
                  "into spectra was explicitly disabled! (raw=true)",
                  to_string());
        } else {
            auto result = interpolate(si, active);

            if constexpr (Channels == 3 && is_monochromatic_v<Spectrum>)
                return luminance(result);
            else
                return result;
        }
    }

    Float eval_1(const SurfaceInteraction3f &si, Mask active = true) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (Channels == 3 && is_spectral_v<Spectrum> && !Raw) {
            ENOKI_MARK_USED(si);
            Throw("eval_1(): The bitmap texture %s was queried for a "
                  "monochromatic value, but texture conversion to color "
                  "spectra had previously been requested! (raw=false)",
                  to_string());
        } else {
            auto result = interpolate(si, active);

            if constexpr (Channels == 3)
                return luminance(result);
            else
                return result;
        }
    }

    Vector2f eval_1_grad(const SurfaceInteraction3f& si, Mask active = true) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (Channels == 3 && is_spectral_v<Spectrum> && !Raw) {
            ENOKI_MARK_USED(si);
            Throw("eval_1_grad(): The bitmap texture %s was queried for a "
                  "monochromatic gradient value, but texture conversion to color "
                  "spectra had previously been requested! (raw=false)",
                  to_string());
        }
        else {
            if (m_filter_type == FilterType::Bilinear) {
                // Storage representation underlying this texture
                using StorageType = std::conditional_t<Channels == 1, Float, Color3f>;
                using Int4 = Array<Int32, 4>;
                using Int24 = Array<Int4, 2>;

                if constexpr (!is_array_v<Mask>)
                    active = true;

                Point2f uv = m_transform.transform_affine(si.uv);

                // Scale to bitmap resolution and apply shift
                uv = fmadd(uv, m_resolution, -.5f);

                // Integer pixel positions for bilinear interpolation
                Vector2i uv_i = floor2int<Vector2i>(uv);

                // Interpolation weights
                Point2f w1 = uv - Point2f(uv_i), w0 = 1.f - w1;

                // Apply wrap mode
                Int24 uv_i_w = wrap(Int24(Int4(0, 1, 0, 1) + uv_i.x(),
                                          Int4(0, 0, 1, 1) + uv_i.y()));

                Int4 index = uv_i_w.x() + uv_i_w.y() * m_resolution.x();

                auto convert_to_monochrome = [](const auto& a) {
                    if constexpr (Channels == 3)
                        return luminance(a);
                    else
                        return a;
                };

                Float f00 = convert_to_monochrome(gather<StorageType>(m_data, index.x(), active));
                Float f10 = convert_to_monochrome(gather<StorageType>(m_data, index.y(), active));
                Float f01 = convert_to_monochrome(gather<StorageType>(m_data, index.z(), active));
                Float f11 = convert_to_monochrome(gather<StorageType>(m_data, index.w(), active));

                // Partials w.r.t. pixel coordinate x and y
                Vector2f df_xy{ fmadd(w0.y(), f10 - f00, w1.y() * (f11 - f01)),
                                fmadd(w0.x(), f01 - f00, w1.x() * (f11 - f10)) };

                // Partials w.r.t. u and v (include uv transform by transpose multiply)
                Matrix uv_tm = m_transform.matrix;
                Vector2f df_uv{ uv_tm.coeff(0, 0) * df_xy.x() + uv_tm.coeff(1, 0) * df_xy.y(),
                                uv_tm.coeff(0, 1) * df_xy.x() + uv_tm.coeff(1, 1) * df_xy.y() };
                return m_resolution * df_uv;
            }
            // else if (m_filter_type == FilterType::Nearest)
            return Vector2f(.0f, .0f);
        }
    }

    Color3f eval_3(const SurfaceInteraction3f &si, Mask active = true) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (Channels != 3) {
            ENOKI_MARK_USED(si);
            Throw("eval_3(): The bitmap texture %s was queried for a RGB "
                  "value, but it is monochromatic!", to_string());
        } else if constexpr (is_spectral_v<Spectrum> && !Raw) {
            ENOKI_MARK_USED(si);
            Throw("eval_3(): The bitmap texture %s was queried for a RGB "
                  "value, but texture conversion to color spectra had "
                  "previously been requested! (raw=false)",
                  to_string());
        } else {
            return interpolate(si, active);
        }
    }

    template <typename T> T wrap(const T &value) const {
        if (m_wrap_mode == WrapMode::Clamp) {
            return clamp(value, 0, m_resolution - 1);
        } else {
            T div = T(m_inv_resolution_x(value.x()),
                      m_inv_resolution_y(value.y())),
              mod = value - div * m_resolution;

            masked(mod, mod < 0) += T(m_resolution);

            if (m_wrap_mode == WrapMode::Mirror)
                mod = select(eq(div & 1, 0) ^ (value < 0), mod, m_resolution - 1 - mod);

            return mod;
        }
    }

    MTS_INLINE auto interpolate(const SurfaceInteraction3f &si, Mask active) const {
        // Storage representation underlying this texture
        using StorageType = std::conditional_t<Channels == 1, Float, Color3f>;

        if constexpr (!is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform.transform_affine(si.uv);

        if (m_filter_type == FilterType::Bilinear) {
            using Int4  = Array<Int32, 4>;
            using Int24 = Array<Int4, 2>;

            // Scale to bitmap resolution and apply shift
            uv = fmadd(uv, m_resolution, -.5f);

            // Integer pixel positions for bilinear interpolation
            Vector2i uv_i = floor2int<Vector2i>(uv);

            // Interpolation weights
            Point2f w1 = uv - Point2f(uv_i),
                    w0 = 1.f - w1;

            // Apply wrap mode
            Int24 uv_i_w = wrap(Int24(Int4(0, 1, 0, 1) + uv_i.x(),
                                      Int4(0, 0, 1, 1) + uv_i.y()));

            Int4 index = uv_i_w.x() + uv_i_w.y() * m_resolution.x();

            /// TODO: merge into a single gather with the upcoming Enoki
            StorageType v00 = gather<StorageType>(m_data, index.x(), active),
                        v10 = gather<StorageType>(m_data, index.y(), active),
                        v01 = gather<StorageType>(m_data, index.z(), active),
                        v11 = gather<StorageType>(m_data, index.w(), active);

            // Bilinear interpolation
            if constexpr (is_spectral_v<Spectrum> && !Raw && Channels == 3) {
                // Evaluate spectral upsampling model from stored coefficients
                UnpolarizedSpectrum c00, c10, c01, c11, c0, c1;

                c00 = srgb_model_eval<UnpolarizedSpectrum>(v00, si.wavelengths);
                c10 = srgb_model_eval<UnpolarizedSpectrum>(v10, si.wavelengths);
                c01 = srgb_model_eval<UnpolarizedSpectrum>(v01, si.wavelengths);
                c11 = srgb_model_eval<UnpolarizedSpectrum>(v11, si.wavelengths);

                c0 = fmadd(w0.x(), c00, w1.x() * c10);
                c1 = fmadd(w0.x(), c01, w1.x() * c11);

                return fmadd(w0.y(), c0, w1.y() * c1);
            } else {
                StorageType v0 = fmadd(w0.x(), v00, w1.x() * v10),
                            v1 = fmadd(w0.x(), v01, w1.x() * v11);

                return fmadd(w0.y(), v0, w1.y() * v1);
            }
        } else {
            // Scale to bitmap resolution, no shift
            uv *= m_resolution;

            // Integer pixel positions for bilinear interpolation
            Vector2i uv_i   = floor2int<Vector2i>(uv),
                     uv_i_w = wrap(uv_i);

            Int32 index = uv_i_w.x() + uv_i_w.y() * m_resolution.x();

            StorageType v = gather<StorageType>(m_data, index, active);
            if constexpr (is_spectral_v<Spectrum> && !Raw && Channels == 3)
                return srgb_model_eval<UnpolarizedSpectrum>(v, si.wavelengths);
            else
                return v;
        }
    }

    std::pair<Point2f, Float> sample_position(const Point2f &sample,
                                              Mask active = true) const override {
        if (!m_distr2d) {
            // Construct 2D distribution upon first access, avoid races
            std::lock_guard<tbb::spin_mutex> guard(m_mutex);
            if (!m_distr2d) {
                auto self = const_cast<BitmapTextureImpl *>(this);
                self->rebuild_internals(false, true);
            }
        }

        auto [pos, pdf, sample2] = m_distr2d->sample(sample, active);
        ScalarVector2f inv_resolution = rcp(ScalarVector2f(m_resolution));

        if (m_filter_type == FilterType::Nearest) {
            sample2 = (Point2f(pos) + sample2) * inv_resolution;
        } else {
            sample2 = (Point2f(pos) + 0.5f + warp::square_to_tent(sample2)) *
                      inv_resolution;

            switch (m_wrap_mode) {
                case WrapMode::Repeat:
                    sample2[sample2 < 0.f] += 1.f;
                    sample2[sample2 > 1.f] -= 1.f;
                    break;

                /* Texel sampling is restricted to [0, 1] and only interpolation
                   with one row/column of pixels beyond that is considered, so
                   both clamp/mirror effectively use the same strategy. No such
                   distinction is needed for the pdf() method. */
                case WrapMode::Clamp:
                case WrapMode::Mirror:
                    sample2[sample2 < 0.f] = -sample2;
                    sample2[sample2 > 1.f] = 2.f - sample2;
                    break;
            }
        }

        return { sample2, pdf * hprod(m_resolution) };
    }

    Float pdf_position(const Point2f &pos_, Mask active = true) const override {
        if (!m_distr2d) {
            // Construct 2D distribution upon first access, avoid races
            std::lock_guard<tbb::spin_mutex> guard(m_mutex);
            if (!m_distr2d) {
                auto self = const_cast<BitmapTextureImpl *>(this);
                self->rebuild_internals(false, true);
            }
        }

        if (m_filter_type == FilterType::Bilinear) {
            using Int4  = Array<Int32, 4>;
            using Int24 = Array<Int4, 2>;

            // Scale to bitmap resolution and apply shift
            Point2f uv = fmadd(pos_, m_resolution, -.5f);

            // Integer pixel positions for bilinear interpolation
            Vector2i uv_i = floor2int<Vector2i>(uv);

            // Interpolation weights
            Point2f w1 = uv - Point2f(uv_i),
                    w0 = 1.f - w1;

            Float v00 = m_distr2d->pdf(wrap(uv_i + Point2i(0, 0)), active),
                  v10 = m_distr2d->pdf(wrap(uv_i + Point2i(1, 0)), active),
                  v01 = m_distr2d->pdf(wrap(uv_i + Point2i(0, 1)), active),
                  v11 = m_distr2d->pdf(wrap(uv_i + Point2i(1, 1)), active);

            Float v0 = fmadd(w0.x(), v00, w1.x() * v10),
                  v1 = fmadd(w0.x(), v01, w1.x() * v11);

            return fmadd(w0.y(), v0, w1.y() * v1) * hprod(m_resolution);
        } else {
            // Scale to bitmap resolution, no shift
            Point2f uv = pos_ * m_resolution;

            // Integer pixel positions for bilinear interpolation
            Vector2i uv_i = wrap(floor2int<Vector2i>(uv));

            return m_distr2d->pdf(uv_i, active) * hprod(m_resolution);
        }
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("data", m_data);
        callback->put_parameter("resolution", m_resolution);
        callback->put_parameter("transform", m_transform);
    }

    void parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (keys.empty() || string::contains(keys, "data")) {
            /// Convert m_data into a managed array (available in CPU/GPU address space)
            rebuild_internals(true, m_distr2d != nullptr);
        }
    }

    ScalarVector2i resolution() const override { return m_resolution; }

    ScalarFloat mean() const override {
        return m_mean;
    }

    bool is_spatially_varying() const override { return true; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "BitmapTextureImpl[" << std::endl
            << "  name = \"" << m_name << "\"," << std::endl
            << "  resolution = \"" << m_resolution << "\"," << std::endl
            << "  raw = " << (int) Raw << "," << std::endl
            << "  mean = " << m_mean << "," << std::endl
            << "  transform = " << string::indent(m_transform) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()

protected:
    /**
     * \brief Recompute mean and 2D sampling distribution (if requested)
     * following an update
     */
    void rebuild_internals(bool init_mean, bool init_distr) {
        // Recompute the mean texture value following an update
        m_data = m_data.managed();
        const ScalarFloat *ptr = m_data.data();

        double mean = 0.0;
        size_t pixel_count = (size_t) hprod(m_resolution);
        bool bad = false;

        if (Channels == 3) {
            std::unique_ptr<ScalarFloat[]> importance_map(
                init_distr ? new ScalarFloat[pixel_count] : nullptr);

            for (size_t i = 0; i < pixel_count; ++i) {
                ScalarColor3f value = load_unaligned<ScalarColor3f>(ptr);
                ScalarFloat tmp;
                if constexpr (is_spectral_v<Spectrum> && !Raw) {
                    tmp = srgb_model_mean(value);
                } else {
                    if (!all(value >= 0 && value <= 1))
                        bad = true;
                    tmp = luminance(value);
                }
                if (init_distr)
                    importance_map[i] = tmp;
                mean += (double) tmp;
                ptr += 3;
            }

            if (init_distr)
                m_distr2d = std::make_unique<DiscreteDistribution2D<Float>>(
                    importance_map.get(), m_resolution);
        } else {
            for (size_t i = 0; i < pixel_count; ++i) {
                ScalarFloat value = ptr[i];
                if (!(value >= 0 && value <= 1))
                    bad = true;
                mean += (double) value;
            }

            if (init_distr)
                m_distr2d = std::make_unique<DiscreteDistribution2D<Float>>(
                    ptr, m_resolution);
        }

        if (init_mean)
            m_mean = ScalarFloat(mean / pixel_count);

        if (bad)
            Log(Warn,
                "BitmapTexture: texture named \"%s\" contains pixels that "
                "exceed the [0, 1] range!", m_name);
    }

protected:
    DynamicBuffer<Float> m_data;
    ScalarVector2i m_resolution;
    enoki::divisor<int32_t> m_inv_resolution_x;
    enoki::divisor<int32_t> m_inv_resolution_y;
    std::string m_name;
    ScalarTransform3f m_transform;
    ScalarFloat m_mean;
    FilterType m_filter_type;
    WrapMode m_wrap_mode;

    // Optional: distribution for importance sampling
    mutable tbb::spin_mutex m_mutex;
    std::unique_ptr<DiscreteDistribution2D<Float>> m_distr2d;
};

MTS_IMPLEMENT_CLASS_VARIANT(BitmapTexture, Texture)
MTS_EXPORT_PLUGIN(BitmapTexture, "Bitmap texture")


/* This class has a name that depends on extra template parameters, so
   the standard MTS_IMPLEMENT_CLASS_VARIANT macro cannot be used */

NAMESPACE_BEGIN(detail)
template <uint32_t Channels, bool Raw>
constexpr const char * bitmap_class_name() {
    if constexpr (!Raw) {
        return (Channels == 1) ? "BitmapTextureImpl_1_0"
                               : "BitmapTextureImpl_3_0";
    } else {
        return (Channels == 1) ? "BitmapTextureImpl_1_1"
                               : "BitmapTextureImpl_3_1";
    }
}
NAMESPACE_END(detail)

template <typename Float, typename Spectrum, uint32_t Channels, bool Raw>
Class *BitmapTextureImpl<Float, Spectrum, Channels, Raw>::m_class
    = new Class(detail::bitmap_class_name<Channels, Raw>(), "Texture",
                ::mitsuba::detail::get_variant<Float, Spectrum>(), nullptr, nullptr);

template <typename Float, typename Spectrum, uint32_t Channels, bool Raw>
const Class* BitmapTextureImpl<Float, Spectrum, Channels, Raw>::class_() const {
    return m_class;
}

NAMESPACE_END(mitsuba)
