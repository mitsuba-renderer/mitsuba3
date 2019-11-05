#include <mitsuba/core/rfilter.h>

NAMESPACE_BEGIN(mitsuba)

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

NAMESPACE_END(mitsuba)
