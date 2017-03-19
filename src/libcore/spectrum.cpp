#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/logger.h>

NAMESPACE_BEGIN(mitsuba)

std::tuple<DiscreteSpectrum, DiscreteSpectrum, DiscreteSpectrum> ContinuousSpectrum::sample(Float) const {
    NotImplementedError("sample");
}

std::tuple<DiscreteSpectrumP, DiscreteSpectrumP, DiscreteSpectrumP> ContinuousSpectrum::sample(FloatP) const {
    NotImplementedError("sample");
}

DiscreteSpectrum ContinuousSpectrum::pdf(DiscreteSpectrum) const {
    NotImplementedError("pdf");
}

DiscreteSpectrumP ContinuousSpectrum::pdf(DiscreteSpectrumP) const {
    NotImplementedError("pdf");
}

/**
 * Based on "Simple Analytic Approximations to the CIE XYZ Color Matching
 * Functions" by Chris Wyman, Peter-Pike Sloan, and Peter Shirley
 */
static const Float cie1931_data[7][4] = {
    {  0.362f, 442.0f, 0.0624f, 0.0374f }, // x0
    {  1.056f, 599.8f, 0.0264f, 0.0323f }, // x1
    { -0.065f, 501.1f, 0.0490f, 0.0382f }, // x2
    {  0.821f, 568.8f, 0.0213f, 0.0247f }, // y0
    {  0.286f, 530.9f, 0.0613f, 0.0322f }, // y1
    {  1.217f, 437.0f, 0.0845f, 0.0278f }, // z0
    {  0.681f, 459.0f, 0.0385f, 0.0725f }  // z1
};

std::array<DiscreteSpectrum, 3> cie1931_xyz(DiscreteSpectrum lambda) {
    DiscreteSpectrum result[7];

    for (int i=0; i<7; ++i) {
        /* Coefficients of Gaussian fits */
        Float alpha(cie1931_data[i][0]), beta (cie1931_data[i][1]),
              gamma(cie1931_data[i][2]), delta(cie1931_data[i][3]);

        auto tmp = select(lambda < beta, DiscreteSpectrum(gamma),
                                         DiscreteSpectrum(delta)) * (lambda - beta);

        result[i] = alpha * exp(-.5f * tmp * tmp);
    }

    return {{ result[0] + result[1] + result[2],
              result[3] + result[4],
              result[5] + result[6] }};
}

DiscreteSpectrum cie1931_y(DiscreteSpectrum lambda) {
    DiscreteSpectrum result[2];
    for (int i=0; i<2; ++i) {
        /* Coefficients of Gaussian fits */
        int j = 3 + i;
        Float alpha(cie1931_data[j][0]), beta (cie1931_data[j][1]),
              gamma(cie1931_data[j][2]), delta(cie1931_data[j][3]);

        auto tmp = select(lambda < beta, DiscreteSpectrum(gamma),
                                         DiscreteSpectrum(delta)) * (lambda - beta);

        result[i] = alpha * exp(-.5f * tmp * tmp);
    }

    return result[0] + result[1];
}

MTS_IMPLEMENT_CLASS_ALIAS(ContinuousSpectrum, "spectrum", Object)
NAMESPACE_END(mitsuba)
