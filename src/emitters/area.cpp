#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class AreaLight final : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Emitter, m_flags, m_shape, m_medium)
    MTS_IMPORT_TYPES(Scene, Shape, Texture)

    AreaLight(const Properties &props) : Base(props) {
        if (props.has_property("to_world"))
            Throw("Found a 'to_world' transformation -- this is not allowed. "
                  "The area light inherits this transformation from its parent "
                  "shape.");

        m_radiance = props.texture<Texture>("radiance", Texture::D65(1.f));
        // TODO: detect if underlying spectrum really is spatially varying
        m_flags = EmitterFlags::Surface | EmitterFlags::SpatiallyVarying;
    }

    void set_shape(Shape *shape) override {
        if (m_shape)
            Throw("An area emitter can be only be attached to a single shape.");

        Base::set_shape(shape);
        m_area_times_pi = m_shape->surface_area() * math::Pi<ScalarFloat>;
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        return select(
            Frame3f::cos_theta(si.wi) > 0.f,
            m_radiance->eval(si, active),
            0.f
        );
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2, const Point2f &sample3,
                                          Mask active) const override {
        // 1. Sample spatial component
        PositionSample3f ps = m_shape->sample_position(time, sample2, active);

        // 2. Sample directional component
        Vector3f local = warp::square_to_cosine_hemisphere(sample3);

        // 3. Sample spectrum
        SurfaceInteraction3f si(ps, zero<Wavelength>(0.f));
        auto [wavelengths, spec_weight] = m_radiance->sample(
            si, math::sample_shifted<Wavelength>(wavelength_sample), active);

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

        SurfaceInteraction3f si(ds, it.wavelengths);
        Spectrum spec = m_radiance->eval(si, active) / ds.pdf;

        ds.object = this;
        return { ds, spec & active };
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        return select(dot(ds.d, ds.n) < 0.f,
                      m_shape->pdf_direction(it, ds, active), 0.f);
    }

    ScalarBoundingBox3f bbox() const override { return m_shape->bbox(); }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("radiance", m_radiance.get());
    }

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

    MTS_DECLARE_CLASS()
private:
    ref<Texture> m_radiance;
    ScalarFloat m_area_times_pi = 0.f;
};

MTS_IMPLEMENT_CLASS_VARIANT(AreaLight, Emitter)
MTS_EXPORT_PLUGIN(AreaLight, "Point emitter")
NAMESPACE_END(mitsuba)
