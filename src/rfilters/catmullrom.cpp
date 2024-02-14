#include <mitsuba/core/rfilter.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _rfilter-catmullrom:

Catmull-Rom filter (:monosp:`catmullrom`)
-----------------------------------------

Special version of the Mitchell-Netravali filter with constants B and C configured
to match the Catmull-Rom spline. It usually does a better job at at preserving sharp
features at the cost of more ringing.

.. tabs::
    .. code-tab:: xml
        :name: catmullrom-rfilter

        <rfilter type="catmullrom"/>

    .. code-tab:: python

        'type': 'catmullrom',

 */

template <typename Float, typename Spectrum>
class CatmullRomFilter final : public ReconstructionFilter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(ReconstructionFilter, init_discretization, m_radius)

    CatmullRomFilter(const Properties &props) : Base(props) {
        m_radius = 2.f;
        init_discretization();
    }

    Float eval(Float x, dr::mask_t<Float> /* active */) const override {
        x = dr::abs(x);

        Float x2 = dr::square(x), x3 = x2*x,
              B = 0.f, C = .5f;

        Float result = (1.f / 6.f) * dr::select(
           x < 1,
           (12.f - 9.f * B - 6.f * C) * x3 +
               (-18.f + 12.f * B + 6.f * C) * x2 + (6.f - 2.f * B),
           (-B - 6.f * C) * x3 + (6.f * B + 30.f * C) * x2 +
               (-12.f * B - 48.f * C) * x + (8.f * B + 24.f * C)
        );

        return dr::select(x < 2.f, result, 0.f);
    }

    std::string to_string() const override {
        return tfm::format("CatmullRomFilter[radius=%f]", m_radius);
    }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(CatmullRomFilter, ReconstructionFilter)
MI_EXPORT_PLUGIN(CatmullRomFilter, "Catmull-Rom filter");
NAMESPACE_END(mitsuba)
