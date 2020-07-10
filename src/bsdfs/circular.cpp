#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-circular:

Circular polarizer material (:monosp:`circular`)
-----------------------------------------------

.. pluginparameters::

 * - theta
   - |spectrum| or |texture|
   - Specifies the rotation angle (in degrees) of the polarizer around the optical axis (Default: 0.0)
 * - transmittance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular transmission. (Default: 1.0)
 * - left_handed
   - |bool|
   - Flag to switch between left and right circular polarization. (Default: |false|, i.e. right circular polarizer)

This material simulates an ideal circular polarizer useful to test polarization aware
light transport or to conduct virtual optical experiments. To rotate the polarizer,
either the parameter ``theta`` can be used, or alternative a rotation can be applied
directly to the associated shape.

The following XML snippet describes a left circular polarizer material:

.. code-block:: xml
    :name: circular

    <bsdf type="circular">
        <boolean name="left_handed" value="true"/>
    </bsdf>

Apart from a change of polarization, light does not interact with this material
in any way and does not change its direction.
Internally, this is implemented as a forward-facing Dirac delta distribution.
Note that the standard :ref:`path tracer <integrator-path>` does not have a good
sampling strategy to deal with this, but the (:ref:`volumetric path tracer <integrator-volpath>`) does.

In *unpolarized* rendering modes, the behaviour defaults to non-polarizing
transparent material similar to the :ref:`null <bsdf-null>` BSDF plugin.

*/
template <typename Float, typename Spectrum>
class CircularPolarizer final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture)

    CircularPolarizer(const Properties &props) : Base(props) {
        m_theta = props.texture<Texture>("theta", 0.f);
        m_transmittance = props.texture<Texture>("transmittance", 1.f);
        m_left_handed = props.bool_("left_handed", false);

        m_flags = BSDFFlags::FrontSide | BSDFFlags::BackSide | BSDFFlags::Null;
        m_components.push_back(m_flags);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                             Float /* sample1 */, const Point2f &/* sample2 */,
                                             Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        BSDFSample3f bs = ek::zero<BSDFSample3f>();
        bs.wo = -si.wi;
        bs.pdf = 1.f;
        bs.eta = 1.f;
        bs.sampled_type = +BSDFFlags::Null;
        bs.sampled_component = 0;

        UnpolarizedSpectrum transmittance = m_transmittance->eval(si, active);

        if constexpr (is_polarized_v<Spectrum>) {
            // Query rotation angle
            UnpolarizedSpectrum theta = deg_to_rad(m_theta->eval(si, active));

            // Combine linear polarizer and quarter wave plate
            Spectrum LP  = mueller::linear_polarizer(1.f);
            Spectrum QWP = mueller::linear_retarder(0.5f*ek::Pi<Float>);
            UnpolarizedSpectrum rot = m_left_handed ? 3.f*ek::Pi<Float>/4.f : ek::Pi<Float>/4.f;
            QWP = mueller::rotated_element(rot, QWP);
            Spectrum M = QWP * LP;

            // Rotate optical element by specified angle
            M = mueller::rotated_element(theta, M);

            // Forward direction is always away from light source.
            Vector3f forward = ctx.mode == TransportMode::Radiance ? si.wi : -si.wi;

            // Rotate in/out basis of M s.t. it alignes with BSDF coordinate frame
            M = mueller::rotate_mueller_basis_collinear(M, forward,
                                                        Vector3f(1.f, 0.f, 0.f),
                                                        mueller::stokes_basis(forward));

            // Handle potential absorption if transmittance < 1.0
            M *= mueller::absorber(transmittance);

            return { bs, M };
        } else {
            return { bs, 0.5f*transmittance };
        }
    }

    Spectrum eval(const BSDFContext &/* ctx */, const SurfaceInteraction3f &/* si */,
                  const Vector3f &/* wo */, Mask /* active */) const override {
        return 0.f;
    }

    Float pdf(const BSDFContext &/* ctx */, const SurfaceInteraction3f &/* si */,
              const Vector3f &/* wo */, Mask /* active */) const override {
        return 0.f;
    }

    Spectrum eval_null_transmission(const SurfaceInteraction3f &si, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        UnpolarizedSpectrum transmittance = m_transmittance->eval(si, active);
        if constexpr (is_polarized_v<Spectrum>) {
            // Query rotation angle
            UnpolarizedSpectrum theta = deg_to_rad(m_theta->eval(si, active));

            // Combine linear polarizer and quarter wave plate
            Spectrum LP  = mueller::linear_polarizer(1.f);
            Spectrum QWP = mueller::linear_retarder(0.5f*ek::Pi<Float>);
            UnpolarizedSpectrum rot = m_left_handed ? 3.f*ek::Pi<Float>/4.f : ek::Pi<Float>/4.f;
            QWP = mueller::rotated_element(rot, QWP);
            Spectrum M = QWP * LP;

            // Rotate optical element by specified angle
            M = mueller::rotated_element(theta, M);

            // Forward direction is always away from light source.
            Vector3f forward = si.wi;   // Note: when tracing Importance, this should be reversed.

            // Rotate in/out basis of M s.t. it alignes with BSDF coordinate frame
            M = mueller::rotate_mueller_basis_collinear(M, forward,
                                                        Vector3f(1.f, 0.f, 0.f),
                                                        mueller::stokes_basis(forward));

            // Handle potential absorption if transmittance < 1.0
            M *= mueller::absorber(transmittance);

            return M;
        } else {
            return 0.5f*transmittance;
        }
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("theta", m_theta.get());
        callback->put_object("transmittance", m_transmittance.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "CircularPolarizer[" << std::endl
            << "  theta = " << string::indent(m_theta) << std::endl
            << "  transmittance = " << string::indent(m_transmittance) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Texture> m_theta;
    ref<Texture> m_transmittance;
    bool m_left_handed;
};

MTS_IMPLEMENT_CLASS_VARIANT(CircularPolarizer, BSDF)
MTS_EXPORT_PLUGIN(CircularPolarizer, "Circular polarizer material")
NAMESPACE_END(mitsuba)