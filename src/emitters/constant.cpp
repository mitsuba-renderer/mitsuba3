#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/core/plugin.h>

NAMESPACE_BEGIN(mitsuba)

class ConstantBackgroundEmitter final : public Emitter {
public:
    ConstantBackgroundEmitter(const Properties &props) : Emitter(props) {
        /* Until `create_shape` is called, we have no information
           about the scene and default to the unit bounding sphere. */
        m_bsphere = BoundingSphere3f(Point3f(0.f), 1.f);

        m_radiance = props.spectrum("radiance", ContinuousSpectrum::D65());
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
        return m_radiance->eval(si.wavelengths, active);
    }

    template <typename Point2, typename Value = value_t<Point2>>
    MTS_INLINE auto
    sample_ray_impl(Value time, Value wavelength_sample, const Point2 &sample2,
                    const Point2 &sample3, mask_t<Value> active) const {
        using Spectrum = mitsuba::Spectrum<Value>;
        using Point3   = Point<Value, 3>;
        using Ray3     = Ray<Point3>;
        using Vector3  = vector3_t<Point3>;
        using Frame    = Frame<Vector<Value, 3>>;
        using Ray3     = Ray<Point3>;

        // 1. Sample spectrum
        auto [wavelengths, weight] = m_radiance->sample(
            math::sample_shifted<Spectrum>(wavelength_sample), active);

        // 2. Sample spatial component
        Vector3 v0 = warp::square_to_uniform_sphere(sample2);

        // 3. Sample directional component
        Vector3 v1 = warp::square_to_cosine_hemisphere(sample3);

        Float r2 = m_bsphere.radius * m_bsphere.radius;
        return std::make_pair(
            Ray3(m_bsphere.center + v0 * m_bsphere.radius,
                 Frame(-v0).to_world(v1), time, wavelengths),
            (4 * math::Pi * math::Pi * r2) * weight
        );
    }

    template <typename Interaction,
              typename Point2 = typename Interaction::Point2,
              typename Value  = typename Interaction::Value>
    MTS_INLINE auto sample_direction_impl(const Interaction &it,
                                          const Point2 &sample,
                                          mask_t<Value> active) const {
        using Point3   = typename Interaction::Point3;
        using Vector3  = vector3_t<Point3>;
        using DirectionSample = DirectionSample<Point3>;

        Vector3 d = warp::square_to_uniform_sphere(sample);
        Float dist = 2.f * m_bsphere.radius;

        DirectionSample ds;
        ds.p      = it.p + d * dist;
        ds.n      = -d;
        ds.uv     = Point2(0.f);
        ds.time   = it.time;
        ds.pdf    = warp::square_to_uniform_sphere_pdf(d);
        ds.delta  = false;
        ds.object = this;
        ds.d      = d;
        ds.dist   = dist;

        return std::make_pair(
            ds,
            m_radiance->eval(it.wavelengths, active) / ds.pdf
        );
    }

    template <typename Interaction, typename DirectionSample,
              typename Value = typename DirectionSample::Value>
    MTS_INLINE Value pdf_direction_impl(const Interaction &,
                                        const DirectionSample &ds,
                                        mask_t<Value>) const {
        return warp::square_to_uniform_sphere_pdf(ds.d);
    }

    BoundingBox3f bbox() const override {
        /* This emitter does not occupy any particular region
           of space, return an invalid bounding box */
        return BoundingBox3f();
    }

    bool is_environment() const override {
        return true;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "ConstantBackgroundEmitter[" << std::endl
            << "  radiance = " << string::indent(m_radiance) << "," << std::endl
            << "  bsphere = " << m_bsphere << "," << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_EMITTER()
    MTS_DECLARE_CLASS()
protected:
    ref<ContinuousSpectrum> m_radiance;
    BoundingSphere3f m_bsphere;
};

MTS_IMPLEMENT_CLASS(ConstantBackgroundEmitter, Emitter)
MTS_EXPORT_PLUGIN(ConstantBackgroundEmitter, "Constant background emitter");
NAMESPACE_END(mitsuba)
