#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class ConstantBackgroundEmitter final : public Emitter<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(ConstantBackgroundEmitter, Emitter)
    MTS_USING_BASE(Emitter)
    MTS_IMPORT_TYPES(Scene, Shape, ContinuousSpectrum)

    ConstantBackgroundEmitter(const Properties &props) : Base(props) {
        /* Until `create_shape` is called, we have no information
           about the scene and default to the unit bounding sphere. */
        m_bsphere = ScalarBoundingSphere3f(ScalarPoint3f(0.f), 1.f);

        m_radiance = props.spectrum<ContinuousSpectrum>("radiance", ContinuousSpectrum::D65(1.f));
    }

    ref<Shape> create_shape(const Scene *scene) override {
        // Create a bounding sphere that surrounds the scene
        m_bsphere = scene->bbox().bounding_sphere();
        m_bsphere.radius = max(math::Epsilon<ScalarFloat>, 1.5f * m_bsphere.radius);

        Properties props("sphere");
        props.set_point3f("center", m_bsphere.center);
        props.set_float("radius", m_bsphere.radius);

        // Sphere is "looking in" towards the scene
        props.set_bool("flip_normals", true);

        return PluginManager::instance()->create_object<Shape>(props);
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        return m_radiance->eval(si.wavelengths, active);
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2, const Point2f &sample3,
                                          Mask active) const override {
        // 1. Sample spectrum
        auto [wavelengths, weight] = m_radiance->sample(
            math::sample_shifted<wavelength_t<Spectrum>>(wavelength_sample), active);

        // 2. Sample spatial component
        Vector3f v0 = warp::square_to_uniform_sphere(sample2);

        // 3. Sample directional component
        Vector3f v1 = warp::square_to_cosine_hemisphere(sample3);

        Float r2 = m_bsphere.radius * m_bsphere.radius;
        return std::make_pair(Ray3f(m_bsphere.center + v0 * m_bsphere.radius,
                                    Frame3f(-v0).to_world(v1), time, wavelengths),
                              (4 * math::Pi<Float> * math::Pi<Float> * r2) * weight);
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, Mask active) const override {
        Vector3f d = warp::square_to_uniform_sphere(sample);
        Float dist = 2.f * m_bsphere.radius;

        DirectionSample3f ds;
        ds.p      = it.p + d * dist;
        ds.n      = -d;
        ds.uv     = Point2f(0.f);
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

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &ds,
                        Mask /*active*/) const override {
        return warp::square_to_uniform_sphere_pdf(ds.d);
    }

    /// This emitter does not occupy any particular region of space, return an invalid bounding box
    ScalarBoundingBox3f bbox() const override {
        return ScalarBoundingBox3f();
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

protected:
    ref<ContinuousSpectrum> m_radiance;
    ScalarBoundingSphere3f m_bsphere;
};

MTS_IMPLEMENT_PLUGIN(ConstantBackgroundEmitter, Emitter, "Constant background emitter");
NAMESPACE_END(mitsuba)
