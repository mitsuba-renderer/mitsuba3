#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-distant:

Distant directional emitter (:monosp:`directional`)
---------------------------------------------------

.. pluginparameters::

    * - irradiance
      - |spectrum|
      - Spectral irradiance, which corresponds to the amount of spectral power
        per unit area received by a hypothetical surface normal to the specified
        direction.
    * - to_world
      - |transform|
      - Emitter-to-world transformation matrix.
    * - direction
      - |vector|
      - Alternative (and exclusive) to `to_world`. Direction towards which the
        emitter is radiating in world coordinates.

This emitter plugin implements a distant directional source which radiates a
specified power per unit area along a fixed direction. By default, the emitter
radiates in the direction of the positive Z axis, i.e. :math:`(0, 0, 1)`.

*/

MTS_VARIANT class DirectionalEmitter final : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Emitter, m_flags, m_world_transform, m_needs_sample_3)
    MTS_IMPORT_TYPES(Scene, Texture)

    DirectionalEmitter(const Properties &props) : Base(props) {
        /* Until `set_scene` is called, we have no information
           about the scene and default to the unit bounding sphere. */
        m_bsphere = ScalarBoundingSphere3f(ScalarPoint3f(0.f), 1.f);

        if (props.has_property("direction")) {
            if (props.has_property("to_world"))
                Throw("Only one of the parameters 'direction' and 'to_world' "
                      "can be specified at the same time!'");

            ScalarVector3f direction(normalize(props.vector3f("direction")));
            auto [up, unused] = coordinate_system(direction);

            m_world_transform =
                new AnimatedTransform(ScalarTransform4f::look_at(
                    ScalarPoint3f(0.0f), ScalarPoint3f(direction), up));
        }

        m_flags      = EmitterFlags::Infinite | EmitterFlags::DeltaDirection;
        m_irradiance = props.texture<Texture>("irradiance", Texture::D65(1.f));
        m_needs_sample_3 = false;
    }

    void set_scene(const Scene *scene) override {
        m_bsphere = scene->bbox().bounding_sphere();
        m_bsphere.radius =
            max(math::RayEpsilon<Float>,
                m_bsphere.radius * (1.f + math::RayEpsilon<Float>) );
    }

    Spectrum eval(const SurfaceInteraction3f & /*si*/,
                  Mask /*active*/) const override {
        return 0.f;
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &spatial_sample,
                                          const Point2f & /*direction_sample*/,
                                          Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spectrum
        auto [wavelengths, weight] =
            sample_wavelength<Float, Spectrum>(wavelength_sample);

        // 2. Sample spatial component
        const Transform4f &trafo = m_world_transform->eval(time, active);
        Point2f offset =
            warp::square_to_uniform_disk_concentric(spatial_sample);
        Vector3f perp_offset =
            trafo.transform_affine(Vector3f{ offset.x(), offset.y(), 0.f });

        // 3. Set ray direction
        Vector3f d = trafo.transform_affine(Vector3f{ 0.f, 0.f, 1.f });

        return std::make_pair(
            Ray3f(m_bsphere.center + (perp_offset - d) * m_bsphere.radius, d,
                  time, wavelengths),
            unpolarized<Spectrum>(weight) *
                (math::Pi<Float> * sqr(m_bsphere.radius)));
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f & /*sample*/,
                     Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        Vector3f d = m_world_transform->eval(it.time, active)
                         .transform_affine(Vector3f{ 0.f, 0.f, 1.f });
        Float dist = 2.f * m_bsphere.radius;

        DirectionSample3f ds;
        ds.p      = it.p - d * dist;
        ds.n      = d;
        ds.uv     = Point2f(0.f);
        ds.time   = it.time;
        ds.pdf    = 1.f;
        ds.delta  = true;
        ds.object = this;
        ds.d      = -d;
        ds.dist   = dist;

        SurfaceInteraction3f si = zero<SurfaceInteraction3f>();
        si.wavelengths          = it.wavelengths;

        // No need to divide by the PDF here (always equal to 1.f)
        return std::make_pair(
            ds, unpolarized<Spectrum>(m_irradiance->eval(si, active)));
    }

    Float pdf_direction(const Interaction3f & /*it*/,
                        const DirectionSample3f & /*ds*/,
                        Mask /*active*/) const override {
        return 0.f;
    }

    ScalarBoundingBox3f bbox() const override {
        /* This emitter does not occupy any particular region
           of space, return an invalid bounding box */
        return ScalarBoundingBox3f();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("irradiance", m_irradiance.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "DirectionalEmitter[" << std::endl
            << "  irradiance = " << string::indent(m_irradiance) << ","
            << std::endl
            << "  bsphere = " << m_bsphere << "," << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()

protected:
    ref<Texture> m_irradiance;
    ScalarBoundingSphere3f m_bsphere;
};

MTS_IMPLEMENT_CLASS_VARIANT(DirectionalEmitter, Emitter)
MTS_EXPORT_PLUGIN(DirectionalEmitter, "Distant emitter")
NAMESPACE_END(mitsuba)
