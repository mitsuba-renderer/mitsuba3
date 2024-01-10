#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/phase.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _phase-isotropic:

Isotropic phase function (:monosp:`isotropic`)
----------------------------------------------

This phase function simulates completely uniform scattering,
where all directionality is lost after a single scattering
interaction. It does not have any parameters.

.. tabs::
    .. code-tab:: xml

        <phase type="isotropic" />

    .. code-tab:: python

        'type': 'isotropic'
*/

template <typename Float, typename Spectrum>
class IsotropicPhaseFunction final : public PhaseFunction<Float, Spectrum> {
public:
    MI_IMPORT_BASE(PhaseFunction, m_flags, m_components)
    MI_IMPORT_TYPES(PhaseFunctionContext)

    IsotropicPhaseFunction(const Properties & props) : Base(props) {
        m_flags = +PhaseFunctionFlags::Isotropic;
        m_components.push_back(m_flags);
    }

    std::tuple<Vector3f, Spectrum, Float> sample(const PhaseFunctionContext & /* ctx */,
                                                 const MediumInteraction3f & /* mi */,
                                                 Float /* sample1 */,
                                                 const Point2f &sample2,
                                                 Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::PhaseFunctionSample, active);

        auto wo  = warp::square_to_uniform_sphere(sample2);
        auto pdf = warp::square_to_uniform_sphere_pdf(wo);
        return { wo, 1.f, pdf };
    }

    std::pair<Spectrum, Float> eval_pdf(const PhaseFunctionContext & /* ctx */,
                                        const MediumInteraction3f & /* mi */,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::PhaseFunctionEvaluate, active);
        Float pdf = warp::square_to_uniform_sphere_pdf(wo);
        return { pdf, pdf };
    }

    std::string to_string() const override { return "IsotropicPhaseFunction[]"; }

    MI_DECLARE_CLASS()
private:
};

MI_IMPLEMENT_CLASS_VARIANT(IsotropicPhaseFunction, PhaseFunction)
MI_EXPORT_PLUGIN(IsotropicPhaseFunction, "Isotropic phase function")
NAMESPACE_END(mitsuba)
