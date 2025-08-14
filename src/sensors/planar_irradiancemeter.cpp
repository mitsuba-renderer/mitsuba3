#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT class PlanarIrradianceMeter final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_film, sample_wavelengths, pdf_wavelengths, m_to_world)
    MI_IMPORT_TYPES(Shape)

    PlanarIrradianceMeter(const Properties &props) : Base(props) {
        if (props.has_property("shape"))
            Throw("Found a 'shape' attached to this sensor -- this is not allowed.");

        if (m_film->rfilter()->radius() > .5f + math::RayEpsilon<Float>)
            Log(Warn, "This sensor should only be used with a reconstruction filter"
               "of radius 0.5 or lower (e.g. default 'box' filter)");
    }

    std::pair<Ray3f, Spectrum>
    sample_ray(Float time, Float wavelength_sample,
                            const Point2f & sample2,
                            const Point2f & sample3,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spatial component
        Point3f origin = m_to_world.value() * Point3f(
            2.f * sample2.x() - 1.f,
            2.f * sample2.y() - 1.f,
            0.f
        );

        // 2. Sample directional component
        Vector3f local = warp::square_to_uniform_sphere(sample3);

        // 3. Sample spectrum
        auto [wavelengths, wav_weight] =
            sample_wavelengths(dr::zeros<SurfaceInteraction3f>(),
                               wavelength_sample, active);

        Vector3f d = dr::normalize(m_to_world.value() * local);
        Point3f o = origin + d * math::RayEpsilon<Float>;

        return {
            Ray3f(o, d, time, wavelengths),
            4.f * dr::Pi<Float> * surface_area() *
            depolarizer<Spectrum>(wav_weight)
        };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, Mask active) const override {
        const auto [ps, ps_pdf] =
            sample_position(it.time, sample, active);

        DirectionSample3f ds = ps;

        Vector3f d = ds.p - it.p;
        Float dist = dr::norm(d);

        ds.d = d / dist;
        ds.dist = dist;

        return std::make_pair(ds, ps_pdf);
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &, Mask) const override {
        return dr::rcp(surface_area());
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        Spectrum wav_pdf = 1.f;
        if constexpr (is_spectral_v<Spectrum>)
            wav_pdf = pdf_wavelengths(si.wavelengths, active);

        return 4.f * dr::Pi<Float> * surface_area() / wav_pdf;
    }

    std::pair<PositionSample3f, Float> sample_position(Float time, const Point2f &sample, Mask) const override {
        PositionSample3f ps = PositionSample3f(
            m_to_world.value() * Point3f(
                2.f * sample.x() - 1.f,
                2.f * sample.y() - 1.f,
                0.f),
            m_to_world.value() * Normal3f(0.f, 0.f, 1.f), // TODO no normal?
            sample,
            time,
            dr::rcp(surface_area()),
            false
        );

        return std::make_pair(ps, ps.pdf);
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox = ScalarBoundingBox3f();

        for (uint8_t corner_idx = 0; corner_idx < 4; ++corner_idx)
            bbox.expand(m_to_world.scalar() * get_corner(corner_idx));

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

    Float surface_area() const {
        return dr::norm(dr::cross(
            m_to_world.value() * (get_corner(1) - get_corner(0)),
            m_to_world.value() * (get_corner(2) - get_corner(0))
        ));
    }

    static MI_INLINE ScalarPoint3f get_corner(const uint8_t corner_idx) {
        return ScalarPoint3f(
                2.f * (ScalarFloat)(corner_idx & 1) - 1.f,
                2.f * (ScalarFloat)((corner_idx >> 1) & 1) - 1,
                0.f
        );
    }

    MI_DECLARE_CLASS(IrradianceMeter)
};

MI_EXPORT_PLUGIN(PlanarIrradianceMeter)
NAMESPACE_END(mitsuba)
