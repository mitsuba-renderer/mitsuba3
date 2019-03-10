#include <mitsuba/core/rfilter.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Simple tent (triangular) filter. This reconstruction filter never suffers
 * from ringing and usually causes less aliasing than a naive box filter. When
 * rendering scenes with sharp brightness discontinuities, this may be useful;
 * otherwise, negative-lobed filters may be preferable (e.g. Mitchell-Netravali
 * or Lanczos Sinc)
 */
class TentFilter final : public ReconstructionFilter {
public:
    TentFilter(const Properties &props) : ReconstructionFilter(props) {
        m_radius = 1.0f;
        m_inv_radius = 1.f / m_radius;
        init_discretization();
    }

    template <typename Value> Value eval_impl(Value x) const {
        return max(0.f, 1.f - abs(x * m_inv_radius));
    }

    std::string to_string() const override {
        return tfm::format("TentFilter[radius=%f]", m_radius);
    }

    MTS_IMPLEMENT_RFILTER_ALL()
    MTS_DECLARE_CLASS()
private:
    Float m_inv_radius;
};

MTS_IMPLEMENT_CLASS(TentFilter, ReconstructionFilter);
MTS_EXPORT_PLUGIN(TentFilter, "Tent filter");
NAMESPACE_END(mitsuba)
