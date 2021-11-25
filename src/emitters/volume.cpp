#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/volume.h>

NAMESPACE_BEGIN(mitsuba)
/**!

.. _emitter-area:

Volume light (:monosp:`area`)
---------------------------

.. pluginparameters::

 * - radiance
   - |spectrum|
   - Specifies the emitted radiance in units of power per unit area per unit steradian.

This plugin implements an area light, i.e. a light source that emits
diffuse illumination from the exterior of an arbitrary shape.
Since the emission profile of an area light is completely diffuse, it
has the same apparent brightness regardless of the observer's viewing
direction. Furthermore, since it occupies a nonzero amount of space, an
area light generally causes scene objects to cast soft shadows.

To create an area light source, simply instantiate the desired
emitter shape and specify an :monosp:`area` instance as its child:

.. code-block:: xml
    :name: sphere-light

    <shape type="sphere">
        <emitter type="area">
            <spectrum name="radiance" value="1.0"/>
        </emitter>
    </shape>

 */

template <typename Float, typename Spectrum>
class VolumeLight final : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Emitter, m_flags, m_shape, m_medium)
    MTS_IMPORT_TYPES(Scene, Shape, Texture, Volume)

    VolumeLight(const Properties &props) : Base(props) {
        if (props.has_property("to_world"))
            Throw("Found a 'to_world' transformation -- this is not allowed. "
                  "The volume light inherits this transformation from its parent "
                  "shape.");

        m_radiance = props.volume<Volume>("radiance", 1.f);
        m_scale = props.float_("scale", 1.0f);

        m_flags = +EmitterFlags::Volume;
        if (m_radiance->is_spatially_varying())
            m_flags |= +EmitterFlags::SpatiallyVarying;
        ek::set_attr(this, "flags", m_flags);
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        MediumInteraction3f mi = MediumInteraction3f(si);
        return m_scale * depolarizer<Spectrum>(m_radiance->eval(mi, active));
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2, const Point2f &sample3,
                                          const Float &volume_sample,
                                          Mask active) const override {
        NotImplementedError("sample_ray");
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, const Float &volume_sample, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);
        Assert(m_shape, "Can't sample from a volume emitter without an associated Shape.");
        Spectrum spec;
        
        MTS_MASK_ARGUMENT(active);

        auto [ps, pdf] = this->sample_position(it.time, sample, volume_sample, active);
        DirectionSample3f ds(ps);
        ds.d = ds.p - it.p;

        Float dist_squared = ek::squared_norm(ds.d);
        ds.dist = ek::sqrt(dist_squared);
        ds.d /= ds.dist;
        ds.n = ds.d;
        ds.pdf = pdf;

        // // First term is the distance term and the second term is the solid angle term
        // // based on the sample point
        Float x = dist_squared;
        ds.pdf = ek::select(ek::isfinite(x), ds.pdf * x, 0.f);

        MediumInteraction3f mi = MediumInteraction3f(ds, it.wavelengths);
        spec = m_scale * m_radiance->eval(mi, active) / ds.pdf;

        ds.emitter = this;
        active &= mi.is_valid();

        return { ds, depolarizer<Spectrum>(spec) & active };
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f &sample_,
                    const Float &volume_sample, 
                    Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSamplePosition, active);
        Assert(m_shape, "Cannot sample from a volume emitter without an associated Shape.");

        ScalarBoundingBox3f shape_bbox = m_shape->bbox();
        PositionSample3f ps = ek::zero<PositionSample3f>();
        Point3f sample = Point3f(sample_.x(), sample_.y(), volume_sample);
        ek::masked(ps.p, active) = shape_bbox.min + sample * (shape_bbox.max - shape_bbox.min);
        ek::masked(ps.pdf, active) = 1.f / shape_bbox.volume();

        Float weight = ek::select(active & (ps.pdf > 0.f), ek::rcp(ps.pdf), Float(0.f));
        return { ps, weight };
    }

    Float pdf_direction(const Interaction3f &/*it*/, const DirectionSample3f &ds,
                        Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        Mask is_inside = this->m_shape->bbox().contains(ds.p);
        Float pdf = ek::select(is_inside, ds.dist * ds.dist / this->m_shape->bbox().volume(), 0.f);

        active &= is_inside;

        return pdf;
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        Wavelength wav;
        SurfaceInteraction3f si2(si);
        if (is_spectral_v<Spectrum>) {
            wav = MTS_WAVELENGTH_MIN +
                 (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN) * sample;
        } else {
            wav = ek::zero<Wavelength>();
        }
        si2.wavelengths = wav;
        auto weight = m_radiance->eval(si2, active);
        // Log(Debug, "%s %s %s", weight, si2, m_radiance);
        if (is_spectral_v<Spectrum> && m_radiance->is_spatially_varying()) {
            weight *= m_d65->eval(si2, active);
        }
        return { wav, weight };
    }

    ScalarBoundingBox3f bbox() const override { return m_shape->bbox(); }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("radiance", m_radiance.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "VolumeLight[" << std::endl
            << "  radiance = " << string::indent(m_radiance) << "," << std::endl
            << "  volume = ";
        if (m_shape) oss << m_shape->bbox().volume();
        else         oss << "  <no shape attached!>";
        oss << "," << std::endl;
        if (m_medium) oss << string::indent(m_medium);
        else         oss << "  <no medium attached!>";
        oss << std::endl << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Volume> m_radiance;
<<<<<<< HEAD
    ref<Texture> m_d65;
=======
    float m_scale;
>>>>>>> 903e3175e672ab2caa8a1bc8da8cf8e1e24a3d9e
};

MTS_IMPLEMENT_CLASS_VARIANT(VolumeLight, Emitter)
MTS_EXPORT_PLUGIN(VolumeLight, "Volume emitter")
NAMESPACE_END(mitsuba)
