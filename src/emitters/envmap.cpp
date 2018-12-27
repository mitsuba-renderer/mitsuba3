#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

class EnvironmentMapEmitter final : public Emitter {
public:
    using Warp = warp::Marginal2D<0>;

    EnvironmentMapEmitter(const Properties &props) : Emitter(props) {
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
                std::sin(y / Float(m_bitmap->size().y() - 1) * math::Pi);

            for (size_t x = 0; x < m_bitmap->size().x(); ++x) {
                Color3f rgb = load_unaligned<Vector3f>(ptr);
                Float scale = hmax(rgb) * 2.f;
                Color3f rgb_norm = rgb / scale;

                /* Fetch spectral fit for given sRGB color value */
                Vector4f coeff = concat(srgb_model_fetch(rgb_norm), scale);

                /* Overwrite the pixel value with the coefficients */
                *lum_ptr++ = mitsuba::luminance(rgb) * sin_theta;
                store_unaligned(ptr, coeff);
                ptr += 4;
            }
        }

        m_scale = props.float_("scale", 1.f);
        m_warp = Warp(m_bitmap->size(), luminance.get());
        m_d65 = ContinuousSpectrum::D65();
    }

    ref<Shape> create_shape(const Scene *scene) override {
        // Create a bounding sphere that surrounds the scene
        m_bsphere = scene->kdtree()->bbox().bounding_sphere();
        m_bsphere.radius = max(math::Epsilon, m_bsphere.radius * 1.5f);

        Properties props("sphere");
        props.set_point3f("center", m_bsphere.center);
        props.set_float("radius", m_bsphere.radius);

        // Sphere is "looking in" towards the scene
        props.set_bool("flip_normals", true);

        return PluginManager::instance()->create_object<Shape>(props);
    }

    template <typename SurfaceInteraction, typename Mask,
              typename Spectrum = typename SurfaceInteraction::Spectrum>
    MTS_INLINE Spectrum eval_impl(const SurfaceInteraction &si,
                                  Mask active) const {
        using Value    = typename SurfaceInteraction::Value;
        using Point2f  = Point<Value, 2>;
        using Vector3f = Vector<Value, 3>;

        Vector3f v = m_world_transform->eval(si.time, active)
                         .inverse()
                         .transform_affine(-si.wi);

        /* Convert to latitude-longitude texture coordinates */
        Point2f uv = Point2f(atan2(v.x(), -v.z()) * math::InvTwoPi,
                             safe_acos(v.y()) * math::InvPi);
        uv -= floor(uv);

        return eval_spectral(uv, si.wavelengths, active);
    }

    template <typename Point2, typename Value = value_t<Point2>>
    MTS_INLINE auto
    sample_ray_impl(Value /* time */, Value /* wavelength_sample */,
                    const Point2 & /* sample2 */,
                    const Point2 & /* sample3 */,
                    mask_t<Value> /* active */) const {
        NotImplementedError("sample_ray");
        return std::pair<Ray<Point<Value, 3>>, Spectrum<Value>>();
    }

    template <typename Interaction,
              typename Point2 = typename Interaction::Point2,
              typename Value  = typename Interaction::Value>
    MTS_INLINE auto sample_direction_impl(const Interaction &it,
                                          const Point2 &sample,
                                          mask_t<Value> active) const {
        using Point3  = typename Interaction::Point3;
        using Vector3 = vector3_t<Point3>;
        using DirectionSample = DirectionSample<Point3>;

        auto [uv, pdf] = m_warp.sample(sample);

        Value theta = uv.y() * math::Pi,
              phi = uv.x() * (2.f * math::Pi);

        Vector3 d = math::sphdir(theta, phi);
        d = Vector3(d.y(), d.z(), -d.x());

        Float dist = 2.f * m_bsphere.radius;

        Value inv_sin_theta = safe_rsqrt(sqr(d.x()) + sqr(d.z()));

        d = m_world_transform->eval(it.time, active).transform_affine(d);

        DirectionSample ds;
        ds.p      = it.p + d * dist;
        ds.n      = -d;
        ds.uv     = uv;
        ds.time   = it.time;
        ds.pdf    = pdf * inv_sin_theta * (1.f / (2.f * math::Pi * math::Pi));
        ds.delta  = false;
        ds.object = this;
        ds.d      = d;
        ds.dist   = dist;

        return std::make_pair(
            ds,
            eval_spectral(uv, it.wavelengths, active) / ds.pdf
        );
    }

    template <typename Interaction, typename DirectionSample,
              typename Value = typename DirectionSample::Value>
    MTS_INLINE Value pdf_direction_impl(const Interaction &it,
                                        const DirectionSample &ds,
                                        mask_t<Value> active) const {
        using Vector3f = Vector<Value, 3>;
        using Point2f  = Point<Value, 2>;

        Vector3f d = m_world_transform->eval(it.time, active)
                         .inverse()
                         .transform_affine(ds.d);

        /* Convert to latitude-longitude texture coordinates */
        Point2f uv = Point2f(atan2(d.x(), -d.z()) * math::InvTwoPi,
                             safe_acos(d.y()) * math::InvPi);
        uv -= floor(uv);

        Value inv_sin_theta = safe_rsqrt(sqr(d.x()) + sqr(d.z()));
        Value result = m_warp.eval(uv) * inv_sin_theta * (1.f / (2.f * math::Pi * math::Pi));

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

    MTS_IMPLEMENT_EMITTER()
    MTS_DECLARE_CLASS()

protected:
    template <typename Point2f, typename Spectrum, typename Mask>
    Spectrum eval_spectral(Point2f uv,
                           const Spectrum &wavelengths,
                           Mask active) const {
        using Value    = value_t<Point2f>;
        using UInt32   = uint32_array_t<Value>;
        using Point2u  = Point<UInt32, 2>;
        using Vector4f = Vector<Value, 4>;

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

        Spectrum s00 = srgb_model_eval(v00, wavelengths),
                 s10 = srgb_model_eval(v10, wavelengths),
                 s01 = srgb_model_eval(v01, wavelengths),
                 s11 = srgb_model_eval(v11, wavelengths),
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
};

MTS_IMPLEMENT_CLASS(EnvironmentMapEmitter, Emitter)
MTS_EXPORT_PLUGIN(EnvironmentMapEmitter, "Environment map emitter");
NAMESPACE_END(mitsuba)

