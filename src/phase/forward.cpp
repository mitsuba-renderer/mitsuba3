#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/phase.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _phase-forward:

Forward-scattering phase function (:monosp:`forward`)
-----------------------------------------------------

This phase function is a delta function to simulate a fully
forward scattering medium.

*/

template <typename Float, typename Spectrum>
class ForwardPhaseFunction final : public PhaseFunction<Float, Spectrum> {
public:
    MI_IMPORT_BASE(PhaseFunction, m_flags, m_components)
    MI_IMPORT_TYPES(PhaseFunctionContext)

    ForwardPhaseFunction(const Properties & props) : Base(props) {
        m_flags = +PhaseFunctionFlags::Delta;
        dr::set_attr(this, "flags", m_flags);
        m_components.push_back(m_flags);
    }

    std::pair<Vector3f, Float> sample(const PhaseFunctionContext & /*ctx*/,
                                      const MediumInteraction3f & mi,
                                      Float /*sample1*/,
                                      const Point2f & /*sample2*/,
                                      Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::PhaseFunctionSample, active);
        return { -mi.wi, 1.f };
    }

    Float eval(const PhaseFunctionContext & /* ctx */, const MediumInteraction3f & /* mi */,
               const Vector3f &/*wo*/, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::PhaseFunctionEvaluate, active);
        return 0.f;
    }

    std::string to_string() const override { return "ForwardPhaseFunction[]"; }

    MI_DECLARE_CLASS()
private:
};

MI_IMPLEMENT_CLASS_VARIANT(ForwardPhaseFunction, PhaseFunction)
MI_EXPORT_PLUGIN(ForwardPhaseFunction, "Forward phase function")
NAMESPACE_END(mitsuba)
