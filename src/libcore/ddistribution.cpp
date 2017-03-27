#include <mitsuba/core/ddistribution.h>

NAMESPACE_BEGIN(mitsuba)

template MTS_EXPORT_CORE Float  DiscreteDistribution::operator[](uint32_t) const;
template MTS_EXPORT_CORE FloatP DiscreteDistribution::operator[](UInt32P ) const;

template MTS_EXPORT_CORE uint32_t DiscreteDistribution::sample(Float ) const;
template MTS_EXPORT_CORE UInt32P  DiscreteDistribution::sample(FloatP) const;

template MTS_EXPORT_CORE std::pair<uint32_t, Float>  DiscreteDistribution::sample_pdf(Float ) const;
template MTS_EXPORT_CORE std::pair<UInt32P,  FloatP> DiscreteDistribution::sample_pdf(FloatP) const;

template MTS_EXPORT_CORE uint32_t DiscreteDistribution::sample_reuse(Float  *) const;
template MTS_EXPORT_CORE UInt32P  DiscreteDistribution::sample_reuse(FloatP *) const;

template MTS_EXPORT_CORE std::pair<uint32_t, Float>  DiscreteDistribution::sample_reuse_pdf(Float  *) const;
template MTS_EXPORT_CORE std::pair<UInt32P,  FloatP> DiscreteDistribution::sample_reuse_pdf(FloatP *) const;

NAMESPACE_END(mitsuba)
