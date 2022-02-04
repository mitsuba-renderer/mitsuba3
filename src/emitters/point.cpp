#include <mitsuba/core/bbox.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-point:

Point light source (:monosp:`point`)
------------------------------------

.. pluginparameters::

 * - intensity
   - |spectrum|
   - Specifies the radiant intensity in units of power per unit steradian.

 * - position
   - |point|
   - Alternative parameter for specifying the light source position.
     Note that only one of the parameters :monosp:`to_world` and
     :monosp:`position` can be used at a time.

 * - to_world
   - |transform|
   - Specifies an optional emitter-to-world transformation.  (Default: none,
     i.e. emitter space = world space)

This emitter plugin implements a simple point light source, which
uniformly radiates illumination into all directions.

 */

template <typename Float, typename Spectrum>
class PointLight final : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Emitter, m_flags, m_medium, m_needs_sample_3, m_to_world)
    MTS_IMPORT_TYPES(Scene, Shape, Texture)

    PointLight(const Properties &props) : Base(props) {
        if (props.has_property("position")) {
            if (props.has_property("to_world"))
                Throw("Only one of the parameters 'position' and 'to_world' "
                      "can be specified at the same time!'");

            m_position = props.get<ScalarPoint3f>("position");
        } else {
            m_position = ScalarPoint3f(m_to_world.scalar().translation());
        }

        dr::make_opaque(m_position);

        m_intensity = props.texture<Texture>("intensity", Texture::D65(1.f));
        m_needs_sample_3 = false;
        m_flags = +EmitterFlags::DeltaPosition;
        dr::set_attr(this, "flags", m_flags);
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f & /*pos_sample*/,
                                          const Point2f &dir_sample,
                                          Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        auto [wavelengths, weight] = sample_wavelengths(
            dr::zero<SurfaceInteraction3f>(), wavelength_sample, active);

        weight *= 4.f * dr::Pi<Float>;

        Ray3f ray(m_position.value(),
                  warp::square_to_uniform_sphere(dir_sample), time,
                  wavelengths);

        return { ray, depolarizer<Spectrum>(weight) };
    }

    std::pair<DirectionSample3f, Spectrum> sample_direction(const Interaction3f &it,
                                                            const Point2f & /*sample*/,
                                                            Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        DirectionSample3f ds;
        ds.p       = m_position.value();
        ds.n       = 0.f;
        ds.uv      = 0.f;
        ds.time    = it.time;
        ds.pdf     = 1.f;
        ds.delta   = true;
        ds.emitter = this;
        ds.d       = ds.p - it.p;

        Float dist2    = dr::squared_norm(ds.d),
              inv_dist = dr::rsqrt(dist2);

        // Redundant sqrt (removed by the JIT when the 'dist' field is not used)
        ds.dist = dr::sqrt(dist2);
        ds.d *= inv_dist;

        SurfaceInteraction3f si = dr::zero<SurfaceInteraction3f>();
        si.wavelengths = it.wavelengths;

        UnpolarizedSpectrum spec =
            m_intensity->eval(si, active) * dr::sqr(inv_dist);

        return { ds, depolarizer<Spectrum>(spec) };
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &,
                        Mask) const override {
        return 0.f;
    }

    Spectrum eval_direction(const Interaction3f &it,
                            const DirectionSample3f &ds,
                            Mask active) const override {
        SurfaceInteraction3f si = dr::zero<SurfaceInteraction3f>();
        si.wavelengths = it.wavelengths;

        UnpolarizedSpectrum spec = m_intensity->eval(si, active) *
                                   dr::rcp(dr::squared_norm(ds.p - it.p));

        return depolarizer<Spectrum>(spec);
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f & /*sample*/,
                    Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSamplePosition, active);

        PositionSample3f ps = dr::zero<PositionSample3f>();
        ps.p = m_position.value();
        ps.time = time;
        ps.delta = true;

        return { ps, Float(1.f) };
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        return m_intensity->sample_spectrum(
            si, math::sample_shifted<Wavelength>(sample), active);
    }

    Spectrum eval(const SurfaceInteraction3f &, Mask) const override {
        return 0.f;
    }

    ScalarBoundingBox3f bbox() const override {
        return ScalarBoundingBox3f(m_position.scalar());
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("position", (Point3f &) m_position.value());
        callback->put_object("intensity", m_intensity.get());
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "position")) {
            m_position = m_position.value(); // update scalar part as well
            dr::make_opaque(m_position);
        }
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "PointLight[" << std::endl
            << "  position = " << string::indent(m_position) << "," << std::endl
            << "  intensity = " << m_intensity << "," << std::endl
            << "  medium = " << (m_medium ? string::indent(m_medium) : "none")
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Texture> m_intensity;
    field<Point3f> m_position;
};


MTS_IMPLEMENT_CLASS_VARIANT(PointLight, Emitter)
MTS_EXPORT_PLUGIN(PointLight, "Point emitter")
NAMESPACE_END(mitsuba)
