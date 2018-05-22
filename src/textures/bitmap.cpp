#include <mitsuba/core/properties.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

class BitmapTexture final : public ContinuousSpectrum {
public:
    BitmapTexture(const Properties &props) {
        m_transform = props.transform("to_uv", Transform4f()).extract();

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_bitmap = new Bitmap(file_path);

        /* Convert to linear RGBA float bitmap, will be converted
           into spectral profile coefficients below */
        m_bitmap = m_bitmap->convert(Bitmap::ERGBA, Bitmap::EFloat, false);
        m_name = file_path.filename().string();

        Float *ptr = (Float *) m_bitmap->data();

        for (size_t y = 0; y < m_bitmap->size().y(); ++y) {
            for (size_t x = 0; x < m_bitmap->size().x(); ++x) {
                Vector4f coeff = load<Vector4f>(ptr);
                Color3f color = head<3>(coeff);

                /* Fetch spectral fit for given sRGB color value (4 coefficients) */
                coeff = srgb_model_fetch(color);

                /* Overwrite the pixel value with the coefficients */
                store(ptr, coeff);
                ptr += 4;
            }
        }
    }

    template <typename SurfaceInteraction, typename Mask,
              typename Value    = typename SurfaceInteraction::Value,
              typename Spectrum = mitsuba::Spectrum<Value>>
    MTS_INLINE Spectrum eval_impl(const SurfaceInteraction &it,
                                  Mask active) const {
        using UInt32   = uint32_array_t<Value>;
        using Point2u  = Point<UInt32, 2>;
        using Point2f  = Point<Value, 2>;
        using Vector4f = Vector<Value, 4>;

        auto uv = m_transform.transform_affine(it.uv);
        uv -= floor(uv);

        uv *= Vector2f(m_bitmap->size() - 1u);

        Point2u pos = min(Point2u(uv), m_bitmap->size() - 2u);

        Point2f w1 = uv - Point2f(pos),
                w0 = 1.f - w1;

        UInt32 index = pos.x() + pos.y() * (uint32_t) m_bitmap->size().x();

        const Float *ptr = (const Float *) m_bitmap->data();

        uint32_t width = (uint32_t) m_bitmap->size().x() * 4;

        Vector4f v00 = gather<Vector4f>(ptr, index, active),
                 v10 = gather<Vector4f>(ptr + 4, index, active),
                 v01 = gather<Vector4f>(ptr + width, index, active),
                 v11 = gather<Vector4f>(ptr + width + 4, index, active);

        Spectrum s00 = srgb_model_eval(v00, it.wavelengths),
                 s10 = srgb_model_eval(v10, it.wavelengths),
                 s01 = srgb_model_eval(v01, it.wavelengths),
                 s11 = srgb_model_eval(v11, it.wavelengths);

        return w0.y() * (w0.x() * s00 + w1.x() * s10) +
               w1.y() * (w0.x() * s01 + w1.x() * s11);
    }

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
};

MTS_IMPLEMENT_CLASS(BitmapTexture, ContinuousSpectrum)
MTS_EXPORT_PLUGIN(BitmapTexture, "Bitmap texture")

NAMESPACE_END(mitsuba)
