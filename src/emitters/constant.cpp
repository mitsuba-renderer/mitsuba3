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
        m_surface_area = 4.f * ek::Pi<ScalarFloat>;

        m_radiance = props.texture<Texture>("radiance", Texture::D65(1.f));
        m_flags = +EmitterFlags::Infinite;
        ek::set_attr(this, "flags", m_flags);
    }

    void set_scene(const Scene *scene) override {
        if (scene->bbox().valid()) {
            m_bsphere = scene->bbox().bounding_sphere();
            m_bsphere.radius =
                ek::max(math::RayEpsilon<Float>,
                        m_bsphere.radius * (1.f + math::RayEpsilon<Float>) );
        } else {
            m_bsphere.center = 0.f;
            m_bsphere.radius = math::RayEpsilon<Float>;
        }
        m_surface_area = 4.f * ek::Pi<ScalarFloat> * ek::sqr(m_bsphere.radius);
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        return depolarizer<Spectrum>(m_radiance->eval(si, active));
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2, const Point2f &sample3,
                                          Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spatial component
        Vector3f v0 = warp::square_to_uniform_sphere(sample2);
        Point3f origin = m_bsphere.center + v0 * m_bsphere.radius;

        // 2. Sample directional component
        Vector3f v1 = warp::square_to_cosine_hemisphere(sample3);
        Vector3f direction = Frame3f(-v0).to_world(v1);

        // 3. Sample spectrum
        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
        si.t    = 0.f;
        si.time = time;
        si.p    = origin;
        si.uv   = sample2;
        auto [wavelengths, weight] = sample_wavelengths(si, wavelength_sample, active);

        /* Note: removed a 1/cos_theta term compared to `square_to_cosine_hemisphere`
         * because we are not sampling from a surface here. */
        ScalarFloat inv_pdf = m_surface_area * ek::Pi<ScalarFloat>;

        return std::make_pair(Ray3f(origin, direction, time, wavelengths),
                              depolarizer<Spectrum>(weight) * inv_pdf);
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        Vector3f d = warp::square_to_uniform_sphere(sample);
        // Needed when the reference point is on the sensor, which is not part of the bbox
        Float radius = ek::max(m_bsphere.radius, ek::norm(it.p - m_bsphere.center));
        Float dist = 2.f * radius;

        DirectionSample3f ds;
        ds.p        = it.p + d * dist;
        ds.n        = -d;
        ds.uv       = sample;
        ds.time     = it.time;
        ds.pdf      = warp::square_to_uniform_sphere_pdf(d);
        ds.delta    = false;
        ds.endpoint = this;
        ds.d        = d;
        ds.dist     = dist;

        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
        si.wavelengths = it.wavelengths;

        return {
            ds,
            depolarizer<Spectrum>(m_radiance->eval(si, active)) / ds.pdf
        };
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &ds,
                        Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        return warp::square_to_uniform_sphere_pdf(ds.d);
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        return m_radiance->sample_spectrum(
            si, math::sample_shifted<Wavelength>(sample), active);
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float /*time*/, const Point2f & /*sample*/,
                    Mask /*active*/) const override {
        if constexpr (ek::is_jit_array_v<Float>) {
            // When vcalls are recorded in symbolic mode, we can't throw an exception,
            // even though this result will be unused.
            return { ek::zero<PositionSample3f>(),
                     ek::full<Float>(ek::NaN<ScalarFloat>) };
        } else {
            NotImplementedError("sample_position");
        }
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
            << "  bsphere = " << string::indent(m_bsphere) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    ref<Texture> m_radiance;
    ScalarBoundingSphere3f m_bsphere;

    /// Surface area of the bounding sphere
    ScalarFloat m_surface_area;
};

MTS_IMPLEMENT_CLASS_VARIANT(ConstantBackgroundEmitter, Emitter)
MTS_EXPORT_PLUGIN(ConstantBackgroundEmitter, "Constant background emitter")
NAMESPACE_END(mitsuba)
