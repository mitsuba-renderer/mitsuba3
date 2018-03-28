#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * This is a windowed Gaussian filter with configurable standard deviation.
 * It often produces pleasing results, but may introduce too much blurring.
 *
 * When no reconstruction filter is explicitly requested, this is the default
 * choice in Mitsuba.
 */
class GaussianFilter final : public ReconstructionFilter {
public:
    GaussianFilter(const Properties &props) : ReconstructionFilter(props) {
        /* Standard deviation */
        m_stddev = props.float_("stddev", 0.5f);

        /* Cut off after 4 standard deviations */
        m_radius = 4 * m_stddev;

        init_discretization();
    }

    Float eval(Float x) const override {
        Float alpha = -1.f / (2.f * m_stddev * m_stddev);
        return std::max((Float) 0.f,
            std::exp(alpha * x * x) -
            std::exp(alpha * m_radius * m_radius));
    }

    std::string to_string() const override {
        return tfm::format("GaussianFilter[stddev=%.2f, radius=%.2f]", m_stddev, m_radius);
    }

    MTS_DECLARE_CLASS()
protected:
    Float m_stddev;
};

MTS_IMPLEMENT_CLASS(GaussianFilter, ReconstructionFilter);
MTS_EXPORT_PLUGIN(GaussianFilter, "Gaussian reconstruction filter");
NAMESPACE_END(mitsuba)
