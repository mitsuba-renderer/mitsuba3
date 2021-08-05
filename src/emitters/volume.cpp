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

        m_flags = +EmitterFlags::Volume;
        if (m_radiance->is_spatially_varying())
            m_flags |= +EmitterFlags::SpatiallyVarying;
        ek::set_attr(this, "flags", m_flags);
    }

    Spectrum eval(const MediumInteraction3f &mi, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        return depolarizer<Spectrum>(m_radiance->eval(mi, active));
    }

    // Samples a 3d point within the volume defined by a given shape
    PositionSample3f sample_radiance_point(const Point3f &sample, const Mask active = true) const {
        ScalarBoundingBox3f shape_bbox = m_shape->bbox();
        PositionSample3f ps = ek::zero<PositionSample3f>();
        ek::masked(ps.p, active) = shape_bbox.min + sample * (shape_bbox.max - shape_bbox.min);
        ek::masked(ps.pdf, active) = 1.f / shape_bbox.volume();
        Log(Debug, "bbox: %s, sample: %s, p: %s, pdf: %s", shape_bbox, sample, ps.p, ps.pdf);
        return ps;
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point3f &sample2, const Point2f &sample3,
                                          Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        MediumInteraction3f mi = ek::zero<MediumInteraction3f>();

        // 1. Two strategies to sample spatial component based on 'm_radiance'
        // Currently only supports homogeneous sampling within the bounding box of the shape
        auto ps = this->sample_radiance_point(sample2, active);
        mi = MediumInteraction3f(ps, ek::zero<Wavelength>());

        // 2. Sample directional component
        Vector3f local = warp::square_to_uniform_sphere(sample3);

        Wavelength wavelength;
        Spectrum spec_weight;

        if constexpr (is_spectral_v<Spectrum>) {
            wavelength = MTS_WAVELENGTH_MIN + (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN) * wavelength_sample;
            spec_weight = (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN);
        } else {
            wavelength = ek::zero<Wavelength>();
            spec_weight = m_radiance->eval(mi, active);
            ENOKI_MARK_USED(wavelength_sample);
        }

        return { Ray3f(mi.p, mi.to_world(local), time, wavelength),
                 depolarizer<Spectrum>(spec_weight) * ps.pdf };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point3f &sample, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);
        Assert(m_shape, "Can't sample from a volume emitter without an associated Shape.");
        Spectrum spec;
        
        MTS_MASK_ARGUMENT(active);

        DirectionSample3f ds(this->sample_radiance_point(sample, active));
        ds.d = ds.p - it.p;

        Float dist_squared = ek::squared_norm(ds.d);
        ds.dist = ek::sqrt(dist_squared);
        ds.d /= ds.dist;
        ds.n = ds.d;

        // // First term is the distance term and the second term is the solid angle term
        // // based on the sample point
        Float x = dist_squared;
        ds.pdf *= ek::select(ek::isfinite(x), x, 0.f);
        ek::masked(ds.pdf, ds.dist == 0.f || !(ek::isfinite(ds.dist))) = 0.f;

        // // Volume is uniform, try to importance sample the shape wrt. solid angle at 'it'

        // PositionSample3f ps = this->sample_bbox_point(sample, active);
        // ds.d = ps.p - it.p;
        // Float dist = ek::squared_norm(ds.d);
        // ds.dist = ek::sqrt(dist);
        // ds.d /= ds.dist;
        // ds.p = it.p;
        // ds.time = it.time;
        // ds.delta = false;
        // ds.pdf = ps.pdf * warp::square_to_uniform_hemisphere_pdf(ds.d);
        // ek::masked(ds.pdf, ds.dist == 0.f || !(ek::isfinite(ds.dist))) = 0.f;

        MediumInteraction3f mi = MediumInteraction3f(ds, it.wavelengths);
        spec = m_radiance->eval(mi, active) / ds.pdf;
        // ek::masked(spec, ds.pdf == 0.f) = 0.f;

        ds.emitter = this;
        active &= mi.is_valid();

        Log(Debug, "Emission of volume is finite: %s ~ %s ~ pdf: %s ~ maxt: %s, mint: %s, maxt - mint: %s ~ active: %s", spec,  ek::isfinite(spec), ds.pdf, mi.maxt, mi.mint, mi.maxt - mi.mint, active);

        return { ds, depolarizer<Spectrum>(spec) & active };
    }

    Float pdf_direction(const Interaction3f &/*it*/, const DirectionSample3f &ds,
                        Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        MTS_MASK_ARGUMENT(active);

        Float pdf = 1.f / this->m_shape->bbox().volume();

        pdf *= (ds.dist * ds.dist);

        return pdf;
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
};

MTS_IMPLEMENT_CLASS_VARIANT(VolumeLight, Emitter)
MTS_EXPORT_PLUGIN(VolumeLight, "Volume emitter")
NAMESPACE_END(mitsuba)
