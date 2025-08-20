#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT class PlanarIrradianceMeter final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_film, sample_wavelengths, pdf_wavelengths, m_to_world, m_flags)
    MI_IMPORT_TYPES(Shape)

    PlanarIrradianceMeter(const Properties &props) : Base(props) {
        if (props.has_property("shape"))
            Throw("Found a 'shape' attached to this sensor -- this is not allowed.");

        if (m_film->rfilter()->radius() > .5f + math::RayEpsilon<Float>)
            Log(Warn, "This sensor should only be used with a reconstruction filter"
               "of radius 0.5 or lower (e.g. default 'box' filter)");

        Normal3f n     = dr::normalize(m_to_world.value() * Normal3f(0.f, 0.f, 1.f)),
                 dp_du = m_to_world.value() * Vector3f(2.f, 0.f, 0.f),
                 dp_dv = m_to_world.value() * Vector3f(0.f, 2.f, 0.f);

        m_frame = Frame3f(dp_du, dp_dv, n);
        m_inv_surface_area = dr::rcp(dr::norm(dr::cross(m_frame.s, m_frame.t)));
        dr::make_opaque(m_frame, m_inv_surface_area);

        m_flags = +EndpointFlags::Empty;
    }

    std::pair<Ray3f, Spectrum>
    sample_ray(Float time, Float wavelength_sample,
                            const Point2f & sample2,
                            const Point2f & sample3,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spatial component
        Point3f origin = m_to_world.value() *
            Point3f(dr::fmadd(sample2.x(), 2.f, -1.f),
                    dr::fmadd(sample2.y(), 2.f, -1.f), 0.f);

        // 2. Sample directional component
        Vector3f local = warp::square_to_cosine_hemisphere(sample3);

        // 3. Sample spectrum
        auto [wavelengths, wav_weight] =
            sample_wavelengths(dr::zeros<SurfaceInteraction3f>(),
                               wavelength_sample, active);

        Vector3f d = dr::normalize(m_to_world.value() * local);
        Point3f o = origin + d * math::RayEpsilon<Float>;

        return {
            Ray3f(o, d, time, wavelengths),
            dr::Pi<Float> * depolarizer<Spectrum>(wav_weight)
        };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, Mask active) const override {
        DirectionSample3f ds;
        std::tie(ds, std::ignore) = sample_position(it.time, sample, active);

        ds.d = ds.p - it.p;

        Float dist_squared = dr::squared_norm(ds.d);
        ds.dist = dr::sqrt(dist_squared);
        ds.d /= ds.dist;

        Float dp = dr::abs_dot(ds.d, ds.n);
        Float x = dist_squared / dp;
        ds.pdf *= dr::select(dr::isfinite(x), x, 0.f);

        return std::make_pair(ds, ds.pdf);
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &ds, Mask active) const override {
        Float pdf = pdf_position(ds, active),
        dp = dr::abs_dot(ds.d, ds.n);

        pdf *= dr::select(dp != 0.f, (ds.dist * ds.dist) / dp, 0.f);

        return pdf;
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        // TODO account for spectral pdf?
        Spectrum wav_pdf = 1.f;
        if constexpr (is_spectral_v<Spectrum>)
            wav_pdf = pdf_wavelengths(si.wavelengths, active);

        return dr::Pi<Float> * m_inv_surface_area;
    }

    std::pair<PositionSample3f, Float> sample_position(Float time, const Point2f &sample, Mask) const override {
        PositionSample3f ps = PositionSample3f(
        m_to_world.value() *
            Point3f(dr::fmadd(sample.x(), 2.f, -1.f),
                dr::fmadd(sample.y(), 2.f, -1.f), 0.f),
            m_frame.n,
            sample,
            time,
            m_inv_surface_area,
            false
        );

        return std::make_pair(ps, ps.pdf);
    }

    Float pdf_position(const PositionSample3f &, Mask) const override {
        return m_inv_surface_area;
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox = ScalarBoundingBox3f();

        for (uint8_t corner_idx = 0; corner_idx < 4; ++corner_idx)
            bbox.expand(m_to_world.scalar() *
                ScalarPoint3f(((corner_idx & 1) << 1) - 1,
                              (corner_idx & 2) - 1, 0));

        return bbox;
    }

    void set_shape(Shape*) override {
        Throw("This sensor should not be attached to a shape");
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "PlanarIrradianceMeter["
            << "\n\t to_world = " << m_to_world.scalar()
        << "\n]";
        return oss.str();
    }

protected:

    Frame3f m_frame;
    Float m_inv_surface_area;

    MI_DECLARE_CLASS(IrradianceMeter)
};

MI_EXPORT_PLUGIN(PlanarIrradianceMeter)
NAMESPACE_END(mitsuba)
