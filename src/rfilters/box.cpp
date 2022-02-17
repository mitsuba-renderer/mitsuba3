#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _rfilter-box:

Box filter (:monosp:`box`)
--------------------------

This is the fastest, but also about the worst possible reconstruction filter,
since it is prone to severe aliasing. It is included mainly for completeness,
though some rare situations may warrant its use.

.. tabs::
    .. code-tab:: xml
        :name: box-rfilter

        <rfilter type="box"/>

    .. code-tab:: python

        'type': 'box',

 */

template <typename Float, typename Spectrum>
class BoxFilter final : public ReconstructionFilter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(ReconstructionFilter, init_discretization, m_radius)
    MI_IMPORT_TYPES()

    BoxFilter(const Properties &props) : Base(props) {
        m_radius = .5f;
        init_discretization();
    }

    Float eval(Float x, Mask /* active */) const override {
        return dr::select(x >= -.5f && x < .5f, Float(1.f), Float(0.f));
    }

    std::string to_string() const override { return "BoxFilter[]"; }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(BoxFilter, ReconstructionFilter)
MI_EXPORT_PLUGIN(BoxFilter, "Box filter");
NAMESPACE_END(mitsuba)
