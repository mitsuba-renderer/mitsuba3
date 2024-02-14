#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _rfilter-gaussian:

Gaussian filter (:monosp:`gaussian`)
------------------------------------

.. pluginparameters::

 * - stddev
   - |float|
   - Specifies the standard deviation (Default: 0.5)

This is a windowed Gaussian filter with configurable standard deviation.
It often produces pleasing results, and never suffers from ringing, but may
occasionally introduce too much blurring.

When no reconstruction filter is explicitly requested, this is the default
choice in Mitsuba.

.. tabs::
    .. code-tab::  xml
        :name: gaussian-rfilter

        <rfilter type="gaussian">
            <float name="stddev" value="0.25"/>
        </rfilter>

    .. code-tab:: python

        'type': 'gaussian',
        'stddev': 0.25

 */

template <typename Float, typename Spectrum>
class GaussianFilter final : public ReconstructionFilter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(ReconstructionFilter, init_discretization, m_radius)
    MI_IMPORT_TYPES()

    GaussianFilter(const Properties &props) : Base(props) {
        // Standard deviation
        m_stddev = props.get<ScalarFloat>("stddev", .5f);

        // Cut off after 4 standard deviations
        m_radius = 4 * m_stddev;

        /* Precompute a cheap approximation to the filter kernel.
           Unnecessary on NVIDIA GPUs that provide a fast exponential
           instruction via the MUFU (multi-function generator). */
        if constexpr (!dr::is_cuda_v<Float>) {
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
                scale /= dr::square((double) m_stddev);
            }

            // Ensure that we really reach zero at the boundary
            coeff_s[0] -= dr::detail::estrin_impl(dr::square(m_radius), coeff_s);

            for (int i = 0; i < 10; ++i)
                m_coeff[i] = coeff_s[i];
        }

        init_discretization();
    }

    Float eval(Float x, Mask /* active */) const override {
        if constexpr (!dr::is_cuda_v<Float>) {
            return dr::maximum(dr::detail::estrin_impl(dr::square(x), m_coeff), 0.f);
        } else {
            // Use the base-2 exponential functions on NVIDIA hardware
            ScalarFloat alpha = -1.f / (2.f * dr::square(m_stddev));
            ScalarFloat bias = dr::exp(alpha * dr::square(m_radius));
            return dr::maximum(0.f, dr::exp2((dr::InvLogTwo<Float> * alpha) * dr::square(x)) - bias);
        }
    }

    std::string to_string() const override {
        return tfm::format("GaussianFilter[stddev=%.2f, radius=%.2f]",
                           m_stddev, m_radius);
    }

    MI_DECLARE_CLASS()

protected:
    ScalarFloat m_stddev;
    Float m_coeff[10];
};

MI_IMPLEMENT_CLASS_VARIANT(GaussianFilter, ReconstructionFilter)
MI_EXPORT_PLUGIN(GaussianFilter, "Gaussian reconstruction filter");
NAMESPACE_END(mitsuba)
