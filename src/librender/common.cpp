#include <mitsuba/render/common.h>
#include <mitsuba/core/simd.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/string.h>

NAMESPACE_BEGIN(mitsuba)

std::ostream &operator<<(std::ostream &os, const ETransportMode &mode) {
    switch (mode) {
        case ERadiance:   os << "radiance"; break;
        case EImportance: os << "importance"; break;
        default:          os << "invalid"; break;
    }
    return os;
}

std::ostream &operator<<(std::ostream &os, const EMeasure &measure) {
    switch (measure) {
        case ESolidAngle: os << "solidangle"; break;
        case ELength:     os << "length"; break;
        case EArea:       os << "area"; break;
        case EDiscrete:   os << "discrete"; break;
        default:          os << "invalid"; break;
    }
    return os;
}

NAMESPACE_END(mitsuba)
