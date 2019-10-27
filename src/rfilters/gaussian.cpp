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
template <typename Float, typename Spectrum = void>
class GaussianFilter final : public ReconstructionFilter<Float> {
public:
    MTS_REGISTER_CLASS(GaussianFilter, ReconstructionFilter)
    MTS_USING_BASE_FLOAT(ReconstructionFilter, init_discretization, m_radius)

    GaussianFilter(const Properties &props) : Base(props) {
        // Standard deviation
        m_stddev = props.float_("stddev", 0.5f);

        // Cut off after 4 standard deviations
        m_radius = 4 * m_stddev;

        m_alpha = -1.f / (2.f * m_stddev * m_stddev);
        m_bias = std::exp(m_alpha * sqr(m_radius));

        init_discretization();
    }

    Float eval(Float x) const override {
        return max(0.f, exp(m_alpha * sqr(x)) - m_bias);
    }

    std::string to_string() const override {
        return tfm::format("GaussianFilter[stddev=%.2f, radius=%.2f]", m_stddev, m_radius);
    }

protected:
    Float m_stddev, m_alpha, m_bias;
};

MTS_IMPLEMENT_PLUGIN(GaussianFilter, ReconstructionFilter, "Gaussian reconstruction filter");
NAMESPACE_END(mitsuba)
