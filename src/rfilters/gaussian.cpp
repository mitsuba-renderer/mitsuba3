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
        ScalarFloat stddev = props.get<ScalarFloat>("stddev", .5f);

        // Cut off after 4 standard deviations
        m_radius = 4 * stddev;

        /*
          Remez fit to exp(-x/2), obtained using:

          f[x_] = MiniMaxApproximation[Exp[-x/2], {x, {0, 16}, 9, 0},
              WorkingPrecision -> 50, Brake -> {400, 400},
              MaxIterations -> 100, Bias -> 0][[2, 1]];

          N[CoefficientList[h[x] - h[16], x], 10]
        */

        double coeff[10] = { 9.992604880e-1, -4.977025247e-1,
                             1.222248550e-1, -1.932406282e-2,
                             2.136713061e-3, -1.679873860e-4,
                             9.202145248e-6, -3.329417433e-7,
                             7.128382794e-9, -6.821193280e-11 };

        ScalarFloat coeff_s[10];

        // Scale Remez approximation
        double scale = 1;
        for (int i = 0; i < 10; ++i) {
            coeff_s[i] = ScalarFloat(coeff[i] * scale);
            scale /= ek::sqr((double) stddev);
        }

        // Ensure that we really reach zero at the boundary
        coeff_s[0] -= ek::detail::estrin_impl(ek::sqr(m_radius), coeff_s);

        for (int i = 0; i < 10; ++i)
            m_coeff[i] = coeff_s[i];

        init_discretization();
    }

    Float eval(Float x, Mask /* active */) const override {
        return ek::max(ek::detail::estrin_impl(ek::sqr(x), m_coeff), 0.f);
      }

    std::string to_string() const override {
        return tfm::format("GaussianFilter[stddev=%.2f, radius=%.2f]",
                           m_radius * .25f, m_radius);
    }

    MTS_DECLARE_CLASS()

protected:
    Float m_coeff[10];
};

MTS_IMPLEMENT_CLASS_VARIANT(GaussianFilter, ReconstructionFilter)
MTS_EXPORT_PLUGIN(GaussianFilter, "Gaussian reconstruction filter");
NAMESPACE_END(mitsuba)
