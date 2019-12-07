#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class EnvironmentMapEmitter final : public Emitter<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(EnvironmentMapEmitter, Emitter)
    MTS_IMPORT_BASE(Emitter, m_world_transform)
    MTS_IMPORT_TYPES(Scene, Shape, Texture)

    using Warp = warp::Marginal2D<Float, 0>;

    EnvironmentMapEmitter(const Properties &props) : Base(props) {
        /* Until `set_scene` is called, we have no information
           about the scene and default to the unit bounding sphere. */
        m_bsphere = ScalarBoundingSphere3f(ScalarPoint3f(0.f), 1.f);

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));

        m_bitmap = new Bitmap(file_path);

        /* Convert to linear RGB float bitmap, will be converted
           into spectral profile coefficients below */
        m_bitmap = m_bitmap->convert(PixelFormat::RGBA, Bitmap::FloatFieldType, false);
        m_name = file_path.filename().string();

        std::unique_ptr<ScalarFloat[]> luminance(new ScalarFloat[hprod(m_bitmap->size())]);

        ScalarFloat *ptr = (ScalarFloat *) m_bitmap->data(),
              *lum_ptr = (ScalarFloat *) luminance.get();

        // TODO: avoid having this code duplicated over BitmapTexture
        for (size_t y = 0; y < m_bitmap->size().y(); ++y) {
            ScalarFloat sin_theta =
                std::sin(y / ScalarFloat(m_bitmap->size().y() - 1) * math::Pi<ScalarFloat>);

            for (size_t x = 0; x < m_bitmap->size().x(); ++x) {
                // We potentially overwrite the pixel value with the spectral
                // upsampling coefficients.
                ScalarColor3f rgb = load_unaligned<ScalarVector3f>(ptr);
                ScalarFloat lum   = mitsuba::luminance(rgb);

                ScalarVector4f coeff;
                if constexpr (is_monochromatic_v<Spectrum>) {
                    coeff = ScalarVector4f(lum, lum, lum, 1.f);
                } else if constexpr (is_rgb_v<Spectrum>) {
                    coeff = concat(rgb, ScalarFloat(1.f));
                } else {
                    static_assert(is_spectral_v<Spectrum>);
                    // Fetch spectral fit for given sRGB color value
                    ScalarFloat scale      = hmax(rgb) * 2.f;
                    ScalarColor3f rgb_norm = rgb / std::max((ScalarFloat) 1e-8, scale);
                    coeff = concat(static_cast<Array<ScalarFloat, 3>>(srgb_model_fetch(rgb_norm)),
                                   scale);
                }

                *lum_ptr++ = lum * sin_theta;
                store_unaligned(ptr, coeff);
                ptr += 4;
            }
        }

        m_scale = props.float_("scale", 1.f);
        m_warp = Warp(m_bitmap->size(), luminance.get());
        m_d65 = Texture::D65(1.f);
    }

    void set_scene(const Scene *scene) override {
        m_bsphere = scene->bbox().bounding_sphere();
        m_bsphere.radius = max(math::Epsilon<Float>,
                               m_bsphere.radius * (1.f + math::Epsilon<Float>));
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        Vector3f v = m_world_transform->eval(si.time, active)
                         .inverse()
                         .transform_affine(-si.wi);

        /* Convert to latitude-longitude texture coordinates */
        Point2f uv = Point2f(atan2(v.x(), -v.z()) * math::InvTwoPi<Float>,
                             safe_acos(v.y()) * math::InvPi<Float>);
        uv -= floor(uv);

        return eval_spectrum(uv, si.wavelengths, active);
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float /* time */, Float /* wavelength_sample */,
                                          const Point2f & /* sample2 */,
                                          const Point2f & /* sample3 */,
                                          Mask /* active */) const override {
        NotImplementedError("sample_ray");
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, Mask active) const override {
        auto [uv, pdf] = m_warp.sample(sample);

        Float theta = uv.y() * math::Pi<Float>,
              phi = uv.x() * (2.f * math::Pi<Float>);

        Vector3f d = math::sphdir(theta, phi);
        d = Vector3f(d.y(), d.z(), -d.x());

        Float dist = 2.f * m_bsphere.radius;

        Float inv_sin_theta = safe_rsqrt(sqr(d.x()) + sqr(d.z()));

        d = m_world_transform->eval(it.time, active).transform_affine(d);

        DirectionSample3f ds;
        ds.p      = it.p + d * dist;
        ds.n      = -d;
        ds.uv     = uv;
        ds.time   = it.time;
        ds.pdf    = pdf * inv_sin_theta * (1.f / (2.f * math::Pi<Float> * math::Pi<Float>) );
        ds.delta  = false;
        ds.object = this;
        ds.d      = d;
        ds.dist   = dist;

        return std::make_pair(
            ds,
            eval_spectrum(uv, it.wavelengths, active) / ds.pdf
        );
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        Vector3f d = m_world_transform->eval(it.time, active)
                         .inverse()
                         .transform_affine(ds.d);

        /* Convert to latitude-longitude texture coordinates */
        Point2f uv = Point2f(atan2(d.x(), -d.z()) * math::InvTwoPi<Float>,
                             safe_acos(d.y()) * math::InvPi<Float>);
        uv -= floor(uv);

        Float inv_sin_theta = safe_rsqrt(sqr(d.x()) + sqr(d.z()));
        Float result =
            m_warp.eval(uv) * inv_sin_theta * (1.f / (2.f * math::Pi<Float> * math::Pi<Float>) );

        return result;
    }

    ScalarBoundingBox3f bbox() const override {
        /* This emitter does not occupy any particular region
           of space, return an invalid bounding box */
        return ScalarBoundingBox3f();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("scale", m_scale);
        callback->put_object("d65", m_d65.get());
        callback->put_object("bitmap", m_bitmap.get());
    }

    void parameters_changed() override {
        // TODO update bsphere
        // TODO update bitmap and warp
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "EnvironmentMapEmitter[" << std::endl
            << "  name = \"" << m_name << "\"," << std::endl
            << "  bitmap = " << string::indent(m_bitmap) << "," << std::endl
            << "  bsphere = " << m_bsphere << std::endl
            << "]";
        return oss.str();
    }

    bool is_environment() const override {
        return true;
    }

protected:
    UnpolarizedSpectrum eval_spectrum(Point2f uv, const Wavelength &wavelengths, Mask active) const {
        uv *= Vector2f(m_bitmap->size() - 1u);

        Point2u pos = min(Point2u(uv), m_bitmap->size() - 2u);

        Point2f w1 = uv - Point2f(pos),
                w0 = 1.f - w1;

        UInt32 index = pos.x() + pos.y() * (uint32_t) m_bitmap->size().x();

        Vector4f v00, v10, v01, v11;
        uint32_t width   = (uint32_t) m_bitmap->size().x() * 4;
        const Float *ptr = (const Float *) m_bitmap->data();

        v00 = gather<Vector4f>(ptr, index, active);
        v10 = gather<Vector4f>(ptr + 4, index, active);
        v01 = gather<Vector4f>(ptr + width, index, active);
        v11 = gather<Vector4f>(ptr + width + 4, index, active);

        if constexpr (is_monochromatic_v<Spectrum>) {
            // Only one channel is enough here
            UnpolarizedSpectrum v0 = fmadd(w0.x(), v00.x(), w1.x() * v10.x()),
                                v1 = fmadd(w0.x(), v01.x(), w1.x() * v11.x());
            return fmadd(w0.y(), v0, w1.y() * v1);
        } else if constexpr (is_rgb_v<Spectrum>) {
            // No need for spectral conversion, we directly read-off the RGB values
            UnpolarizedSpectrum v0 = fmadd(w0.x(), head<3>(v00), w1.x() * head<3>(v10)),
                                v1 = fmadd(w0.x(), head<3>(v01), w1.x() * head<3>(v11));
            return fmadd(w0.y(), v0, w1.y() * v1);
        } else {
            static_assert(is_spectral_v<Spectrum>);
            UnpolarizedSpectrum s00 = srgb_model_eval<UnpolarizedSpectrum>(head<3>(v00), wavelengths),
                                s10 = srgb_model_eval<UnpolarizedSpectrum>(head<3>(v10), wavelengths),
                                s01 = srgb_model_eval<UnpolarizedSpectrum>(head<3>(v01), wavelengths),
                                s11 = srgb_model_eval<UnpolarizedSpectrum>(head<3>(v11), wavelengths),
                                s0  = fmadd(w0.x(), s00, w1.x() * s10),
                                s1  = fmadd(w0.x(), s01, w1.x() * s11),
                                f0  = fmadd(w0.x(), v00.w(), w1.x() * v10.w()),
                                f1  = fmadd(w0.x(), v01.w(), w1.x() * v11.w()),
                                s   = fmadd(w0.y(), s0, w1.y() * s1),
                                f   = fmadd(w0.y(), f0, w1.y() * f1);

            SurfaceInteraction3f si = zero<SurfaceInteraction3f>();
            si.wavelengths = wavelengths;

            return s * f * m_d65->eval(si, active) * m_scale;
        }
    }

protected:
    std::string m_name;
    ScalarBoundingSphere3f m_bsphere;
    ref<Bitmap> m_bitmap;
    Warp m_warp;
    ref<Texture> m_d65;
    ScalarFloat m_scale;
};

MTS_EXPORT_PLUGIN(EnvironmentMapEmitter, "Environment map emitter");
NAMESPACE_END(mitsuba)
