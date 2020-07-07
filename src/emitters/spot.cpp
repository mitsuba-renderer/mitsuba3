#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-spot:

Spot light source (:monosp:`spot`)
------------------------------------

.. pluginparameters::

 * - intensity
   - |spectrum|
   - Specifies the maximum radiant intensity at the center in units of power per unit steradian. (Default: 1).
     This cannot be spatially varying (e.g. have bitmap as type).

 * - cutoff_angle
   - |float|
   - Cutoff angle, beyond which the spot light is completely black (Default: 20 degrees)

 * - beam_width
   - |float|
   - Subtended angle of the central beam portion (Default: :math:`cutoff_angle \times 3/4`)

 * - texture
   - |texture|
   - An optional texture to be projected along the spot light. This must be spatially varying (e.g. have bitmap as type).

 * - to_world
   - |transform|
   - Specifies an optional emitter-to-world transformation.  (Default: none, i.e. emitter space = world space)

This plugin provides a spot light with a linear falloff. In its local coordinate system, the spot light is
positioned at the origin and points along the positive Z direction. It can be conveniently reoriented
using the lookat tag, e.g.:

.. code-block:: xml
    :name: spot-light

    <emitter type="spot">
        <transform name="to_world">
            <!-- Orient the light so that points from (1, 1, 1) towards (1, 2, 1) -->
            <lookat origin="1, 1, 1" target="1, 2, 1"/>
        </transform>
    </emitter>

The intensity linearly ramps up from cutoff_angle to beam_width (both specified in degrees),
after which it remains at the maximum value. A projection texture may optionally be supplied.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/emitter_spot_no_texture.jpg
   :caption: Two spot lights with different colors and no texture specified.
.. subfigure:: ../../resources/data/docs/images/render/emitter_spot_texture.jpg
   :caption: A spot light with a texture specified.
.. subfigend::
   :label: fig-spot-light

 */

template <typename Float, typename Spectrum>
class SpotLight final : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Emitter, m_flags, m_medium, m_world_transform)
    MTS_IMPORT_TYPES(Scene, Texture)

    SpotLight(const Properties &props) : Base(props) {
        m_flags = +EmitterFlags::DeltaPosition;
        m_intensity = props.texture<Texture>("intensity", Texture::D65(1.f));
        m_texture = props.texture<Texture>("texture", Texture::D65(1.f));

        if (m_intensity->is_spatially_varying())
            Throw("The parameter 'intensity' cannot be spatially varying (e.g. bitmap type)!");

        if (props.has_property("texture")) {
            if (!m_texture->is_spatially_varying())
                Throw("The parameter 'texture' must be spatially varying (e.g. bitmap type)!");
            m_flags |= +EmitterFlags::SpatiallyVarying;
        }

        m_cutoff_angle = props.float_("cutoff_angle", 20.0f);
        m_beam_width = props.float_("beam_width", m_cutoff_angle * 3.0f / 4.0f);
        m_cutoff_angle = deg_to_rad(m_cutoff_angle);
        m_beam_width = deg_to_rad(m_beam_width);
        m_inv_transition_width = 1.0f / (m_cutoff_angle - m_beam_width);
        m_cos_cutoff_angle = cos(m_cutoff_angle);
        m_cos_beam_width = cos(m_beam_width);
        Assert(m_cutoff_angle >= m_beam_width);
        m_uv_factor = tan(m_cutoff_angle);
    }

    UnpolarizedSpectrum falloff_curve(const Vector3f &d, Wavelength wavelengths, Mask active) const {
        SurfaceInteraction3f si = zero<SurfaceInteraction3f>();
        si.wavelengths = wavelengths;
        UnpolarizedSpectrum result = m_intensity->eval(si, active);

        Vector3f local_dir = normalize(d);
        Float cos_theta = local_dir.z();

        if (m_texture->is_spatially_varying()) {
            si.uv = Point2f(.5f + .5f * local_dir.x() / (local_dir.z() * m_uv_factor),
                            .5f + .5f * local_dir.y() / (local_dir.z() * m_uv_factor));
            result *= m_texture->eval(si, active);
        }

        UnpolarizedSpectrum beam_res = select(cos_theta >= m_cos_beam_width, result,
                               result * ((m_cutoff_angle - acos(cos_theta)) * m_inv_transition_width));

        return select(cos_theta <= m_cos_cutoff_angle, UnpolarizedSpectrum(0.0f), beam_res);
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &spatial_sample,
                                          const Point2f & /*dir_sample*/,
                                          Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample directional component
        const Transform4f &trafo = m_world_transform->eval(time, active);
        Vector3f local_dir = warp::square_to_uniform_cone(spatial_sample, (Float)m_cos_cutoff_angle);
        Float pdf_dir = warp::square_to_uniform_cone_pdf(local_dir, (Float)m_cos_cutoff_angle);

        // 2. Sample spectrum
        auto [wavelengths, spec_weight] = m_intensity->sample_spectrum(
            zero<SurfaceInteraction3f>(),
            math::sample_shifted<Wavelength>(wavelength_sample), active);

        UnpolarizedSpectrum falloff_spec = falloff_curve(local_dir, wavelengths, active);

        return { Ray3f(trafo.translation(), trafo * local_dir, time, wavelengths),
                unpolarized<Spectrum>(falloff_spec / pdf_dir) };
    }

    std::pair<DirectionSample3f, Spectrum> sample_direction(const Interaction3f &it,
                                                            const Point2f &/*sample*/,
                                                            Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        Transform4f trafo = m_world_transform->eval(it.time, active);

        DirectionSample3f ds;
        ds.p        = trafo.translation();
        ds.n        = 0.f;
        ds.uv       = 0.f;
        ds.pdf      = 1.0f;
        ds.time     = it.time;
        ds.delta    = true;
        ds.object   = this;
        ds.d        = ds.p - it.p;
        ds.dist     = norm(ds.d);
        Float inv_dist = rcp(ds.dist);
        ds.d        *= inv_dist;
        Vector3f local_d = trafo.inverse() * -ds.d;
        UnpolarizedSpectrum falloff_spec = falloff_curve(local_d, it.wavelengths, active);

        return { ds, unpolarized<Spectrum>(falloff_spec * (inv_dist * inv_dist)) };
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &, Mask) const override {
        return 0.f;
    }

    Spectrum eval(const SurfaceInteraction3f &, Mask) const override { return 0.f; }

    ScalarBoundingBox3f bbox() const override {
        return m_world_transform->translation_bounds();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("intensity", m_intensity.get());
        callback->put_object("texture", m_texture.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SpotLight[" << std::endl
            << "  world_transform = " << string::indent(m_world_transform) << "," << std::endl
            << "  intensity = " << m_intensity << "," << std::endl
            << "  cutoff_angle = " << m_cutoff_angle << "," << std::endl
            << "  beam_width = " << m_beam_width << "," << std::endl
            << "  texture = " << (m_texture ? string::indent(m_texture) : "")
            << "  medium = " << (m_medium ? string::indent(m_medium) : "")
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Texture> m_intensity;
    ref<Texture> m_texture;
    ScalarFloat m_beam_width, m_cutoff_angle, m_uv_factor;
    ScalarFloat m_cos_beam_width, m_cos_cutoff_angle, m_inv_transition_width;
};


MTS_IMPLEMENT_CLASS_VARIANT(SpotLight, Emitter)
MTS_EXPORT_PLUGIN(SpotLight, "Spot emitter")
NAMESPACE_END(mitsuba)
