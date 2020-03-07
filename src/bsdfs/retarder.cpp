#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-retarder:

Linear retarder material (:monosp:`retarder`)
-----------------------------------------------

.. pluginparameters::

 * - theta
   - |spectrum| or |texture|
   - Specifies the rotation angle (in degrees) of the retarder around the optical axis (Default: 0.0)
 * - delta
   - |spectrum| or |texture|
   - Specifies the retardance (in degrees) where 360 degrees is equivalent to a full wavelength. (Default: 90.0)

This material simulates an ideal linear retarder useful to test polarization aware
light transport or to conduct virtual optical experiments. The fast axis of the
retarder is aligned with the *U*-direction of the underlying surface parameterization.
For non-perpendicular incidence, a cosine falloff term is applied to the retardance.

This plugin can be used to instantiate the  common special cases of
*half-wave plates* (with ``delta=180``) and *quarter-wave plates* (with ``delta=90``).

The following XML snippet describes a quarter-wave plate material:

.. code-block:: xml
    :name: retarder

    <bsdf type="retarder">
        <spectrum name="delta" value="90"/>
    </bsdf>

Apart from a change of polarization, light does not interact with this material
in any way and does not change its direction.
Internally, this is implemented as a forward-facing Dirac delta distribution.
Note that the standard :ref:`path tracer <integrator-path>` does not have a good sampling strategy to deal with this,
but the (:ref:`volumetric path tracer <integrator-volpath>`) does.

In *unpolarized* rendering modes, the behaviour defaults to non-polarizing
transparent material similar to the :ref:`null <bsdf-null>` BSDF plugin.

*/
template <typename Float, typename Spectrum>
class LinearRetarder final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture)

    LinearRetarder(const Properties &props) : Base(props) {
        m_theta = props.texture<Texture>("theta", 0.f);
        // As default, instantiate as a quarter-wave plate
        m_delta = props.texture<Texture>("delta", 90.f);

        m_flags = BSDFFlags::FrontSide | BSDFFlags::BackSide | BSDFFlags::Null;
        m_components.push_back(m_flags);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                             Float /* sample1 */, const Point2f &/* sample2 */,
                                             Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        BSDFSample3f bs = zero<BSDFSample3f>();
        bs.wo = -si.wi;
        bs.pdf = 1.f;
        bs.eta = 1.f;
        bs.sampled_type = +BSDFFlags::Null;
        bs.sampled_component = 0;

        if constexpr (is_polarized_v<Spectrum>) {
            // Query rotation angle
            UnpolarizedSpectrum theta = deg_to_rad(m_theta->eval(si, active));

            // Query phase difference
            UnpolarizedSpectrum delta = deg_to_rad(m_delta->eval(si, active));

            // Approximate angle-of-incidence behaviour with a cosine falloff
            delta *= abs(Frame3f::cos_theta(si.wi));

            // Get standard Mueller matrix for a linear polarizer.
            Spectrum M = mueller::linear_retarder(delta);

            // Rotate optical element by specified angle
            M = mueller::rotated_element(theta, M);

            // Forward direction is always away from light source.
            Vector3f forward = ctx.mode == TransportMode::Radiance ? si.wi : -si.wi;

            // Rotate in/out basis of M s.t. it aligns with BSDF coordinate frame
            M = mueller::rotate_mueller_basis_collinear(M, forward,
                                                        Vector3f(1.f, 0.f, 0.f),
                                                        mueller::stokes_basis(forward));
            return { bs, M };
        } else {
            return { bs, 1.f };
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

        if constexpr (is_polarized_v<Spectrum>) {
            // Query rotation angle
            UnpolarizedSpectrum theta = deg_to_rad(m_theta->eval(si, active));

            // Query phase difference
            UnpolarizedSpectrum delta = deg_to_rad(m_delta->eval(si, active));

            // Approximate angle-of-incidence behaviour with a cosine falloff
            delta *= abs(Frame3f::cos_theta(si.wi));

            // Get standard Mueller matrix for a linear polarizer.
            Spectrum M = mueller::linear_retarder(delta);

            // Rotate optical element by specified angle
            M = mueller::rotated_element(theta, M);

            // Forward direction is always away from light source.
            Vector3f forward = si.wi;   // Note: when tracing Importance, this should be reversed.

            // Rotate in/out basis of M s.t. it aligns with BSDF coordinate frame
            M = mueller::rotate_mueller_basis_collinear(M, forward,
                                                        Vector3f(1.f, 0.f, 0.f),
                                                        mueller::stokes_basis(forward));
            return M;
        } else {
            return 1.f;
        }
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("theta", m_theta.get());
        callback->put_object("delta", m_delta.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "LinearPolarizer[" << std::endl
            << "  theta = " << string::indent(m_theta) << std::endl
            << "  delta = " << string::indent(m_delta) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Texture> m_theta;
    ref<Texture> m_delta;
};

MTS_IMPLEMENT_CLASS_VARIANT(LinearRetarder, BSDF)
MTS_EXPORT_PLUGIN(LinearRetarder, "Linear retarder material")
NAMESPACE_END(mitsuba)