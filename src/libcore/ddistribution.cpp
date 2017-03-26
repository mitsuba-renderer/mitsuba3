#include <mitsuba/core/ddistribution.h>

NAMESPACE_BEGIN(mitsuba)

template Float DiscreteDistribution::operator[](size_t) const;
template FloatP DiscreteDistribution::operator[](SizeP) const;

template size_t DiscreteDistribution::sample(Float) const;
template SizeP DiscreteDistribution::sample(FloatP) const;

template std::pair<size_t, Float> DiscreteDistribution::sample_pdf(Float) const;
template std::pair<SizeP, FloatP> DiscreteDistribution::sample_pdf(FloatP) const;

template size_t DiscreteDistribution::sample_reuse(Float *) const;
template SizeP DiscreteDistribution::sample_reuse(FloatP *) const;

template std::pair<size_t, Float> DiscreteDistribution::sample_reuse_pdf(Float *) const;
template std::pair<SizeP, FloatP> DiscreteDistribution::sample_reuse_pdf(FloatP *) const;

NAMESPACE_END(mitsuba)
