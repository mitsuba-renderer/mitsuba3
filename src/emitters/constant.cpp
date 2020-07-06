#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-constant:

Constant environment emitter (:monosp:`constant`)
-------------------------------------------------

.. pluginparameters::

 * - radiance
   - |spectrum|
   - Specifies the emitted radiance in units of power per unit area per unit steradian.

This plugin implements a constant environment emitter, which surrounds
the scene and radiates diffuse illumination towards it. This is often
a good default light source when the goal is to visualize some loaded
geometry that uses basic (e.g. diffuse) materials.

 */

template <typename Float, typename Spectrum>
class ConstantBackgroundEmitter final : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Emitter, m_flags)
    MTS_IMPORT_TYPES(Scene, Shape, Texture)

    ConstantBackgroundEmitter(const Properties &props) : Base(props) {
        /* Until `set_scene` is called, we have no information
           about the scene and default to the unit bounding sphere. */
        m_bsphere = ScalarBoundingSphere3f(ScalarPoint3f(0.f), 1.f);

        m_radiance = props.texture<Texture>("radiance", Texture::D65(1.f));
        m_flags = +EmitterFlags::Infinite;
    }

    void set_scene(const Scene *scene) override {
        m_bsphere = scene->bbox().bounding_sphere();
        m_bsphere.radius = max(math::RayEpsilon<Float>,
                               m_bsphere.radius * (1.f + math::RayEpsilon<Float>));
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        return unpolarized<Spectrum>(m_radiance->eval(si, active));
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2, const Point2f &sample3,
                                          Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spectrum
        auto [wavelengths, weight] = m_radiance->sample_spectrum(
            zero<SurfaceInteraction3f>(),
            math::sample_shifted<Wavelength>(wavelength_sample), active);

        // 2. Sample spatial component
        Vector3f v0 = warp::square_to_uniform_sphere(sample2);

        // 3. Sample directional component
        Vector3f v1 = warp::square_to_cosine_hemisphere(sample3);

        return std::make_pair(Ray3f(m_bsphere.center + v0 * m_bsphere.radius,
                                    Frame3f(-v0).to_world(v1), time, wavelengths),
                              unpolarized<Spectrum>(weight) * (4.f * sqr(math::Pi<Float> * m_bsphere.radius)));
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        Vector3f d = warp::square_to_uniform_sphere(sample);
        Float dist = 2.f * m_bsphere.radius;

        DirectionSample3f ds;
        ds.p      = it.p + d * dist;
        ds.n      = -d;
        ds.uv     = Point2f(0.f);
        ds.time   = it.time;
        ds.pdf    = warp::square_to_uniform_sphere_pdf(d);
        ds.delta  = false;
        ds.object = this;
        ds.d      = d;
        ds.dist   = dist;

        SurfaceInteraction3f si = zero<SurfaceInteraction3f>();
        si.wavelengths = it.wavelengths;

        return std::make_pair(
            ds,
            unpolarized<Spectrum>(m_radiance->eval(si, active)) / ds.pdf
        );
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &ds,
                        Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        return warp::square_to_uniform_sphere_pdf(ds.d);
    }

    /// This emitter does not occupy any particular region of space, return an invalid bounding box
    ScalarBoundingBox3f bbox() const override {
        return ScalarBoundingBox3f();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("radiance", m_radiance.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "ConstantBackgroundEmitter[" << std::endl
            << "  radiance = " << string::indent(m_radiance) << "," << std::endl
            << "  bsphere = " << m_bsphere << "," << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    ref<Texture> m_radiance;
    ScalarBoundingSphere3f m_bsphere;
};

MTS_IMPLEMENT_CLASS_VARIANT(ConstantBackgroundEmitter, Emitter)
MTS_EXPORT_PLUGIN(ConstantBackgroundEmitter, "Constant background emitter")
NAMESPACE_END(mitsuba)
