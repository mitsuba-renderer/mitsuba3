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
    MTS_IMPORT_BASE(Emitter, m_flags, m_medium, m_needs_sample_3, m_world_transform)
    MTS_IMPORT_TYPES(Scene, Shape, Texture)

    PointLight(const Properties &props) : Base(props) {
        if (props.has_property("position")) {
            if (props.has_property("to_world"))
                Throw("Only one of the parameters 'position' and 'to_world' "
                      "can be specified at the same time!'");

            m_world_transform = new AnimatedTransform(
                ScalarTransform4f::translate(ScalarVector3f(props.point3f("position"))));
        }

        m_intensity = props.texture<Texture>("intensity", Texture::D65(1.f));
        m_needs_sample_3 = false;
        m_flags = +EmitterFlags::DeltaPosition;
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f & /*pos_sample*/, const Point2f &dir_sample,
                                          Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        auto [wavelengths, spec_weight] = m_intensity->sample_spectrum(
            zero<SurfaceInteraction3f>(),
            math::sample_shifted<Wavelength>(wavelength_sample), active);

        spec_weight *= 4.f * math::Pi<Float>;

        const auto &trafo = m_world_transform->eval(time);

        Ray3f ray(trafo * Point3f(0.f),
                  warp::square_to_uniform_sphere(dir_sample),
                  time, wavelengths);

        return { ray, unpolarized<Spectrum>(spec_weight) };
    }

    std::pair<DirectionSample3f, Spectrum> sample_direction(const Interaction3f &it,
                                                            const Point2f & /*sample*/,
                                                            Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        auto trafo = m_world_transform->eval(it.time, active);

        DirectionSample3f ds;
        ds.p     = trafo.translation();
        ds.n     = 0.f;
        ds.uv    = 0.f;
        ds.time  = it.time;
        ds.pdf   = 1.f;
        ds.delta = true;
        ds.object = this;
        ds.d     = ds.p - it.p;
        ds.dist  = norm(ds.d);

        Float inv_dist = rcp(ds.dist);
        ds.d *= inv_dist;

        SurfaceInteraction3f si = zero<SurfaceInteraction3f>();
        si.wavelengths = it.wavelengths;

        UnpolarizedSpectrum spec =
            m_intensity->eval(si, active) * sqr(inv_dist);

        return { ds, unpolarized<Spectrum>(spec) };
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &,
                        Mask) const override {
        return 0.f;
    }

    Spectrum eval(const SurfaceInteraction3f &, Mask) const override {
        return 0.f;
    }

    ScalarBoundingBox3f bbox() const override {
        return m_world_transform->translation_bounds();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("intensity", m_intensity.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "PointLight[" << std::endl
            << "  world_transform = " << string::indent(m_world_transform) << "," << std::endl
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
