#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-astroobject:

Distant astronomical object (:monosp:`astroobject`)
---------------------------------------------------

.. pluginparameters::

 * - irradiance
   - |spectrum|
   - Spectral irradiance, which corresponds to the amount of spectral power
     per unit area received by a hypothetical surface normal to the specified
     direction.
   - |exposed|, |differentiable|

 * - to_world
   - |transform|
   - Emitter-to-world transformation matrix.
   - |exposed|

 * - direction
   - |vector|
   - Alternative (and exclusive) to `to_world`. Direction towards which the
     emitter is radiating in world coordinates.

 * - angular_diameter
   - |float|
   - Angular diameter of the object in degrees. (Default: 0.5358)

This emitter plugin implements an environment light source simulating a
distant astronomical object. It radiates a specified power per unit area within
a solid angle specified by the `angular_diameter` parameter.  By default, the
emitter radiates in the direction of the positive Z axis, i.e. :math:`(0, 0, 1)`,
with an angular diameter equal to the average apparent size of the Sun from Earth.

.. tabs::
    .. code-tab:: xml
        :name: astroobject-light

        <emitter type="astroobject">
            <vector name="direction" value="1.0, 0.0, 0.0"/>
            <rgb name="irradiance" value="1.0"/>
        </emitter>

    .. code-tab:: python

        'type': 'astroobject',
        'direction': [1.0, 0.0, 0.0],
        'irradiance': {
            'type': 'rgb',
            'value': 1.0,
        }

*/

/* Apparent radius of the sun as seen from the earth (in degrees).
   This is an approximation--the actual value is somewhere between
   0.526 and 0.545 depending on the time of year */
#define SUN_ANGULAR_DIAMETER 0.5358f

MI_VARIANT class AstroObjectEmitter final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_to_world, m_needs_sample_3)
    MI_IMPORT_TYPES(Scene, Texture)

    AstroObjectEmitter(const Properties &props) : Base(props) {
        /* Until `set_scene` is called, we have no information
           about the scene and default to the unit bounding sphere. */
        m_bsphere = ScalarBoundingSphere3f(ScalarPoint3f(0.f), 1.f);

        if (props.has_property("direction")) {
            if (props.has_property("to_world"))
                Throw("Only one of the parameters 'direction' and 'to_world' "
                      "can be specified at the same time!'");
            ScalarVector3f direction(
                dr::normalize(props.get<ScalarVector3f>("direction"))),
                up;
            std::tie(up, std::ignore) = coordinate_system(direction);
            m_direction               = direction;
            m_to_world =
                ScalarTransform4f::look_at(0.f, ScalarPoint3f(direction), up);
            dr::make_opaque(m_to_world);
        }

        ScalarFloat angular_diameter = SUN_ANGULAR_DIAMETER;
        if (props.has_property("angular_diameter"))
            angular_diameter = props.get<ScalarFloat>("angular_diameter");

        ScalarFloat m_angular_radius = dr::deg_to_rad(angular_diameter / 2.f);
        m_cos_angular_radius   = dr::cos(m_angular_radius);
        if (dr::any((1.f <= m_cos_angular_radius) |
                    (m_cos_angular_radius < 0.f))) {
            Throw("Invalid angular diameter specified! (must be in ]0, 180[°)");
        }

        m_omega      = 2.f * dr::Pi<ScalarFloat> * (1.f - m_cos_angular_radius);
        m_irradiance = props.texture_d65<Texture>("irradiance", 1.f);
        Log(Debug,
            "angular_radius: %s; angular_radius_cos: %s; solid angle omega: %s",
            m_angular_radius, m_cos_angular_radius, m_omega);

        if (m_irradiance->is_spatially_varying())
            Throw("Expected a non-spatially varying irradiance spectra!");

        m_needs_sample_3 = false;

        m_flags = +EmitterFlags::Infinite;
        dr::set_attr(this, "flags", m_flags);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("irradiance", m_irradiance.get(),
                             +ParamFlags::Differentiable);
        callback->put_parameter("to_world", *m_to_world.ptr(),
                                +ParamFlags::NonDifferentiable);
    }

    void set_scene(const Scene *scene) override {
        if (scene->bbox().valid()) {
            m_bsphere        = scene->bbox().bounding_sphere();
            m_bsphere.radius = dr::maximum(
                math::RayEpsilon<Float>,
                m_bsphere.radius * (1.f + math::RayEpsilon<Float>) );
        } else {
            m_bsphere.center = 0.f;
            m_bsphere.radius = math::RayEpsilon<Float>;
        }
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        Float cos_dirwwi = dr::dot(si.wi, m_direction);

        return depolarizer<Spectrum>(m_irradiance->eval(si, active) / m_omega) &
               (cos_dirwwi > m_cos_angular_radius);
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample,
                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        Vector3f local_dir =
            warp::square_to_uniform_cone(sample, m_cos_angular_radius);
        Float pdf =
            warp::square_to_uniform_cone_pdf(local_dir, m_cos_angular_radius);
        Vector3f d = m_to_world.value().transform_affine(local_dir);

        // Needed when the reference point is on the sensor, which is not part
        // of the bbox
        Float dist =
            dr::maximum(m_bsphere.radius, dr::norm(it.p - m_bsphere.center)) *
            2.f;

        DirectionSample3f ds;
        ds.p       = it.p - d * dist;
        ds.n       = d;
        ds.uv      = Point2f(0.f);
        ds.time    = it.time;
        ds.pdf     = pdf;
        ds.delta   = false;
        ds.emitter = this;
        ds.d       = -d;
        ds.dist    = dist;

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.wavelengths          = it.wavelengths;

        UnpolarizedSpectrum weight =
            depolarizer<Spectrum>(m_irradiance->eval(si, active)) / m_omega /
            pdf;

        return { ds, weight & active };
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        return m_irradiance->sample_spectrum(
            si, math::sample_shifted<Wavelength>(sample), active);
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float, Float, const Point2f &,
                                          const Point2f &,
                                          Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);
        if constexpr (dr::is_jit_v<Float>) {
            /* Do not throw an exception in JIT-compiled variants. This
               function might be invoked by DrJit's virtual function call
               recording mechanism despite not influencing any actual
               calculation. */
            return { dr::zeros<Ray3f>(), dr::NaN<Spectrum> };
        } else {
            NotImplementedError("sample_ray");
        }
    }

    Float pdf_direction(const Interaction3f & /*it*/,
                        const DirectionSample3f &ds,
                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        Vector3f d = m_to_world.value().inverse().transform_affine(ds.d);
        Float pdf  = warp::square_to_uniform_cone_pdf(d, m_cos_angular_radius);

        return pdf;
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float /*time*/, const Point2f & /*sample*/,
                    Mask /*active*/) const override {
        if constexpr (dr::is_jit_v<Float>) {
            // When vcalls are recorded in symbolic mode, we can't throw an
            // exception, even though this result will be unused.
            return { dr::zeros<PositionSample3f>(),
                     dr::full<Float>(dr::NaN<Float>) };
        } else {
            NotImplementedError("sample_position");
        }
    }

    ScalarBoundingBox3f bbox() const override {
        /* This emitter does not occupy any particular region
           of space, return an invalid bounding box */
        return ScalarBoundingBox3f();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "AstroObjectEmitter[" << std::endl
            << "  irradiance = " << string::indent(m_irradiance) << ","
            << std::endl
            << "  bsphere = " << string::indent(m_bsphere) << "," << std::endl
            << "  cos_angular_radius = " << string::indent(m_cos_angular_radius)
            << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

protected:
    ref<Texture> m_irradiance;
    ScalarBoundingSphere3f m_bsphere;
    Float m_cos_angular_radius;
    Float m_omega;
    ScalarVector3f m_direction;
};

MI_IMPLEMENT_CLASS_VARIANT(AstroObjectEmitter, Emitter)
MI_EXPORT_PLUGIN(AstroObjectEmitter, "Distant Astronomical Object Emitter")
NAMESPACE_END(mitsuba)
