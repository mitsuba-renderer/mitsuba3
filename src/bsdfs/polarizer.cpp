#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-polarizer:

Linear polarizer material (:monosp:`polarizer`)
-----------------------------------------------

.. pluginparameters::

 * - theta
   - |spectrum| or |texture|
   - Specifies the rotation angle (in degrees) of the polarizer around the optical axis (Default: 0.0)
   - |exposed|, |differentiable|, |discontinuous|

 * - transmittance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular transmission. (Default: 1.0)
   - |exposed|, |differentiable|

 * - polarizing
   - |bool|
   - Optional flag to disable polarization changes in order to use this as a neutral density filter,
     even in polarized render modes. (Default: |true|, i.e. act as polarizer)

This material simulates an ideal linear polarizer useful to test polarization aware
light transport or to conduct virtual optical experiments. The absorbing axis of the
polarizer is aligned with the *V*-direction of the underlying surface parameterization.
To rotate the polarizer, either the parameter ``theta`` can be used, or alternative
a rotation can be applied directly to the associated shape.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_polarizer_aligned.jpg
   :caption: Two aligned polarizers. The average intensity is reduced by a factor of 2.
.. subfigure:: ../../resources/data/docs/images/render/bsdf_polarizer_absorbing.jpg
   :caption: Two polarizers offset by 90 degrees. All transmitted light is absorbed.
.. subfigure:: ../../resources/data/docs/images/render/bsdf_polarizer_middle.jpg
   :caption: Two polarizers offset by 90 degrees, with a third polarizer in between at 45 degrees. Some light is transmitted again.
.. subfigend::
   :label: fig-polarizer

The following XML snippet describes a linear polarizer material with a rotation
of 90 degrees.

.. tabs::
    .. code-tab:: xml
        :name: polarizer

        <bsdf type="polarizer">
            <spectrum name="theta" value="90"/>
        </bsdf>

    .. code-tab:: python

        'type': 'polarizer',
        'theta': {
            'type': 'spectrum',
            'value': 90
        }

Apart from a change of polarization, light does not interact with this material
in any way and does not change its direction.
Internally, this is implemented as a forward-facing Dirac delta distribution.
Note that the standard :ref:`path tracer <integrator-path>` does not have a good sampling strategy to deal with this,
but the (:ref:`volumetric path tracer <integrator-volpath>`) does.

In *unpolarized* rendering modes, the behavior defaults to a non-polarizing
transmitting material that absorbs 50% of the incident illumination.

*/
template <typename Float, typename Spectrum>
class LinearPolarizer final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    LinearPolarizer(const Properties &props) : Base(props) {
        m_theta = props.texture<Texture>("theta", 0.f);
        m_transmittance = props.texture<Texture>("transmittance", 1.f);
        m_polarizing = props.get<bool>("polarizing", true);

        m_flags = BSDFFlags::FrontSide | BSDFFlags::BackSide | BSDFFlags::Null;
        m_components.push_back(m_flags);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("theta",         m_theta.get(),         ParamFlags::Differentiable | ParamFlags::Discontinuous);
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
            if (!m_polarizing) {
                return { bs, mueller::absorber(0.5f*transmittance) };
            }

            using Vector3fS = Vector<UnpolarizedSpectrum, 3>;

            // Query rotation angle
            UnpolarizedSpectrum theta = dr::deg_to_rad(m_theta->eval(si, active));
            auto [sin_theta, cos_theta] = dr::sincos(theta);

            // Get standard Mueller matrix for a linear polarizer.
            Spectrum M = mueller::linear_polarizer(1.f);

            /* The `forward` direction here is always along the direction that
               light travels. This is needed for the coordinate system rotation
               below. */
            Vector3fS forward = ctx.mode == TransportMode::Radiance ? si.wi : -si.wi;

            /* To account for non-perpendicular incidence, we compute the effective
               transmitting axis based on "The polarization properties of a tilted polarizer"
               by Korger et al. 2013. */
            Vector3fS a_axis(sin_theta, cos_theta, 0.f);
            Vector3fS eff_a_axis = dr::normalize(a_axis - dr::dot(a_axis, forward)*forward);
            Vector3fS eff_t_axis = dr::cross(forward, eff_a_axis);

            // Rotate in/out basis of M s.t. to standard basis
            M = mueller::rotate_mueller_basis_collinear(M, forward,
                                                        eff_t_axis,
                                                        mueller::stokes_basis(forward));

            // Handle potential absorption if transmittance < 1.0
            M *= mueller::absorber(transmittance);

            return { bs, M };
        } else {
            return { bs, 0.5f * transmittance };
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
            if (!m_polarizing) {
                return mueller::absorber(0.5f*transmittance);
            }

            using Vector3fS = Vector<UnpolarizedSpectrum, 3>;

            // Query rotation angle
            UnpolarizedSpectrum theta = dr::deg_to_rad(m_theta->eval(si, active));
            auto [sin_theta, cos_theta] = dr::sincos(theta);

            // Get standard Mueller matrix for a linear polarizer.
            Spectrum M = mueller::linear_polarizer(1.f);

            /* The `forward` direction here is always along the direction that
               light travels. This is needed for the coordinate system rotation
               below. */
            Vector3fS forward = si.wi;   // Note: Should be reversed for TransportMode::Importance.

            /* To account for non-perpendicular incidence, we compute the effective
               transmitting axis based on "The polarization properties of a tilted polarizer"
               by Korger et al. 2013. */
            Vector3fS a_axis(sin_theta, cos_theta, 0.f);
            Vector3fS eff_a_axis = dr::normalize(a_axis - dr::dot(a_axis, forward)*forward);
            Vector3fS eff_t_axis = dr::cross(forward, eff_a_axis);

            // Rotate in/out basis of M s.t. to standard basis
            M = mueller::rotate_mueller_basis_collinear(M, forward,
                                                        eff_t_axis,
                                                        mueller::stokes_basis(forward));

            // Handle potential absorption if transmittance < 1.0
            M *= mueller::absorber(transmittance);

            return M;
        } else {
            return 0.5f * transmittance;
        }
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "LinearPolarizer[" << std::endl
            << "  theta = " << string::indent(m_theta) << std::endl
            << "  transmittance = " << string::indent(m_transmittance) << std::endl
            << "  polarizing = " << m_polarizing << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    bool m_polarizing;
    ref<Texture> m_theta;
    ref<Texture> m_transmittance;
};

MI_IMPLEMENT_CLASS_VARIANT(LinearPolarizer, BSDF)
MI_EXPORT_PLUGIN(LinearPolarizer, "Linear polarizer material")
NAMESPACE_END(mitsuba)
