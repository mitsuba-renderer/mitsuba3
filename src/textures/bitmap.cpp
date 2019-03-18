#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

class BitmapTexture final : public ContinuousSpectrum {
public:
    BitmapTexture(const Properties &props) {
        m_transform = props.transform("to_uv", Transform4f()).extract();

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_name = file_path.filename().string();
        Log(EDebug, "Loading bitmap texture from \"%s\" ..", m_name);

        m_bitmap = new Bitmap(file_path);

        /* Convert to linear RGBA float bitmap, will be converted
           into spectral profile coefficients below */
        m_bitmap = m_bitmap->convert(Bitmap::ERGB, Bitmap::EFloat, false);

        Float upscale = props.float_("upscale", 1.f);
        if (upscale != 1.f) {
            Vector2s old_size = m_bitmap->size(),
                     new_size = upscale * old_size;

            ref<ReconstructionFilter> rfilter =
                PluginManager::instance()->create_object<ReconstructionFilter>(
                    Properties("tent"));

            Log(EInfo, "Upsampling bitmap texture \"%s\" to %ix%i ..", m_name,
                       new_size.x(), new_size.y());
            m_bitmap = m_bitmap->resample(new_size, rfilter);
        }

        Float *ptr = (Float *) m_bitmap->data();
        double mean = 0.0;

        for (size_t y = 0; y < m_bitmap->size().y(); ++y) {
            for (size_t x = 0; x < m_bitmap->size().x(); ++x) {
                Color3f rgb = load_unaligned<Vector3f>(ptr);

                if (props.bool_("monochrome"))
                    rgb = luminance(rgb);

                /* Fetch spectral fit for given sRGB color value (3 coefficients) */
                Vector3f coeff = srgb_model_fetch(rgb);
                mean += (double) srgb_model_mean(coeff);

                /* Overwrite the pixel value with the coefficients */
                store_unaligned(ptr, coeff);
                ptr += 3;
            }
        }

        m_mean = Float(mean / hprod(m_bitmap->size()));
    }

    template <typename SurfaceInteraction, typename Mask,
              typename Value    = typename SurfaceInteraction::Value,
              typename Spectrum = mitsuba::Spectrum<Value>>
    MTS_INLINE Spectrum eval_impl(const SurfaceInteraction &it,
                                  Mask active) const {
        using UInt32   = uint32_array_t<Value>;
        using Point2u  = Point<UInt32, 2>;
        using Point2f  = Point<Value, 2>;
        using Vector3f = Vector<Value, 3>;

        auto uv = m_transform.transform_affine(it.uv);
        uv -= floor(uv);
        uv *= Vector2f(m_bitmap->size() - 1u);

        Point2u pos = min(Point2u(uv), m_bitmap->size() - 2u);

        Point2f w1 = uv - Point2f(pos),
                w0 = 1.f - w1;

        UInt32 index = pos.x() + pos.y() * (uint32_t) m_bitmap->size().x();

        const Float *ptr = (const Float *) m_bitmap->data();

        uint32_t width = (uint32_t) m_bitmap->size().x() * 3;

        Vector3f v00 = gather<Vector3f, 0, true>(ptr, index, active),
                 v10 = gather<Vector3f, 0, true>(ptr + 3, index, active),
                 v01 = gather<Vector3f, 0, true>(ptr + width, index, active),
                 v11 = gather<Vector3f, 0, true>(ptr + width + 3, index, active);

        Spectrum s00 = srgb_model_eval(v00, it.wavelengths),
                 s10 = srgb_model_eval(v10, it.wavelengths),
                 s01 = srgb_model_eval(v01, it.wavelengths),
                 s11 = srgb_model_eval(v11, it.wavelengths),
                 s0 = fmadd(w0.x(), s00, w1.x() * s10),
                 s1 = fmadd(w0.x(), s01, w1.x() * s11);

        return fmadd(w0.y(), s0, w1.y() * s1);
    }

    Float mean() const override { return m_mean; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Bitmap[" << std::endl
            << "  name = \"" << m_name << "\"," << std::endl
            << "  bitmap = " << string::indent(m_bitmap) << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_TEXTURE()
    MTS_DECLARE_CLASS()
protected:
    ref<Bitmap> m_bitmap;
    std::string m_name;
    Transform3f m_transform;
    Float m_mean;
};

MTS_IMPLEMENT_CLASS(BitmapTexture, ContinuousSpectrum)
MTS_EXPORT_PLUGIN(BitmapTexture, "Bitmap texture")

NAMESPACE_END(mitsuba)
