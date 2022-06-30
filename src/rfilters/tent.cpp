#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _rfilter-tent:

Tent filter (:monosp:`tent`)
----------------------------

.. pluginparameters::

 * - radius
   - |float|
   - Specifies the radius of the tent function (Default: 1.0)

Simple tent (triangular) filter. This reconstruction filter never suffers
from ringing and usually causes less aliasing than a naive box filter. When
rendering scenes with sharp brightness discontinuities, this may be useful;
otherwise, negative-lobed filters may be preferable (e.g. Mitchell-Netravali
or Lanczos Sinc).

.. tabs::
    .. code-tab:: xml
        :name: tent-rfilter

        <rfilter type="tent">
            <float name="radius" value="1.25"/>
        </rfilter>

    .. code-tab:: python

        'type': 'tent',
        'radius': 1.25,

 */

template <typename Float, typename Spectrum>
class TentFilter final : public ReconstructionFilter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(ReconstructionFilter, init_discretization, m_radius)
    MI_IMPORT_TYPES()

    TentFilter(const Properties &props) : Base(props) {
        m_radius = props.get<ScalarFloat>("radius", 1.f);
        m_inv_radius = 1.f / m_radius;
        init_discretization();
    }

    Float eval(Float x, dr::mask_t<Float> /* active */) const override {
        return dr::maximum(0.f, 1.f - dr::abs(x * m_inv_radius));
    }

    std::string to_string() const override {
        return tfm::format("TentFilter[radius=%f]", m_radius);
    }

    MI_DECLARE_CLASS()
private:
    ScalarFloat m_inv_radius;
};

MI_IMPLEMENT_CLASS_VARIANT(TentFilter, ReconstructionFilter)
MI_EXPORT_PLUGIN(TentFilter, "Tent filter");
NAMESPACE_END(mitsuba)
