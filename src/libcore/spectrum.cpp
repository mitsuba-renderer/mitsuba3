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

MTS_IMPLEMENT_CLASS_ALIAS(ContinuousSpectrum, "spectrum", Object)
NAMESPACE_END(mitsuba)
