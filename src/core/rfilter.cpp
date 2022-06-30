#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/config.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT ReconstructionFilter<Float, Spectrum>::ReconstructionFilter(const Properties &/*props*/) { }
MI_VARIANT ReconstructionFilter<Float, Spectrum>::~ReconstructionFilter() { }

MI_VARIANT void ReconstructionFilter<Float, Spectrum>::init_discretization() {
    Assert(m_radius > 0);

    if constexpr (!dr::is_jit_v<Float>) {
        m_values.resize(MI_FILTER_RESOLUTION + 1);

        // Evaluate and store the filter values
        for (size_t i = 0; i < MI_FILTER_RESOLUTION; ++i)
            m_values[i] = eval((m_radius * i) / MI_FILTER_RESOLUTION);

        m_values[MI_FILTER_RESOLUTION] = 0;
    }

    m_scale_factor = MI_FILTER_RESOLUTION / m_radius;
    m_border_size = (int) dr::ceil(m_radius - .5f - 2.f * math::RayEpsilon<ScalarFloat>);
}

MI_VARIANT bool ReconstructionFilter<Float, Spectrum>::is_box_filter() const {
    // The box filter is the only filter in Mitsuba 3 with 1/2px radius
    return m_radius == .5f;
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

MI_IMPLEMENT_CLASS_VARIANT(ReconstructionFilter, Object, "rfilter")
MI_INSTANTIATE_CLASS(ReconstructionFilter)
NAMESPACE_END(mitsuba)
