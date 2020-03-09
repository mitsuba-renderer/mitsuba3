#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _texture-bitmap:

Bitmap texture (:monosp:`bitmap`)
---------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the bitmap to be loaded
 * - raw
   - |bool|
   - Should the transformation to the stored color data
     (e.g. sRGB to linear, spectral upsampling) be disabled? (Default: false)
 * - to_uv
   - |transform|
   - Specifies an optional uv transformation.  (Default: none, i.e. emitter space = world space)

This plugin provides a bitmap texture source that performs bilinearly interpolated
lookups on JPEG, PNG, OpenEXR, RGBE, TGA, and BMP files.

When loading the plugin, the data is first converted into a usable color representation
for the renderer:

* In :monosp:`rgb` modes, sRGB textures are converted into linear color space.
* In :monosp:`spectral` modes, sRGB textures are *spectrally upsampled* to plausible
  smooth spectra :cite:`Jakob2019Spectral` and store an intermediate representation
  used to query them during rendering.
* In :monosp:`monochrome` modes, sRGB textures are converted to grayscale.

These conversions can alternatively be disabled with the :paramtype:`raw` flag,
e.g. when textured data is already in linear space or does not represent colors
at all.

 */

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
                      "format (Y[A], RGB[A], XYZ[A])");
        }

        /* Should Mitsuba disable transformations to the stored color data? (e.g.
           sRGB to linear, spectral upsampling, etc.) */
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

        double mean = 0.0;
        if (m_bitmap->channel_count() == 3) {
            if (is_spectral_v<Spectrum> && !m_raw) {
                for (size_t i = 0; i < m_bitmap->pixel_count(); ++i) {
                    ScalarColor3f value = load_unaligned<ScalarColor3f>(ptr);
                    value = srgb_model_fetch(value);
                    mean += (double) srgb_model_mean(value);
                    store_unaligned(ptr, value);
                    ptr += 3;
                }
            } else {
                for (size_t i = 0; i < m_bitmap->pixel_count(); ++i) {
                    ScalarColor3f value = load_unaligned<ScalarColor3f>(ptr);
                    mean += (double) luminance(value);
                    ptr += 3;
                }
            }
        } else {
            for (size_t i = 0; i < m_bitmap->pixel_count(); ++i)
                mean += (double) ptr[i];
        }

        m_mean = ScalarFloat(mean / m_bitmap->pixel_count());
    }

    template <uint32_t Channels, bool Raw>
    using Impl = BitmapTextureImpl<Float, Spectrum, Channels, Raw>;

    /**
     * Recursively expand into an implementation specialized to the
     * actual loaded image.
     */
    std::vector<ref<Object>> expand() const override {
        ref<Object> result;
        Properties props;
        props.set_id(this->id());

        switch (m_bitmap->channel_count()) {
            case 1:
                result = m_raw
                  ? (Object *) new Impl<1, true >(props, m_bitmap, m_name, m_transform, m_mean)
                  : (Object *) new Impl<1, false>(props, m_bitmap, m_name, m_transform, m_mean);
                break;

            case 3:
                result = m_raw
                  ? (Object *) new Impl<3, true >(props, m_bitmap, m_name, m_transform, m_mean)
                  : (Object *) new Impl<3, false>(props, m_bitmap, m_name, m_transform, m_mean);
                break;

            default:
                Throw("Unsupported channel count: %d (expected 1 or 3)",
                      m_bitmap->channel_count());
        }

        return { result };
    }

    MTS_DECLARE_CLASS()
protected:
    ref<Bitmap> m_bitmap;
    std::string m_name;
    ScalarTransform3f m_transform;
    bool m_raw;
    ScalarFloat m_mean;
};

template <typename Float, typename Spectrum, uint32_t Channels, bool Raw>
class BitmapTextureImpl final : public Texture<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Texture)

    BitmapTextureImpl(const Properties &props,
                      const Bitmap *bitmap,
                      const std::string &name,
                      const ScalarTransform3f &transform,
                      ScalarFloat mean)
        : Texture(props), m_resolution(bitmap->size()),
          m_name(name), m_transform(transform), m_mean(mean) {
        m_data = DynamicBuffer<Float>::copy(bitmap->data(),
            hprod(m_resolution) * Channels);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("data", m_data);
        callback->put_parameter("resolution", m_resolution);
        callback->put_parameter("transform", m_transform);
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
            Throw("eval_1(): The bitmap texture %s was queried for a scalar value, but texture "
                  "conversion into spectra was requested! (raw=false)",
                  to_string());
        } else {
            auto result = interpolate(si, active);

            if constexpr (Channels == 3)
                return luminance(result);
            else
                return result;
        }
    }

    Color3f eval_3(const SurfaceInteraction3f &si, Mask active = true) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (Channels != 3) {
            ENOKI_MARK_USED(si);
            Throw("eval_3(): The bitmap texture %s was queried for a RGB value, but it is "
                  "monochromatic!", to_string());
        } else if constexpr (is_spectral_v<Spectrum> && !Raw) {
            ENOKI_MARK_USED(si);
            Throw("eval_3(): The bitmap texture %s was queried for a RGB value, but texture "
                  "conversion into spectra was requested! (raw=false)",
                  to_string());
        } else {
            return interpolate(si, active);
        }
    }

    MTS_INLINE auto interpolate(const SurfaceInteraction3f &si, Mask active) const {
        if constexpr (!is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform.transform_affine(si.uv);
        uv -= floor(uv);
        uv *= Vector2f(m_resolution - 1u);

        Point2u pos = min(Point2u(uv), m_resolution - 2u);

        Point2f w1 = uv - Point2f(pos),
                w0 = 1.f - w1;

        UInt32 index = pos.x() + pos.y() * m_resolution.x();
        uint32_t width = m_resolution.x();

        using StorageType = std::conditional_t<Channels == 1, Float, Color3f>;

        StorageType v00 = gather<StorageType>(m_data, index, active),
                    v10 = gather<StorageType>(m_data, index + 1, active),
                    v01 = gather<StorageType>(m_data, index + width, active),
                    v11 = gather<StorageType>(m_data, index + width + 1, active);

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
    }

    void parameters_changed() override {
        /// Convert m_data into a managed array (available in CPU/GPU address space)
        if constexpr (is_cuda_array_v<Float>)
            m_data = m_data.managed();

        // Recompute the mean texture value following an update
        ScalarFloat *ptr = m_data.data();

        double mean = 0.0;
        size_t pixel_count = hprod(m_resolution);
        if (Channels == 3) {
            if (is_spectral_v<Spectrum> && !Raw) {
                for (size_t i = 0; i < pixel_count; ++i) {
                    ScalarColor3f value = load_unaligned<ScalarColor3f>(ptr);
                    mean += (double) srgb_model_mean(value);
                    ptr += 3;
                }
            } else {
                for (size_t i = 0; i < pixel_count; ++i) {
                    ScalarColor3f value = load_unaligned<ScalarColor3f>(ptr);
                    value = clamp(value, 0.f, 1.f);
                    store_unaligned(ptr, value);
                    mean += (double) luminance(value);
                    ptr += 3;
                }
            }
        } else {
            for (size_t i = 0; i < pixel_count; ++i) {
                ScalarFloat value = ptr[i];
                value = clamp(value, 0.f, 1.f);
                ptr[i] = value;
                mean += (double) value;
            }
        }

        m_mean = ScalarFloat(mean / pixel_count);
    }

    ScalarFloat mean() const override { return m_mean; }

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
    DynamicBuffer<Float> m_data;
    ScalarVector2u m_resolution;
    std::string m_name;
    ScalarTransform3f m_transform;
    ScalarFloat m_mean;
};

MTS_IMPLEMENT_CLASS_VARIANT(BitmapTexture, Texture)
MTS_EXPORT_PLUGIN(BitmapTexture, "Bitmap texture")

NAMESPACE_BEGIN(detail)
template <uint32_t Channels, bool Raw>
constexpr const char * bitmap_class_name() {
    if constexpr (!Raw) {
        if constexpr (Channels == 1)
            return "BitmapTextureImpl_1_0";
        else
            return "BitmapTextureImpl_3_0";
    } else {
        if constexpr (Channels == 1)
            return "BitmapTextureImpl_1_1";
        else
            return "BitmapTextureImpl_3_1";
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
