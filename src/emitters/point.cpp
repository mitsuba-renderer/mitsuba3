#include <mitsuba/core/bbox.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class PointLight final : public Emitter<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN()
    MTS_USING_BASE(Emitter, m_medium, m_needs_sample_3, m_world_transform)
    using Scene              = typename Aliases::Scene;
    using Shape              = typename Aliases::Shape;
    using ContinuousSpectrum = typename Aliases::ContinuousSpectrum;

    PointLight(const Properties &props) : Base(props) {
        if (props.has_property("position")) {
            if (props.has_property("to_world"))
                Throw("Only one of the parameters 'position' and 'to_world' "
                      "can be specified at the same time!'");

            m_world_transform = new AnimatedTransform(
                Transform4f::translate(Vector3f(props.point3f("position"))));
        }

        m_intensity = props.spectrum<Float, Spectrum>("intensity", ContinuousSpectrum::D65(1.f));
        m_needs_sample_3 = false;
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f & /*pos_sample*/, const Point2f &dir_sample,
                                          Mask active) const override {
        auto [wavelengths, spec_weight] = m_intensity->sample(
            math::sample_shifted<wavelength_t<Spectrum>>(wavelength_sample), active);

        const auto &trafo = m_world_transform->eval(time);
        Ray3f ray(trafo * Point3f(0.f),
                 warp::square_to_uniform_sphere(dir_sample),
                 time, wavelengths);

        return { ray, spec_weight * (4.f * math::Pi<Float>) };
    }

    MTS_INLINE std::pair<DirectionSample3f, Spectrum> sample_direction(const Interaction3f &it,
                                                                       const Point2f & /*sample*/,
                                                                       Mask active) const override {
        auto trafo = m_world_transform->eval(it.time, active);

        DirectionSample3f ds;
        ds.p     = trafo.translation();
        ds.n     = 0.f;
        ds.uv    = 0.f;
        ds.time  = it.time;
        ds.pdf   = 1.f;
        ds.delta = true;
        ds.d     = ds.p - it.p;
        ds.dist  = norm(ds.d);
        Float inv_dist = rcp(ds.dist);
        ds.d *= inv_dist;

        Spectrum spec = m_intensity->eval(it.wavelengths, active) *
                        (inv_dist * inv_dist);

        return { ds, spec };
    }

    MTS_INLINE Float pdf_direction(const Interaction3f &, const DirectionSample3f &,
                                   Mask) const override {
        return 0.f;
    }

    MTS_INLINE Spectrum eval(const SurfaceInteraction3f &, Mask) const override {
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

private:
    ref<ContinuousSpectrum> m_intensity;
};


MTS_IMPLEMENT_PLUGIN(PointLight, Emitter, "Point emitter");
NAMESPACE_END(mitsuba)
