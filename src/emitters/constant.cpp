#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/scene.h>

NAMESPACE_BEGIN(mitsuba)

class ConstantBackgroundEmitter : public Emitter {
public:
    /**
     * Until `create_shape` is called, we have no information about the scene,
     * and default to the unit bounding sphere.
     */
    ConstantBackgroundEmitter(const Properties &props)
        : Emitter(props), m_scene_bsphere(Point3f(0.f), 1.f) {
        m_radiance = props.spectrum("radiance", ContinuousSpectrum::D65());
    }

    ref<Shape> create_shape(const Scene *scene) override {
        // Create a bounding sphere that surrounds the scene
        m_scene_bsphere = scene->kdtree()->bbox().bounding_sphere();
        m_scene_bsphere.radius = max(math::Epsilon, m_scene_bsphere.radius * 1.5f);

        Properties props("sphere");
        props.set_point3f("center", m_scene_bsphere.center);
        props.set_float("radius", m_scene_bsphere.radius);
        // Sphere is "looking in" towards the scene
        props.set_bool("flip_normals", true);

        return PluginManager::instance()->create_object<Shape>(props);
    }

    template <typename SurfaceInteraction, typename Mask,
              typename Spectrum = typename SurfaceInteraction::Spectrum,
              typename Frame = Frame<typename SurfaceInteraction::Point3>>
    Spectrum eval_impl(const SurfaceInteraction &si, Mask active) const {
        return select(dot(si.sh_frame.n, si.wi) > 0.0f,
                      m_radiance->eval(si.wavelengths, active),
                      Spectrum(0.0f));
    }

    template <typename Point2, typename Value = value_t<Point2>>
    auto sample_ray_impl(Value time, Value wavelength_sample, const Point2 &sample2,
                         const Point2 &sample3, mask_t<Value> active) const {
        using Spectrum = Spectrum<Value>;
        using Point3  = Point<Value, 3>;
        using Vector3 = vector3_t<Point3>;
        using Frame   = Frame<Vector<Value, 3>>;
        using Ray3    = Ray<Point3>;

        // 1. Sample spectrum
        Spectrum wavelengths, wav_weight;
        std::tie(wavelengths, wav_weight) = m_radiance->sample(
            enoki::sample_shifted<Spectrum>(wavelength_sample), active);
        // 2. Sample spatial component
        Vector3 v0 = warp::square_to_uniform_sphere(sample2);
        // 3. Sample directional component
        Vector3 v1 = warp::square_to_cosine_hemisphere(sample3);

        Float r2 = m_scene_bsphere.radius * m_scene_bsphere.radius;
        return std::make_pair(
            Ray3(m_scene_bsphere.center + v0 * m_scene_bsphere.radius,
                 Frame(-v0).to_world(v1), time, wavelengths),
            (4 * math::Pi * math::Pi * r2) * wav_weight
        );
    }

    template <typename Interaction,
              typename Point2 = typename Interaction::Point2,
              typename Value  = typename Interaction::Value>
    auto sample_direction_impl(const Interaction &it, const Point2 &sample,
                               mask_t<Value> active) const {
        using Spectrum = typename Interaction::Spectrum;
        using Point3 = typename Interaction::Point3;
        using Vector3 = vector3_t<Point3>;
        using Ray3    = Ray<Vector3>;
        using DirectionSample = DirectionSample<Point3>;

        DirectionSample ds;
        ds.pdf = 0.0f;  // Default value in case of early return.
        ds.dist = math::Infinity;
        ds.time = it.time;
        ds.delta = false;

        Vector3 d = warp::square_to_uniform_sphere(sample);

        /* Intersect against the bounding sphere. This is not really
           necessary for path tracing and similar integrators. However,
           to make BDPT + MLT work with this emitter, we should return
           positions that are consistent with respect to the other
           sampling techniques in this class. */
        Ray3 ray(it.p, d, it.time, it.wavelengths);
        mask_t<Value> hit;
        Value near_t, far_t;
        std::tie(hit, near_t, far_t) = m_scene_bsphere.ray_intersect(ray);

        active = active && hit && (near_t < 0.0f) && (far_t > 0.0f);
        if (none(active))
            return std::make_pair(ds, Spectrum(0.0f));

        ds.p    = ray(far_t);
        ds.n    = normalize(m_scene_bsphere.center - ds.p);
        ds.d    = d;
        ds.pdf  = math::InvFourPi;

        return std::make_pair(
            ds,
            select(active,
                   m_radiance->eval(it.wavelengths, active) / ds.pdf,
                   Spectrum(0.0f))
        );
    }

    template <typename Interaction, typename DirectionSample,
              typename Value = typename DirectionSample::Value>
    Value pdf_direction_impl(const Interaction &, const DirectionSample &,
                             const mask_t<Value> &) const {
        return math::InvFourPi;
    }

    /**
     * \note The scene sets its bounding box so that it contains all shapes and
     * emitters, but this particular emitter always wants to be a little
     * bigger than the scene. To avoid a silly recursion, we return a
     * point here.
     */
    BoundingBox3f bbox() const override {
        return BoundingBox3f(m_scene_bsphere.center);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "ConstantBackgroundEmitter[" << std::endl
            << "  radiance = " << m_radiance << "," << std::endl
            << "  scene_bsphere = " << m_scene_bsphere << "," << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_EMITTER()
    MTS_DECLARE_CLASS()
protected:
    ref<ContinuousSpectrum> m_radiance;
    BoundingSphere3f m_scene_bsphere;
};

MTS_IMPLEMENT_CLASS(ConstantBackgroundEmitter, Emitter)
MTS_EXPORT_PLUGIN(ConstantBackgroundEmitter, "Constant background emitter");
NAMESPACE_END(mitsuba)
