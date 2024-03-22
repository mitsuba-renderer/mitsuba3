#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/volume.h>
#include <mitsuba/render/volumegrid.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-area:

Area light (:monosp:`area`)
---------------------------

.. pluginparameters::

 * - radiance
   - |spectrum| or |texture|
   - Specifies the emitted radiance in units of power per unit area per unit steradian.
   - |exposed|, |differentiable|

This plugin implements an area light, i.e. a light source that emits
diffuse illumination from the exterior of an arbitrary shape.
Since the emission profile of an area light is completely diffuse, it
has the same apparent brightness regardless of the observer's viewing
direction. Furthermore, since it occupies a nonzero amount of space, an
area light generally causes scene objects to cast soft shadows.

To create an area light source, simply instantiate the desired
emitter shape and specify an :monosp:`area` instance as its child:

.. tabs::
    .. code-tab:: xml
        :name: sphere-light

        <shape type="sphere">
            <emitter type="area">
                <rgb name="radiance" value="1.0"/>
            </emitter>
        </shape>

    .. code-tab:: python

        'type': 'sphere',
        'emitter': {
            'type': 'area',
            'radiance': {
                'type': 'rgb',
                'value': 1.0,
            }
        }

 */

template <typename Float, typename Spectrum>
class VolumeLight final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_shape, m_medium, m_needs_sample_2_3d)
    MI_IMPORT_TYPES(Scene, Shape, Texture, Volume)

    VolumeLight(const Properties &props) : Base(props) {
        if (props.has_property("to_world"))
            Throw("Found a 'to_world' transformation -- this is not allowed. "
                  "The volume light inherits this transformation from its parent "
                  "shape.");

        m_radiance = props.volume<Volume>("radiance", 0.f);
        m_needs_sample_2_3d = true;

        m_flags = +EmitterFlags::Medium;

        dr::set_attr(this, "flags", m_flags);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("radiance", m_radiance.get(), +ParamFlags::Differentiable);
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        return m_radiance->eval(si, active);
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time,
                                          Float wavelength_sample,
                                          const Point3f &spatial_sample,
                                          const Point2f &direction_sample,
                                          Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spatial component
        auto [ps, pos_weight] = sample_position(time, spatial_sample, active);

        // 2. Sample directional component
        Vector3f local = warp::square_to_uniform_sphere(direction_sample);

        // 3. Sample spectral component
        SurfaceInteraction3f si(ps, dr::zeros<Wavelength>());
        auto [wavelength, wav_weight] =
            sample_wavelengths(si, wavelength_sample, active);
        si.time = time;
        si.wavelengths = wavelength;

        Spectrum weight = pos_weight * wav_weight * dr::rcp(warp::square_to_uniform_sphere_pdf(local));

        return { si.spawn_ray(si.to_world(local)),
                 depolarizer<Spectrum>(weight) };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point3f &sample, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);
        Assert(m_shape, "Can't sample from a volume emitter without an associated Shape.");

        auto ds = m_shape->sample_direction_volume(it, sample, active);
        ds.emitter = this;

        auto si = dr::zeros<SurfaceInteraction3f>();
        si.time = ds.time;
        si.p = ds.p;
        si.wavelengths = it.wavelengths;
        si.shape = m_shape;
        si.n = ds.n;
        active &= ds.pdf > 0.f;

        UnpolarizedSpectrum spec = dr::select(active, m_radiance->eval(si, active) / ds.pdf, 0.0f);

        return { ds, depolarizer<Spectrum>(spec) & active };
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        MI_MASK_ARGUMENT(active);

        Float pdf = m_shape->pdf_direction_volume(it, ds, active);

        return dr::select(active, pdf, 0.f);
    }

    Spectrum eval_direction(const Interaction3f &it,
                            const DirectionSample3f &ds,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        SurfaceInteraction3f si(ds, it.wavelengths);
        UnpolarizedSpectrum spec = m_radiance->eval(si, active);

        return dr::select(active, depolarizer<Spectrum>(spec), 0.f);
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point3f &sample,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSamplePosition, active);
        Assert(m_shape, "Cannot sample from a volume emitter without an associated Shape.");

        auto ps = m_shape->sample_position_volume(time, sample, active);
        auto weight = dr::select(active && (ps.pdf > 0.f), dr::rcp(ps.pdf), 0.f);

        return { ps, weight };
    }

    Float pdf_position(const PositionSample3f &ps,
                       Mask active = true) const override {
        return m_shape->pdf_position_volume(ps, active);
    };

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &_si, Float sample,
                       Mask active) const override {

        if (dr::none_or<false>(active))
            return { dr::zeros<Wavelength>(), dr::zeros<UnpolarizedSpectrum>() };

        if constexpr (is_spectral_v<Spectrum>) {
            SurfaceInteraction3f si(_si);
            si.wavelengths = MI_CIE_MIN + (MI_CIE_MAX - MI_CIE_MIN) * sample;
            return { si.wavelengths,
                     eval(si, active) * (MI_CIE_MAX - MI_CIE_MIN) };
        } else {
            DRJIT_MARK_USED(sample);
            auto value = eval(_si, active);
            return { dr::empty<Wavelength>(), value };
        }
    }

    ScalarBoundingBox3f bbox() const override { return m_shape->bbox(); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "VolumeLight[" << std::endl
            << "  radiance = " << string::indent(m_radiance) << "," << std::endl
            << "  surface_area = ";
        if (m_shape) oss << m_shape->surface_area();
        else         oss << "  <no shape attached!>";
        oss << "," << std::endl;
        if (m_medium) oss << string::indent(m_medium);
        else         oss << "  <no medium attached!>";
        oss << std::endl << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    ref<Volume> m_radiance;
};

MI_IMPLEMENT_CLASS_VARIANT(VolumeLight, Emitter)
MI_EXPORT_PLUGIN(VolumeLight, "Volume emitter")
NAMESPACE_END(mitsuba)
