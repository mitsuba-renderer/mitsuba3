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

        return unpolarized<Spectrum>(m_radiance->eval(mi, active));
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2, const Point2f &sample3,
                                          Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        MediumInteraction3f mi = ek::zero<MediumInteraction3f>();

        // 1. Two strategies to sample spatial component based on 'm_radiance'
        // Currently only supports homogeneous sampling within the bounding box of the shape
        ScalarBoundingBox3f shape_bbox = m_shape->bbox();
        ScalarPoint3f samples = ScalarPoint3f(sample2.x(), sample2.y(), sample3.x());
        PositionSample3f ps = ek::zero<PositionSample3f>();
        ps.p = shape_bbox.min + samples * shape_bbox.max;
        ps.pdf = 1.f / shape_bbox.volume();
        mi = MediumInteraction3f(ps, ek::zero<Wavelength>());

        // 2. Sample directional component
        Vector3f local = warp::square_to_cosine_hemisphere(sample3);

        Wavelength wavelength;
        Spectrum spec_weight;

        if constexpr (is_spectral_v<Spectrum>) {
            wavelength = MTS_WAVELENGTH_MIN + (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN) * sample3.y();
            spec_weight = (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN);
        } else {
            wavelength = ek::zero<Wavelength>();
            spec_weight = m_radiance->eval(mi, active);
            ENOKI_MARK_USED(wavelength_sample);
        }

        return { Ray3f(mi.p, mi.to_world(local), time, wavelength),
                 unpolarized<Spectrum>(spec_weight) * (ek::Pi<Float> / ps.pdf) };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);
        Assert(m_shape, "Can't sample from a volume emitter without an associated Shape.");
        DirectionSample3f ds;
        Spectrum spec;
        
        // Volume is uniform, try to importance sample the shape wrt. solid angle at 'it'
        ds = m_shape->sample_direction(it, sample, active);
        ScalarBoundingBox3f shape_bbox = m_shape->bbox();
        auto ray = Ray3f(it.p, ds.d, it.time, it.wavelengths);
        auto [aabb_its, mint, maxt] = shape_bbox.ray_intersect(ray);
        Float dist3 = ek::squared_norm(ds.d);
        Float dist2 = mint + sample.y() * (maxt - mint);
        ds.dist     = dist3;
        ds.d /= ek::sqrt(dist2);
        active &= shape_bbox.contains((Point3f) it.p + ds.d*ds.dist);
        ds.pdf *= 1.f / (maxt - mint);
        ds.uv = Point2f(mint, maxt);
        ek::masked(ds.pdf, ds.dist == 0.f || !(ek::isfinite(ds.dist))) = 0.f;

        MediumInteraction3f mi = MediumInteraction3f(ds, it.wavelengths);
        spec = m_radiance->eval(mi, active) / ds.pdf;

        ds.emitter = this;
        return { ds, unpolarized<Spectrum>(spec) & active };
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        Float dp = ek::dot(ds.d, ds.n);
        active &= dp < 0.f;

        Float value = m_shape->pdf_direction(it, ds, active);
        value *= 1.f / (ds.uv.y() - ds.uv.x());

        return ek::select(active, value, 0.f);
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
