#include <mitsuba/core/rfilter.h>

NAMESPACE_BEGIN(mitsuba)

ReconstructionFilter::ReconstructionFilter(const Properties &) { }

ReconstructionFilter::~ReconstructionFilter() { }

void ReconstructionFilter::init_discretization() {
    Assert(m_radius > 0);

    double sum = 0.0;
    /* Evaluate and normalize the filter */
    for (size_t i = 0; i < MTS_FILTER_RESOLUTION; ++i) {
        Float value = eval((m_radius * i) / MTS_FILTER_RESOLUTION);
        m_values[i] = value;
        sum += double(value);
    }

    m_values[MTS_FILTER_RESOLUTION] = 0;
    m_scale_factor = MTS_FILTER_RESOLUTION / m_radius;
    m_border_size = (uint32_t) std::ceil(m_radius - Float(.5));
}

std::ostream &operator<<(std::ostream &os, const ReconstructionFilter::EBoundaryCondition &value) {
    switch (value) {
        case ReconstructionFilter::EClamp: os << "clamp"; break;
        case ReconstructionFilter::ERepeat: os << "repeat"; break;
        case ReconstructionFilter::EMirror: os << "mirror"; break;
        case ReconstructionFilter::EZero: os << "zero"; break;
        case ReconstructionFilter::EOne: os << "one"; break;
        default: os << "invalid"; break;
    }
    return os;
}

MTS_IMPLEMENT_CLASS_ALIAS(ReconstructionFilter, "rfilter", Object)
NAMESPACE_END(mitsuba)
