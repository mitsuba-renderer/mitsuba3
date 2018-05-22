#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

class PointLight : public Emitter {
public:
    PointLight(const Properties &props) : Emitter(props) {
        if (props.has_property("position")) {
            if (props.has_property("to_world"))
                Throw("Only one of the parameters 'position' and 'to_world' "
                      "can be specified at the same time!'");

            m_world_transform = new AnimatedTransform(
                Transform4f::translate(Vector3f(props.point3f("position"))));
        }

        m_intensity = props.spectrum("intensity", ContinuousSpectrum::D65());
        m_needs_sample_3 = false;
    }

    template <typename Point2,
              typename Value    = value_t<Point2>,
              typename Mask     = mask_t<Value>,
              typename Point3   = Point<Value, 3>,
              typename Ray3     = mitsuba::Ray<Point3>,
              typename Spectrum = Spectrum<Value>>
    std::pair<Ray3, Spectrum>
    sample_ray_impl(Value time,
                    Value wavelength_sample,
                    const Point2 &/*pos_sample*/,
                    const Point2 &dir_sample,
                    Mask active) const {
        Spectrum wavelengths, spec_weight;
        std::tie(wavelengths, spec_weight) = m_intensity->sample(
            enoki::sample_shifted<Spectrum>(wavelength_sample), active);

        const auto &trafo = m_world_transform->eval(time);
        Ray3 ray(trafo * Point3(0.0f),
                 warp::square_to_uniform_sphere(dir_sample),
                 time, wavelengths);

        return { ray, spec_weight * (4.0f * math::Pi) };
    }

    template <typename Interaction, typename Mask,
              typename Point2 = typename Interaction::Point2,
              typename Point3 = typename Interaction::Point3,
              typename Value = typename Interaction::Value,
              typename Spectrum = typename Interaction::Spectrum,
              typename DirectionSample = DirectionSample<Point3>>
    std::pair<DirectionSample, Spectrum>
    sample_direction_impl(const Interaction &it,
                          const Point2 & /*sample*/,
                          Mask active) const {
        auto trafo = m_world_transform->eval(it.time, active);

        DirectionSample ds;
        ds.p     = trafo.translation();
        ds.n     = 0.0f;
        ds.uv    = 0.0f;
        ds.time  = it.time;
        ds.pdf   = 1.0f;
        ds.delta = true;
        ds.d     = ds.p - it.p;
        ds.dist  = norm(ds.d);
        Value inv_dist = rcp(ds.dist);
        ds.d *= inv_dist;

        Spectrum spec = m_intensity->eval(it.wavelengths, active) *
                        (inv_dist * inv_dist);

        return { ds, spec };
    }

    template <typename Interaction, typename DirectionSample, typename Mask,
              typename Value = typename DirectionSample::Value>
    Value pdf_direction_impl(const Interaction &, const DirectionSample &, Mask) const {
        return 0.0f;
    }

    template <typename SurfaceInteraction, typename Mask,
              typename Spectrum = typename SurfaceInteraction::Spectrum>
    Spectrum eval_impl(const SurfaceInteraction &, Mask) const {
        return 0.0f;
    }

    template <typename Ray,
              typename Value    = typename Ray::Value,
              typename Spectrum = mitsuba::Spectrum<Value>>
    Spectrum eval_environment_impl(const Ray &, mask_t<Value>) const {
        return 0.f;
    }

    BoundingBox3f bbox() const override {
        return m_world_transform->translation_bounds();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "PointLight[" << std::endl
            << "  world_transform = " << string::indent(m_world_transform->to_string()) << "," << std::endl
            << "  intensity = " << m_intensity << "," << std::endl
            << "  medium = " << (m_medium ? string::indent(m_medium->to_string()) : "")
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_EMITTER()
    MTS_DECLARE_CLASS()
private:
    ref<ContinuousSpectrum> m_intensity;
};


MTS_IMPLEMENT_CLASS(PointLight, Emitter)
MTS_EXPORT_PLUGIN(PointLight, "Point emitter");
NAMESPACE_END(mitsuba)
