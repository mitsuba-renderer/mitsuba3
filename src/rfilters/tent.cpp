#include <mitsuba/core/rfilter.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _rfilter-tent:

Tent filter (:monosp:`tent`)
----------------------------

Simple tent (triangular) filter. This reconstruction filter never suffers
from ringing and usually causes less aliasing than a naive box filter. When
rendering scenes with sharp brightness discontinuities, this may be useful;
otherwise, negative-lobed filters may be preferable (e.g. Mitchell-Netravali
or Lanczos Sinc).

 */

template <typename Float, typename Spectrum>
class TentFilter final : public ReconstructionFilter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(ReconstructionFilter, init_discretization, m_radius)
    MTS_IMPORT_TYPES()

    TentFilter(const Properties &props) : Base(props) {
        m_radius = 1.f;
        m_inv_radius = 1.f / m_radius;
        init_discretization();
    }

    Float eval(Float x, mask_t<Float> /* active */) const override {
        return max(0.f, 1.f - abs(x * m_inv_radius));
    }

    std::string to_string() const override {
        return tfm::format("TentFilter[radius=%f]", m_radius);
    }

    MTS_DECLARE_CLASS()
private:
    ScalarFloat m_inv_radius;
};

MTS_IMPLEMENT_CLASS_VARIANT(TentFilter, ReconstructionFilter)
MTS_EXPORT_PLUGIN(TentFilter, "Tent filter");
NAMESPACE_END(mitsuba)
