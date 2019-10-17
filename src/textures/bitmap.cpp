#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Bilinearly interpolated image texture.
 */
// TODO(!): test the "single-channel texture" use-case that was added during refactoring
template <typename Float, typename Spectrum>
class BitmapTexture final : public ContinuousSpectrum<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN()
    using Base = ContinuousSpectrum<Float, Spectrum>;

    BitmapTexture(const Properties &props) {
        using Scalar    = scalar_t<Float>;
        using SColor3f  = Color<Scalar, 3>;
        using SVector3f = Vector<Scalar, 3>;

        m_transform = props.transform("to_uv", Transform4f()).extract();

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_name = file_path.filename().string();
        Log(Debug, "Loading bitmap texture from \"%s\" ..", m_name);

        m_bitmap = new Bitmap(file_path);

        /* Convert to linear RGB float bitmap, will be converted
           into spectral profile coefficients below (in place) */
        m_bitmap = m_bitmap->convert(Bitmap::ERGB, Bitmap::EFloat, false);

        // Optional texture upsampling
        Scalar upscale = props.float_("upscale", 1.f);
        if (upscale != 1.f) {
            using SVector2s = Vector<size_t, 2>;
            using ReconstructionFilter = mitsuba::ReconstructionFilter<Float>;

            SVector2s old_size = m_bitmap->size(),
                      new_size = upscale * old_size;

            auto rfilter =
                PluginManager::instance()->create_object<ReconstructionFilter>(
                    Properties("tent"));

            Log(Info, "Upsampling bitmap texture \"%s\" to %ix%i ..", m_name,
                       new_size.x(), new_size.y());
            m_bitmap = m_bitmap->resample(new_size, rfilter);
        }

        // Convert to spectral coefficients, monochrome or leave in RGB.
        auto *ptr = (Scalar *) m_bitmap->data();
        double mean = 0.0;
        for (size_t y = 0; y < m_bitmap->size().y(); ++y) {
            for (size_t x = 0; x < m_bitmap->size().x(); ++x) {
                // Loading from a scalar-typed buffer regardless of the value of type Float
                SColor3f rgb = load_unaligned<SVector3f>(ptr);

                if constexpr (is_monochrome_v<Spectrum>) {
                    Scalar lum = luminance(rgb);
                    rgb = lum;
                    mean += (double) lum;
                    store_unaligned(ptr, rgb);
                } else if constexpr (is_spectral_v<Spectrum>) {
                    // Fetch spectral fit for given sRGB color value (3 coefficients)
                    Vector3f coeff = srgb_model_fetch(rgb);
                    mean += (double) srgb_model_mean(coeff);
                    store_unaligned(ptr, coeff);
                }

                ptr += 3;
            }
        }

        m_mean = Float(mean / hprod(m_bitmap->size()));

#if defined(MTS_ENABLE_AUTODIFF)
        m_bitmap_d = FloatC::copy(m_bitmap->data(), hprod(m_bitmap->size()) * 3);
#endif
    }

    MTS_INLINE Spectrum eval(const SurfaceInteraction3f &si,
                             Mask active) const override {
        Point2f uv = m_transform.transform_affine(si.uv);
        uv -= floor(uv);
        uv *= Vector2f(m_bitmap->size() - 1u);

        Point2u pos = min(Point2u(uv), m_bitmap->size() - 2u);

        Point2f w1 = uv - Point2f(pos),
                w0 = 1.f - w1;

        UInt32 index = pos.x() + pos.y() * (uint32_t) m_bitmap->size().x();

        Vector3f v00, v10, v01, v11;
        // TODO: for single-channel textures, gather a single number
        // TODO: unify this part
        if constexpr (is_diff_array_v<Float>) {
#if defined(MTS_ENABLE_AUTODIFF)
            uint32_t width = (uint32_t) m_bitmap->size().x();
            v00 = gather<Vector3f>(m_bitmap_d, index, active);
            v10 = gather<Vector3f>(m_bitmap_d, index + 1u, active);
            v01 = gather<Vector3f>(m_bitmap_d, index + width, active);
            v11 = gather<Vector3f>(m_bitmap_d, index + width + 1u, active);
#endif
        } else {
            uint32_t width = (uint32_t) m_bitmap->size().x() * 3;
            const Float *ptr = (const Float *) m_bitmap->data();

            v00 = gather<Vector3f, 0, true>(ptr, index, active);
            v10 = gather<Vector3f, 0, true>(ptr + 3, index, active);
            v01 = gather<Vector3f, 0, true>(ptr + width, index, active);
            v11 = gather<Vector3f, 0, true>(ptr + width + 3, index, active);
        }

        Spectrum s00, s10, s01, s11;
        if constexpr (is_spectral_v<Spectrum>) {
            // Evaluate spectral upsampling model from stored coefficients
            s00 = srgb_model_eval(v00, si.wavelengths);
            s10 = srgb_model_eval(v10, si.wavelengths);
            s01 = srgb_model_eval(v01, si.wavelengths);
            s11 = srgb_model_eval(v11, si.wavelengths);
        } else if constexpr (is_monochrome_v<Spectrum>) {
            s00 = v00.x();
            s01 = v01.x();
            s10 = v10.x();
            s11 = v11.x();
        } else {
            // No conversion to do
            s00 = v00; s01 = v01; s10 = v10; s11 = v11;
        }

        // Bilinear interpolation
        Spectrum s0 = fmadd(w0.x(), s00, w1.x() * s10),
                 s1 = fmadd(w0.x(), s01, w1.x() * s11);
        return fmadd(w0.y(), s0, w1.y() * s1);
    }

    Float mean() const override { return m_mean; }

#if defined(MTS_ENABLE_AUTODIFF)
    void put_parameters(DifferentiableParameters &dp) override {
        dp.put(this, "bitmap", m_bitmap_d);
    }
#endif

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Bitmap[" << std::endl
            << "  name = \"" << m_name << "\"," << std::endl
            << "  bitmap = " << string::indent(m_bitmap) << std::endl
            << "]";
        return oss.str();
    }

protected:
    ref<Bitmap> m_bitmap;
    static constexpr int m_channel_count = texture_channels_v<Spectrum>;

#if defined(MTS_ENABLE_AUTODIFF)
    FloatD m_bitmap_d;
#endif

    std::string m_name;
    Transform3f m_transform;
    Float m_mean;
};

MTS_IMPLEMENT_PLUGIN(BitmapTexture, ContinuousSpectrum, "Bitmap texture")

NAMESPACE_END(mitsuba)
