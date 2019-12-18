#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/config.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT ReconstructionFilter<Float, Spectrum>::ReconstructionFilter(const Properties &/*props*/) { }
MTS_VARIANT ReconstructionFilter<Float, Spectrum>::~ReconstructionFilter() { }

MTS_VARIANT void ReconstructionFilter<Float, Spectrum>::init_discretization() {
    Assert(m_radius > 0);
    m_values.resize(MTS_FILTER_RESOLUTION + 1);

    // Evaluate and store the filter values
    for (size_t i = 0; i < MTS_FILTER_RESOLUTION; ++i)
        m_values[i] = scalar_cast(hmax(eval((m_radius * i) / MTS_FILTER_RESOLUTION)));

    m_values[MTS_FILTER_RESOLUTION] = 0;
    m_scale_factor = MTS_FILTER_RESOLUTION / m_radius;
    m_border_size = (int) std::ceil(m_radius - .5f - 2.f * math::RayEpsilon<ScalarFloat>);
}

std::ostream &operator<<(std::ostream &os, const FilterBoundaryCondition &value) {
    switch (value) {
        case FilterBoundaryCondition::Clamp: os << "clamp"; break;
        case FilterBoundaryCondition::Repeat: os << "repeat"; break;
        case FilterBoundaryCondition::Mirror: os << "mirror"; break;
        case FilterBoundaryCondition::Zero: os << "zero"; break;
        case FilterBoundaryCondition::One: os << "one"; break;
        default: os << "invalid"; break;
    }
    return os;
}

MTS_IMPLEMENT_CLASS_VARIANT(ReconstructionFilter, Object, "rfilter")
MTS_INSTANTIATE_CLASS(ReconstructionFilter)
NAMESPACE_END(mitsuba)
