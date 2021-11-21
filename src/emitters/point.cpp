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

            m_to_world = ScalarTransform4f::translate(ScalarVector3f(props.get<ScalarPoint3f>("position")));
            ek::make_opaque(m_to_world);
        }

        m_intensity = props.texture<Texture>("intensity", Texture::D65(1.f));
        m_needs_sample_3 = false;
        m_flags = +EmitterFlags::DeltaPosition;
        ek::set_attr(this, "flags", m_flags);
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f & /*pos_sample*/, const Point2f &dir_sample,
                                          Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        auto si = ek::zero<SurfaceInteraction3f>();
        si.t    = 0.f;
        si.p    = m_to_world.value().translation();
        si.time = time;
        auto [wavelengths, weight] =
            sample_wavelengths(si, wavelength_sample, active);

        weight *= 4.f * ek::Pi<Float>;

        Ray3f ray(si.p, warp::square_to_uniform_sphere(dir_sample), time,
                  wavelengths);

        return { ray, depolarizer<Spectrum>(weight) };
    }

    std::pair<DirectionSample3f, Spectrum> sample_direction(const Interaction3f &it,
                                                            const Point2f & /*sample*/,
                                                            Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        DirectionSample3f ds;
        ds.p     = m_to_world.value().translation();
        ds.n     = 0.f;
        ds.uv    = 0.f;
        ds.time  = it.time;
        ds.pdf   = 1.f;
        ds.delta = true;
        ds.emitter = this;
        ds.d     = ds.p - it.p;
        ds.dist  = ek::norm(ds.d);

        Float inv_dist = ek::rcp(ds.dist);
        ds.d *= inv_dist;

        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
        si.wavelengths = it.wavelengths;

        UnpolarizedSpectrum spec =
            m_intensity->eval(si, active) * ek::sqr(inv_dist);

        return { ds, depolarizer<Spectrum>(spec) };
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &,
                        Mask) const override {
        return 0.f;
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f & /*sample*/,
                    Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSamplePosition, active);

        PositionSample3f ps(
            /* position */ m_to_world.value().translation(),
            /* normal (invalid) */ ScalarVector3f(0.f),
            /*uv*/ Point2f(0.5f), time, /*pdf*/ 1.f, /*delta*/ true
        );
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
        ScalarPoint3f p = m_to_world.scalar() * ScalarPoint3f(0.f);
        return ScalarBoundingBox3f(p, p);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("intensity", m_intensity.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "PointLight[" << std::endl
            << "  to_world = " << string::indent(m_to_world) << "," << std::endl
            << "  intensity = " << m_intensity << "," << std::endl
            << "  medium = " << (m_medium ? string::indent(m_medium) : "")
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Texture> m_intensity;
};


MTS_IMPLEMENT_CLASS_VARIANT(PointLight, Emitter)
MTS_EXPORT_PLUGIN(PointLight, "Point emitter")
NAMESPACE_END(mitsuba)
