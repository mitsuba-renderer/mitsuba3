#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _rfilter-mitchell:

Mitchell filter (:monosp:`mitchell`)
------------------------------------

.. pluginparameters::

 * - A
   - |float|
   - A parameter in the original paper (Default: :math:`1/3`)
 * - B
   - |float|
   - B parameter in the original paper (Default: :math:`1/3`)

Separable cubic spline reconstruction filter by Mitchell and Netravali
:cite:`MitchellNetravali88`. This is often a good compromise between sharpness
and ringing.

.. tabs::
    .. code-tab:: xml
        :name: mitchell-rfilter

        <rfilter type="mitchell">
            <float name="A" value="0.25"/>
            <float name="B" value="0.55"/>
        </rfilter>

    .. code-tab:: python

        'type': 'mitchell',
        'A': 0.25,
        'B': 0.55

 */

template <typename Float, typename Spectrum>
class MitchellNetravaliFilter final : public ReconstructionFilter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(ReconstructionFilter, init_discretization, m_radius)
    MI_IMPORT_TYPES()

    MitchellNetravaliFilter(const Properties &props) : Base(props) {
        // Filter radius
        m_radius = 2.f;
        // B parameter from the paper
        m_b = props.get<ScalarFloat>("B", 1.f / 3.f);
        // C parameter from the paper
        m_c = props.get<ScalarFloat>("C", 1.f / 3.f);
        init_discretization();
    }

    Float eval(Float x, Mask /* active */) const override {
        x = dr::abs(x);
        Float x2 = dr::square(x), x3 = x2*x;

        ScalarFloat a3 = (12.f - 9.f * m_b - 6.f * m_c),
                    a2 = (-18.f + 12.f * m_b + 6.f * m_c),
                    a0 = (6.f - 2.f * m_b),
                    b3 = (-m_b - 6.f * m_c),
                    b2 = (6.f * m_b + 30.f * m_c),
                    b1 = (-12.f * m_b - 48.f * m_c),
                    b0 = (8.f * m_b + 24.f * m_c);

        Float result = (1.f / 6.f) * dr::select(
           x < 1,
           dr::fmadd(a3, x3, dr::fmadd(a2, x2, a0)),
           dr::fmadd(b3, x3, dr::fmadd(b2, x2, dr::fmadd(b1, x, b0)))
        );

        return dr::select(x < 2.f, result, 0.f);
    }

    std::string to_string() const override {
        return tfm::format("MitchellNetravaliFilter[radius=%f, B=%f, C=%f]", m_radius, m_b, m_c);
    }

    MI_DECLARE_CLASS()
protected:
    ScalarFloat m_b, m_c;
};

MI_IMPLEMENT_CLASS_VARIANT(MitchellNetravaliFilter, ReconstructionFilter)
MI_EXPORT_PLUGIN(MitchellNetravaliFilter, "Mitchell-Netravali filter");
NAMESPACE_END(mitsuba)
