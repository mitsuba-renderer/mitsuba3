#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _rfilter-mitchell:

Mitchell filter (:monosp:`mitchell`)
------------------------------------

Separable cubic spline reconstruction filter by Mitchell and Netravali :cite:`MitchellNetravali88`.
This is often a good compromise between sharpness and ringing.

This plugin has two :paramtype:`float`-valued parameters :paramtype:`B` and
:paramtype:`C` that correspond to the two parameters in the original paper.
By default, these are set to the recommended value of :math:`1/3`, but can be
tweaked if desired.

 */

template <typename Float, typename Spectrum>
class MitchellNetravaliFilter final : public ReconstructionFilter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(ReconstructionFilter, init_discretization, m_radius)
    MTS_IMPORT_TYPES()

    MitchellNetravaliFilter(const Properties &props) : Base(props) {
        // Filter radius
        m_radius = 2.f;
        // B parameter from the paper
        m_b = props.float_("B", 1.f / 3.f);
        // C parameter from the paper
        m_c = props.float_("C", 1.f / 3.f);
        init_discretization();
    }

    Float eval(Float x, mask_t<Float> /* active */) const override {
        x = abs(x);
        Float x2 = sqr(x), x3 = x2*x;

        Float result = (1.f / 6.f) * select(
           x < 1,
           (12.f - 9.f * m_b - 6.f * m_c) * x3 +
               (-18.f + 12.f * m_b + 6.f * m_c) * x2 + (6.f - 2.f * m_b),
           (-m_b - 6.f * m_c) * x3 + (6.f * m_b + 30.f * m_c) * x2 +
               (-12.f * m_b - 48.f * m_c) * x + (8.f * m_b + 24.f * m_c)
        );

        return select(x < 2.f, result, 0.f);
    }

    std::string to_string() const override {
        return tfm::format("MitchellNetravaliFilter[radius=%f, B=%f, C=%f]", m_radius, m_b, m_c);
    }

    MTS_DECLARE_CLASS()
protected:
    ScalarFloat m_b, m_c;
};

MTS_IMPLEMENT_CLASS_VARIANT(MitchellNetravaliFilter, ReconstructionFilter)
MTS_EXPORT_PLUGIN(MitchellNetravaliFilter, "Mitchell-Netravali filter");
NAMESPACE_END(mitsuba)
