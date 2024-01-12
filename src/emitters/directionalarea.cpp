#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-directionalarea:

Directional area light (:monosp:`directionalarea`)
--------------------------------------------------

.. pluginparameters::

 * - radiance
   - |spectrum|
   - Specifies the emitted radiance in units of power per unit area per unit steradian.
   - |exposed|, |differentiable|

Similar to an area light, but emitting only in the normal direction.

.. note::
    This can only be rendered correctly with a particle tracer, since rays
    traced from the camera and surfaces have zero probability of connecting
    with this emitter at exactly the correct angle.

.. tabs::
    .. code-tab:: xml
        :name: sphere-directional-light

        <shape type="sphere">
            <emitter type="directionalarea">
                <rgb name="radiance" value="1.0"/>
            </emitter>
        </shape>

    .. code-tab:: python

        'type': 'sphere',
        'emitter': {
            'type': 'directionalarea',
            'radiance': {
                'type': 'rgb',
                'value': 1.0,
            }
        }

 */

template <typename Float, typename Spectrum>
class DirectionalArea final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, set_shape, m_flags, m_shape, m_medium,
                    m_needs_sample_3)
    MI_IMPORT_TYPES(Scene, Shape, Mesh, Texture)

    DirectionalArea(const Properties &props) : Base(props) {
        if (props.has_property("to_world"))
            Throw("Found a 'to_world' transformation -- this is not allowed. "
                  "The area light inherits this transformation from its parent "
                  "shape.");

        m_radiance = props.texture_d65<Texture>("radiance", 1.f);
        m_needs_sample_3 = false;

        m_flags = EmitterFlags::Surface | EmitterFlags::DeltaDirection;
        if (m_radiance->is_spatially_varying())
            m_flags |= +EmitterFlags::SpatiallyVarying;
    }

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
        callback->put_object("radiance", m_radiance.get(), +ParamFlags::Differentiable);
    }

    void set_shape(Shape *shape) override {
        Base::set_shape(shape);
        m_area = m_shape->surface_area();
        dr::make_opaque(m_area);
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2,
                                          const Point2f & /*sample3*/,
                                          Mask active) const override {
        if constexpr (drjit::is_jit_v<Float>) {
            if (!m_shape)
                return { dr::zeros<Ray3f>(), 0.f };
        } else {
            Assert(m_shape,
                   "Cannot sample from a directionalarea emitter without an "
                   "associated Shape.");
        }

        // 1. Sample spatial component
        PositionSample3f ps = m_shape->sample_position(time, sample2);

        // 2. Directional component is the normal vector at that position.
        const Vector3f d = ps.n;

        // 3. Sample spectral component
        SurfaceInteraction3f si(ps, dr::zeros<Wavelength>());
        auto [wavelength, wav_weight] = sample_wavelengths(si, wavelength_sample, active);
        si.time = time;
        si.wavelengths = wavelength;

        return { si.spawn_ray(d), m_area * wav_weight };
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
                     Mask /*active*/) const override {
        return { dr::zeros<DirectionSample3f>(), dr::zeros<Spectrum>() };
    }

    Float pdf_direction(const Interaction3f & /*it*/,
                        const DirectionSample3f & /*ds*/,
                        Mask /*active*/) const override {
        return 0.f;
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f &sample,
                    Mask active) const override {
        if constexpr (drjit::is_jit_v<Float>) {
            if (!m_shape)
                return { dr::zeros<PositionSample3f>(), 0.f };
        } else {
            Assert(m_shape, "Can't sample from a directionalarea emitter "
                            "without an associated Shape.");
        }

        PositionSample3f ps = m_shape->sample_position(time, sample, active);
        Float weight        = dr::select(ps.pdf > 0.f, dr::rcp(ps.pdf), 0.f);
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

    MI_DECLARE_CLASS()
private:
    ref<Texture> m_radiance;
    Float m_area = 0.f;
};

MI_IMPLEMENT_CLASS_VARIANT(DirectionalArea, Emitter)
MI_EXPORT_PLUGIN(DirectionalArea, "Directional area emitter");
NAMESPACE_END(mitsuba)
