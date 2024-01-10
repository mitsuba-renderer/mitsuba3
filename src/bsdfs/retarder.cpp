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
   - |exposed|, |differentiable|

 * - delta
   - |spectrum| or |texture|
   - Specifies the retardance (in degrees) where 360 degrees is equivalent to a full wavelength. (Default: 90.0)
   - |exposed|, |differentiable|

 * - transmittance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular transmission. (Default: 1.0)
   - |exposed|, |differentiable|

This material simulates an ideal linear retarder useful to test polarization aware
light transport or to conduct virtual optical experiments. The fast axis of the
retarder is aligned with the *U*-direction of the underlying surface parameterization.
For non-perpendicular incidence, a cosine falloff term is applied to the retardance.

This plugin can be used to instantiate the  common special cases of
*half-wave plates* (with ``delta=180``) and *quarter-wave plates* (with ``delta=90``).

The following XML snippet describes a quarter-wave plate material:

.. tabs::
    .. code-tab:: xml
        :name: retarder

        <bsdf type="retarder">
            <spectrum name="delta" value="90"/>
        </bsdf>

    .. code-tab:: python

        'type': 'retarder',
        'delta': {
            'type': 'spectrum',
            'value': 90
        }

Apart from a change of polarization, light does not interact with this material
in any way and does not change its direction.
Internally, this is implemented as a forward-facing Dirac delta distribution.
Note that the standard :ref:`path tracer <integrator-path>` does not have a good sampling strategy to deal with this,
but the (:ref:`volumetric path tracer <integrator-volpath>`) does.

In *unpolarized* rendering modes, the behavior defaults to non-polarizing
transparent material similar to the :ref:`null <bsdf-null>` BSDF plugin.

*/
template <typename Float, typename Spectrum>
class LinearRetarder final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    LinearRetarder(const Properties &props) : Base(props) {
        m_theta = props.texture<Texture>("theta", 0.f);
        // As default, instantiate as a quarter-wave plate
        m_delta = props.texture<Texture>("delta", 90.f);
        m_transmittance = props.texture<Texture>("transmittance", 1.f);

        m_flags = BSDFFlags::FrontSide | BSDFFlags::BackSide | BSDFFlags::Null;
        m_components.push_back(m_flags);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("theta",         m_theta.get(),         +ParamFlags::Differentiable);
        callback->put_object("delta",         m_delta.get(),         +ParamFlags::Differentiable);
        callback->put_object("transmittance", m_transmittance.get(), +ParamFlags::Differentiable);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                             Float /* sample1 */, const Point2f &/* sample2 */,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        BSDFSample3f bs = dr::zeros<BSDFSample3f>();
        bs.wo = -si.wi;
        bs.pdf = 1.f;
        bs.eta = 1.f;
        bs.sampled_type = +BSDFFlags::Null;
        bs.sampled_component = 0;

        UnpolarizedSpectrum transmittance = m_transmittance->eval(si, active);

        if constexpr (is_polarized_v<Spectrum>) {
            // Query rotation angle
            UnpolarizedSpectrum theta = dr::deg_to_rad(m_theta->eval(si, active));

            // Query phase difference
            UnpolarizedSpectrum delta = dr::deg_to_rad(m_delta->eval(si, active));

            // Approximate angle-of-incidence behaviour with a cosine falloff
            Float cos_theta = Frame3f::cos_theta(si.wi);
            delta *= dr::abs(cos_theta);

            // Get standard Mueller matrix for a linear polarizer.
            Spectrum M = mueller::linear_retarder(delta);

            /* Rotate optical element by specified angle. The angle is flipped if
               the element is intersected from the backside. */
            M = mueller::rotated_element(dr::sign(cos_theta) * theta, M);

            /* The `forward` direction here is always along the direction that
               light travels. This is needed for the coordinate system rotation
               below. */
            Vector3f forward = ctx.mode == TransportMode::Radiance ? si.wi : -si.wi;

            // Rotate in/out basis of M s.t. it aligns with BSDF coordinate frame
            M = mueller::rotate_mueller_basis_collinear(M, forward,
                                                        Vector3f(1.f, 0.f, 0.f),
                                                        mueller::stokes_basis(forward));

            // Handle potential absorption if transmittance < 1.0
            M *= mueller::absorber(transmittance);

            return { bs, M };
        } else {
            return { bs, transmittance };
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
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        UnpolarizedSpectrum transmittance = m_transmittance->eval(si, active);

        if constexpr (is_polarized_v<Spectrum>) {
            // Query rotation angle
            UnpolarizedSpectrum theta = dr::deg_to_rad(m_theta->eval(si, active));

            // Query phase difference
            UnpolarizedSpectrum delta = dr::deg_to_rad(m_delta->eval(si, active));

            // Approximate angle-of-incidence behaviour with a cosine falloff
            Float cos_theta = Frame3f::cos_theta(si.wi);
            delta *= dr::abs(cos_theta);

            // Get standard Mueller matrix for a linear polarizer.
            Spectrum M = mueller::linear_retarder(delta);

            /* Rotate optical element by specified angle. The angle is flipped if
               the element is intersected from the backside. */
            M = mueller::rotated_element(dr::sign(cos_theta) * theta, M);

            /* The `forward` direction here is always along the direction that
               light travels. This is needed for the coordinate system rotation
               below. */
            Vector3f forward = si.wi;   // Note: Should be reversed for TransportMode::Importance.

            // Rotate in/out basis of M s.t. it aligns with BSDF coordinate frame
            M = mueller::rotate_mueller_basis_collinear(M, forward,
                                                        Vector3f(1.f, 0.f, 0.f),
                                                        mueller::stokes_basis(forward));

            // Handle potential absorption if transmittance < 1.0
            M *= mueller::absorber(transmittance);

            return M;
        } else {
            return transmittance;
        }
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "LinearRetarder[" << std::endl
            << "  theta = " << string::indent(m_theta) << std::endl
            << "  delta = " << string::indent(m_delta) << std::endl
            << "  transmittance = " << string::indent(m_transmittance) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    ref<Texture> m_theta;
    ref<Texture> m_delta;
    ref<Texture> m_transmittance;
};

MI_IMPLEMENT_CLASS_VARIANT(LinearRetarder, BSDF)
MI_EXPORT_PLUGIN(LinearRetarder, "Linear retarder material")
NAMESPACE_END(mitsuba)
