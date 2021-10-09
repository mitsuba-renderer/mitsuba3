#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Similar to an area light, but emitting only in the normal direction.
 *
 * Note: this can only be rendered correctly with a particle tracer, since
 * rays traced from the camera and surfaces have zero probability of connecting
 * with this emitter at exactly the correct angle.
 */
template <typename Float, typename Spectrum>
class DirectionalArea final : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Emitter, set_shape, m_flags, m_shape, m_medium,
                    m_needs_sample_3)
    MTS_IMPORT_TYPES(Scene, Shape, Mesh, Texture)

    DirectionalArea(const Properties &props) : Base(props) {
        if (props.has_property("to_world"))
            Throw("Found a 'to_world' transformation -- this is not allowed. "
                  "The area light inherits this transformation from its parent "
                  "shape.");

        m_radiance       = props.texture("radiance", Texture::D65(1.f));
        m_needs_sample_3 = false;

        m_flags = EmitterFlags::Surface | EmitterFlags::DeltaDirection;
        if (m_radiance->is_spatially_varying())
            m_flags |= +EmitterFlags::SpatiallyVarying;
        ek::set_attr(this, "flags", m_flags);
    }

    void set_shape(Shape *shape) override {
        Base::set_shape(shape);
        m_area = m_shape->surface_area();
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2,
                                          const Point2f & /*sample3*/,
                                          const Float &/*volume_sample*/,
                                          Mask active) const override {
        // 1. Sample spatial component
        PositionSample3f ps = m_shape->sample_position(time, sample2);

        // 2. Directional component is the normal vector at that position.
        const Vector3f d = ps.n;

        // 3. Sample spectral component
        SurfaceInteraction3f si(ps, ek::zero<Wavelength>());
        auto [wavelength, wav_weight] = sample_wavelengths(si, wavelength_sample, active);

        // Note: ray.mint will ensure we don't immediately self-intersect
        Ray3f ray(ps.p, d, time, wavelength);
        return { ray, m_area * wav_weight };
    }

    /**
     * Current strategy: don't try to connect this emitter
     * observed from the reference point `it`, since it's
     * unlikely to correspond to the surface normal (= the emission
     * direction).
     *
     * TODO: instead, we could try and find the orthogonal projection
     *       and make the connection then. But that would require a
     *       flat surface.
     */
    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f & /*it*/, const Point2f & /*sample*/,
                     const Float &/*volume_sample*/,
                     Mask /*active*/) const override {
        return { ek::zero<DirectionSample3f>(), ek::zero<Spectrum>() };
    }

    Float pdf_direction(const Interaction3f & /*it*/,
                        const DirectionSample3f & /*ds*/,
                        Mask /*active*/) const override {
        return 0.f;
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f &sample,
                    const Float &/*volume_sample*/,
                    Mask active) const override {
        Assert(m_shape, "Can't sample from an area emitter without an associated Shape.");
        PositionSample3f ps = m_shape->sample_position(time, sample, active);
        Float weight        = ek::select(ps.pdf > 0.f, ek::rcp(ps.pdf), 0.f);
        return { ps, weight };
    }

    /**
     * This will always 'fail': since `si.wi` is given,
     * there's zero probability that it is the exact direction of emission.
     */
    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        return m_radiance->sample_spectrum(
            si, math::sample_shifted<Wavelength>(sample), active);
    }

    Spectrum eval(const SurfaceInteraction3f & /*si*/,
                  Mask /*active*/) const override {
        return 0.f;
    }

    ScalarBoundingBox3f bbox() const override { return m_shape->bbox(); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "DirectionalArea[" << std::endl
            << "  radiance = " << string::indent(m_radiance) << "," << std::endl
            << "  surface_area = ";
        if (m_shape)
            oss << m_shape->surface_area();
        else
            oss << "  <no shape attached!>";
        oss << "," << std::endl;
        if (m_medium)
            oss << string::indent(m_medium->to_string());
        else
            oss << "  <no medium attached!>";
        oss << std::endl << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Texture> m_radiance;
    Float m_area = 0.f;
};

MTS_IMPLEMENT_CLASS_VARIANT(DirectionalArea, Emitter)
MTS_EXPORT_PLUGIN(DirectionalArea, "Directional area emitter");
NAMESPACE_END(mitsuba)
