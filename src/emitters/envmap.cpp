#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class EnvironmentMapEmitter final : public Emitter<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN(EnvironmentMapEmitter, Emitter)
    MTS_USING_BASE(Emitter, m_world_transform)
    using Scene              = typename Aliases::Scene;
    using Shape              = typename Aliases::Shape;
    using ContinuousSpectrum = typename Aliases::ContinuousSpectrum;
    using Warp               = warp::Marginal2D<Float, 0>;

    EnvironmentMapEmitter(const Properties &props) : Base(props) {
        /* Until `create_shape` is called, we have no information
           about the scene and default to the unit bounding sphere. */
        m_bsphere = BoundingSphere3f(Point3f(0.f), 1.f);

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));

        m_bitmap = new Bitmap(file_path);

        /* Convert to linear RGB float bitmap, will be converted
           into spectral profile coefficients below */
        m_bitmap = m_bitmap->convert(Bitmap::ERGBA, Bitmap::EFloat, false);
        m_name = file_path.filename().string();

        std::unique_ptr<Float[]> luminance(new Float[hprod(m_bitmap->size())]);

        Float *ptr = (Float *) m_bitmap->data(),
              *lum_ptr = (Float *) luminance.get();

        for (size_t y = 0; y < m_bitmap->size().y(); ++y) {
            Float sin_theta =
                std::sin(y / Float(m_bitmap->size().y() - 1) * math::Pi<Float>);

            for (size_t x = 0; x < m_bitmap->size().x(); ++x) {
                Color3f rgb = load_unaligned<Vector3f>(ptr);
                Float scale = hmax(rgb) * 2.f;
                Color3f rgb_norm = rgb / std::max((Float) 1e-8, scale);

                // Fetch spectral fit for given sRGB color value
                Vector4f coeff = concat(srgb_model_fetch(rgb_norm), scale);

                // Overwrite the pixel value with the coefficients
                *lum_ptr++ = mitsuba::luminance(rgb) * sin_theta;
                store_unaligned(ptr, coeff);
                ptr += 4;
            }
        }

        m_scale = props.float_("scale", 1.f);
        m_warp = Warp(m_bitmap->size(), luminance.get());
        m_d65 = ContinuousSpectrum::D65(1.f);

#if defined(MTS_ENABLE_AUTODIFF)
        m_bitmap_d = FloatC::copy(m_bitmap->data(), hprod(m_bitmap->size()) * 3);
#endif
    }

    ref<Shape> create_shape(const Scene *scene) override {
        // Create a bounding sphere that surrounds the scene
        m_bsphere = scene->bbox().bounding_sphere();
        m_bsphere.radius = max(math::Epsilon<Float>, m_bsphere.radius * 1.5f);

        Properties props("sphere");
        props.set_point3f("center", m_bsphere.center);
        props.set_float("radius", m_bsphere.radius);

        // Sphere is "looking in" towards the scene
        props.set_bool("flip_normals", true);

        return PluginManager::instance()->create_object<Shape>(props);
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        Vector3f v = m_world_transform->eval(si.time, active)
                         .inverse()
                         .transform_affine(-si.wi);

        /* Convert to latitude-longitude texture coordinates */
        Point2f uv = Point2f(atan2(v.x(), -v.z()) * math::InvTwoPi<Float>,
                             safe_acos(v.y()) * math::InvPi<Float>);
        uv -= floor(uv);

        return eval_spectral(uv, si.wavelengths, active);
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
            eval_spectral(uv, it.wavelengths, active) / ds.pdf
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

    BoundingBox3f bbox() const override {
        /* This emitter does not occupy any particular region
           of space, return an invalid bounding box */
        return BoundingBox3f();
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
    Spectrum eval_spectral(Point2f uv, const Wavelength &wavelengths, Mask active) const {
        uv *= Vector2f(m_bitmap->size() - 1u);

        Point2u pos = min(Point2u(uv), m_bitmap->size() - 2u);

        Point2f w1 = uv - Point2f(pos),
                w0 = 1.f - w1;

        UInt32 index = pos.x() + pos.y() * (uint32_t) m_bitmap->size().x();

        Vector4f v00, v10, v01, v11;
        if constexpr (is_diff_array_v<Float>) {
#if defined(MTS_ENABLE_AUTODIFF)
            uint32_t width = (uint32_t) m_bitmap->size().x();
            v00            = gather<Vector4f>(m_bitmap_d, index, active);
            v10            = gather<Vector4f>(m_bitmap_d, index + 1u, active);
            v01            = gather<Vector4f>(m_bitmap_d, index + width, active);
            v11            = gather<Vector4f>(m_bitmap_d, index + width + 1u, active);
#endif
        } else {
            uint32_t width   = (uint32_t) m_bitmap->size().x() * 4;
            const Float *ptr = (const Float *) m_bitmap->data();

            v00 = gather<Vector4f>(ptr, index, active);
            v10 = gather<Vector4f>(ptr + 4, index, active);
            v01 = gather<Vector4f>(ptr + width, index, active);
            v11 = gather<Vector4f>(ptr + width + 4, index, active);
        }

        Spectrum s00 = srgb_model_eval(head<3>(v00), wavelengths),
                 s10 = srgb_model_eval(head<3>(v10), wavelengths),
                 s01 = srgb_model_eval(head<3>(v01), wavelengths),
                 s11 = srgb_model_eval(head<3>(v11), wavelengths),
                 s0  = fmadd(w0.x(), s00, w1.x() * s10),
                 s1  = fmadd(w0.x(), s01, w1.x() * s11),
                 f0  = fmadd(w0.x(), v00.w(), w1.x() * v10.w()),
                 f1  = fmadd(w0.x(), v01.w(), w1.x() * v11.w()),
                 s   = fmadd(w0.y(), s0, w1.y() * s1),
                 f   = fmadd(w0.y(), f0, w1.y() * f1);

        return s * f * m_d65->eval(wavelengths, active) * m_scale;
    }

protected:
    std::string m_name;
    BoundingSphere3f m_bsphere;
    ref<Bitmap> m_bitmap;
    Warp m_warp;
    ref<ContinuousSpectrum> m_d65;
    Float m_scale;

#if defined(MTS_ENABLE_AUTODIFF)
    FloatD m_bitmap_d;
#endif
};

MTS_IMPLEMENT_PLUGIN(EnvironmentMapEmitter, Emitter, "Environment map emitter");
NAMESPACE_END(mitsuba)

