#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _rfilter-gaussian:

Gaussian filter (:monosp:`gaussian`)
------------------------------------

This is a windowed Gaussian filter with configurable standard deviation.
It often produces pleasing results, and never suffers from ringing, but may
occasionally introduce too much blurring.

When no reconstruction filter is explicitly requested, this is the default
choice in Mitsuba.

Takes a standard deviation parameter (:paramtype:`stddev`) which is set to 0.5
pixels be default.

 */

template <typename Float, typename Spectrum>
class GaussianFilter final : public ReconstructionFilter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(ReconstructionFilter, init_discretization, m_radius)
    MTS_IMPORT_TYPES()

    GaussianFilter(const Properties &props) : Base(props) {
        // Standard deviation
        m_stddev = props.float_("stddev", 0.5f);

        // Cut off after 4 standard deviations
        m_radius = 4 * m_stddev;

        m_alpha = -1.f / (2.f * m_stddev * m_stddev);
        m_bias = std::exp(m_alpha * sqr(m_radius));

        init_discretization();
    }

    Float eval(Float x, mask_t<Float> /* active */) const override {
        return max(0.f, exp(m_alpha * sqr(x)) - m_bias);
    }

    std::string to_string() const override {
        return tfm::format("GaussianFilter[stddev=%.2f, radius=%.2f]", m_stddev, m_radius);
    }

    MTS_DECLARE_CLASS()
protected:
    ScalarFloat m_stddev, m_alpha, m_bias;
};

MTS_IMPLEMENT_CLASS_VARIANT(GaussianFilter, ReconstructionFilter)
MTS_EXPORT_PLUGIN(GaussianFilter, "Gaussian reconstruction filter");
NAMESPACE_END(mitsuba)
