#include <mitsuba/core/distr_1d.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/phase.h>

/**!

.. _phase-rayleigh:

Rayleigh phase function (:monosp:`rayleigh`)
-----------------------------------------------

Scattering by particles that are much smaller than the wavelength
of light (e.g. individual molecules in the atmosphere) is well-approximated
by the Rayleigh phase function. This plugin implements an unpolarized
version of this scattering model (*i.e.* the effects of polarization are
ignored). This plugin is useful for simulating scattering in planetary
atmospheres.

This model has no parameters.

.. tabs::
    .. code-tab:: xml

        <phase type="rayleigh" />

    .. code-tab:: python

        'type': 'rayleigh'

*/

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class RayleighPhaseFunction final : public PhaseFunction<Float, Spectrum> {
public:
    MI_IMPORT_BASE(PhaseFunction, m_flags)
    MI_IMPORT_TYPES(PhaseFunctionContext)

    RayleighPhaseFunction(const Properties &props) : Base(props) {
        if constexpr (is_polarized_v<Spectrum>)
            Log(Warn,
                "Polarized version of Rayleigh phase function not implemented, "
                "falling back to scalar version");

        m_flags = +PhaseFunctionFlags::Anisotropic;
    }

    MI_INLINE Float eval_rayleigh(Float cos_theta) const {
        return (3.f / 16.f) * dr::InvPi<Float> * (1.f + dr::square(cos_theta));
    }

    std::tuple<Vector3f, Spectrum, Float> sample(const PhaseFunctionContext & /* ctx */,
                                                 const MediumInteraction3f &mi,
                                                 Float /* sample1 */,
                                                 const Point2f &sample,
                                                 Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::PhaseFunctionSample, active);

        Float z   = 2.f * (2.f * sample.x() - 1.f);
        Float tmp = dr::sqrt(dr::square(z) + 1.f);
        Float A   = dr::cbrt(z + tmp);
        Float B   = dr::cbrt(z - tmp);
        Float cos_theta = A + B;
        Float sin_theta = dr::safe_sqrt(1.0f - dr::square(cos_theta));
        auto [sin_phi, cos_phi] = dr::sincos(dr::TwoPi<Float> * sample.y());

        auto wo = Vector3f{ sin_theta * cos_phi, sin_theta * sin_phi, cos_theta };

        wo = mi.to_world(wo);
        Float pdf = eval_rayleigh(-cos_theta);
        return { wo, 1.f, pdf };
    }

    std::pair<Spectrum, Float> eval_pdf(const PhaseFunctionContext & /* ctx */,
                                        const MediumInteraction3f & mi,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::PhaseFunctionEvaluate, active);
        Float pdf = eval_rayleigh(dot(wo, mi.wi));
        return { pdf, pdf };
    }

    std::string to_string() const override { return "RayleighPhaseFunction[]"; }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(RayleighPhaseFunction, PhaseFunction)
MI_EXPORT_PLUGIN(RayleighPhaseFunction, "Rayleigh phase function")
NAMESPACE_END(mitsuba)
