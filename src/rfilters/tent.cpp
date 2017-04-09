#include <mitsuba/core/rfilter.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Simple tent (triangular) filter. This reconstruction filter never suffers
 * from ringing and usually causes less aliasing than a naive box filter. When
 * rendering scenes with sharp brightness discontinuities, this may be useful;
 * otherwise, negative-lobed filters may be preferable (e.g. Mitchell-Netravali
 * or Lanczos Sinc)
 */
class TentFilter : public ReconstructionFilter {
public:
    TentFilter(const Properties &props) : ReconstructionFilter(props) {
        m_radius = 1.0f;
        init_discretization();
    }

    Float eval(Float x) const override {
        return std::max((Float) 0.0f, 1.0f - std::abs(x / m_radius));
    }

    std::string to_string() const override {
        return tfm::format("TentFilter[radius=%f]", m_radius);
    }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS(TentFilter, ReconstructionFilter);
MTS_EXPORT_PLUGIN(TentFilter, "Tent filter");
NAMESPACE_END(mitsuba)
