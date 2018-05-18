#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

class AreaLight : public Emitter {
public:
    AreaLight(const Properties &props) : Emitter(props) {
        if (props.has_property("to_world"))
            Throw("Found a 'to_world' transformation -- this is not allowed. "
                  "The area light inherits this transformation from its parent "
                  "shape.");

        m_radiance = props.spectrum("radiance", ContinuousSpectrum::D65());
    }

    void set_shape(Shape *shape) override {
        Emitter::set_shape(shape);
        m_area_times_pi = m_shape->surface_area() * math::Pi;
    }

    template <typename SurfaceInteraction, typename Mask,
              typename Spectrum = typename SurfaceInteraction::Spectrum,
              typename Frame    = Frame<typename SurfaceInteraction::Point3>>
    MTS_INLINE Spectrum eval_impl(const SurfaceInteraction &si,
                                  Mask active) const {
        return m_radiance->eval(si.wavelengths, active) &
               (Frame::cos_theta(si.wi) > 0.f);
    }

    template <typename Point2,
              typename Value    = value_t<Point2>,
              typename Mask     = mask_t<Value>,
              typename Point3   = Point<Value, 3>,
              typename Ray3     = mitsuba::Ray<Point3>,
              typename Spectrum = Spectrum<Value>>
    MTS_INLINE std::pair<Ray3, Spectrum>
    sample_ray_impl(Value time,
                    Value wavelength_sample,
                    const Point2 &sample2,
                    const Point2 &sample3,
                    Mask active) const {
        using PositionSample = PositionSample<Point3>;
        using Frame = mitsuba::Frame<Vector<Value, 3>>;

        /* 1. Sample spectrum */
        Spectrum wavelengths, spec_weight;
        std::tie(wavelengths, spec_weight) = m_radiance->sample(
            enoki::sample_shifted<Spectrum>(wavelength_sample), active);

        /* 2. Sample spatial component */
        PositionSample ps = m_shape->sample_position(time, sample2);

        /* 3. Sample directional component */
        auto local = warp::square_to_cosine_hemisphere(sample3);

        return std::make_pair(
            Ray3(ps.p, Frame(ps.n).to_world(local), time, wavelengths),
            spec_weight * m_area_times_pi
        );
    }

    template <typename Interaction, typename Mask,
              typename Value           = typename Interaction::Value,
              typename Point2          = typename Interaction::Point2,
              typename Point3          = typename Interaction::Point3,
              typename Spectrum        = typename Interaction::Spectrum,
              typename DirectionSample = DirectionSample<Point3>>
    MTS_INLINE std::pair<DirectionSample, Spectrum>
    sample_direction_impl(const Interaction &it, const Point2 &sample, Mask active) const {
        Assert(m_shape, "Can't sample from an area emitter without an associated Shape.");

        DirectionSample ds = m_shape->sample_direction(it, sample, active);
        active &= dot(ds.d, ds.n) < 0.f && neq(ds.pdf, 0.f);

        Spectrum spec = m_radiance->eval(it.wavelengths, active) / ds.pdf;

        masked(ds.pdf, !active) = 0.f;
        masked(spec,   !active) = 0.f;

        ds.object = this;
        return { ds, spec };
    }

    template <typename Interaction, typename DirectionSample, typename Mask,
              typename Value = typename DirectionSample::Value>
    MTS_INLINE Value pdf_direction_impl(const Interaction &it,
                                        const DirectionSample &ds,
                                        Mask active) const {
        return select(dot(ds.d, ds.n) < 0.f,
                      m_shape->pdf_direction(it, ds, active), 0.f);
    }

    template <typename Ray, typename Value = typename Ray::Value,
              typename Spectrum = mitsuba::Spectrum<Value>>
    MTS_INLINE Spectrum eval_environment_impl(const Ray &,
                                              mask_t<Value>) const {
        return 0.f;
    }

    BoundingBox3f bbox() const override { return m_shape->bbox(); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "AreaLight[" << std::endl
            << "  radiance = " << m_radiance << "," << std::endl
            << "  surface_area = ";
        if (m_shape) oss << m_shape->surface_area();
        else         oss << "  <no shape attached!>";
        oss << "," << std::endl;
        if (m_medium) oss << string::indent(m_medium->to_string());
        else         oss << "  <no medium attached!>";
        oss << std::endl << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_EMITTER()
    MTS_DECLARE_CLASS()
private:
    ref<ContinuousSpectrum> m_radiance;
    Float m_area_times_pi = 0.f;
};


MTS_IMPLEMENT_CLASS(AreaLight, Emitter)
MTS_EXPORT_PLUGIN(AreaLight, "Point emitter");
NAMESPACE_END(mitsuba)
