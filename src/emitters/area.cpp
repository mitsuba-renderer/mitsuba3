#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class AreaLight final : public Emitter<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN(AreaLight, Emitter)
    MTS_USING_BASE(Emitter, m_shape, m_medium)
    using Scene              = typename RenderAliases::Scene;
    using Shape              = typename RenderAliases::Shape;
    using ContinuousSpectrum = typename RenderAliases::ContinuousSpectrum;

    AreaLight(const Properties &props) : Base(props) {
        if (props.has_property("to_world"))
            Throw("Found a 'to_world' transformation -- this is not allowed. "
                  "The area light inherits this transformation from its parent "
                  "shape.");

        m_radiance = props.spectrum<Float, Spectrum>("radiance", ContinuousSpectrum::D65(1.f));
    }

    void set_shape(Shape *shape) override {
        Base::set_shape(shape);
        m_area_times_pi = m_shape->surface_area() * math::Pi<ScalarFloat>;
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        return m_radiance->eval(si.wavelengths, active) &
               (Frame3f::cos_theta(si.wi) > 0.f);
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2, const Point2f &sample3,
                                          Mask active) const override {
        /* 1. Sample spectrum */
        auto [wavelengths, spec_weight] = m_radiance->sample(
            math::sample_shifted<wavelength_t<Spectrum>>(wavelength_sample), active);

        /* 2. Sample spatial component */
        PositionSample3f ps = m_shape->sample_position(time, sample2, active);

        /* 3. Sample directional component */
        auto local = warp::square_to_cosine_hemisphere(sample3);

        return std::make_pair(
            Ray3f(ps.p, Frame3f(ps.n).to_world(local), time, wavelengths),
            spec_weight * m_area_times_pi
        );
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, Mask active) const override {
        Assert(m_shape, "Can't sample from an area emitter without an associated Shape.");

        DirectionSample3f ds = m_shape->sample_direction(it, sample, active);
        active &= dot(ds.d, ds.n) < 0.f && neq(ds.pdf, 0.f);

        Spectrum spec = m_radiance->eval(it.wavelengths, active) / ds.pdf;

        ds.object = this;
        return { ds, spec & active };
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        return select(dot(ds.d, ds.n) < 0.f,
                      m_shape->pdf_direction(it, ds, active), 0.f);
    }

    BoundingBox3f bbox() const override { return m_shape->bbox(); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "AreaLight[" << std::endl
            << "  radiance = " << string::indent(m_radiance) << "," << std::endl
            << "  surface_area = ";
        if (m_shape) oss << m_shape->surface_area();
        else         oss << "  <no shape attached!>";
        oss << "," << std::endl;
        if (m_medium) oss << string::indent(m_medium->to_string());
        else         oss << "  <no medium attached!>";
        oss << std::endl << "]";
        return oss.str();
    }

private:
    ref<ContinuousSpectrum> m_radiance;
    ScalarFloat m_area_times_pi = 0.f;
};


MTS_IMPLEMENT_PLUGIN(AreaLight, Emitter, "Point emitter");
NAMESPACE_END(mitsuba)
