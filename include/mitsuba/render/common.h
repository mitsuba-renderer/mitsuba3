#pragma once

#include <ostream>
#include <mitsuba/core/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Specifies the transport mode when sampling or
 * evaluating a scattering function
 */
enum ETransportMode : uint32_t {
    /// Radiance transport
    ERadiance,

    /// Importance transport
    EImportance,

    /// Specifies the number of supported transport modes
    ETransportModes = 2
};

/**
 * \brief A list of measures that are associated with various sampling
 * methods in Mitsuba.
 *
 * Every now and then, sampling densities consist of a sum of several terms
 * defined on spaces that have different associated measures. In this case,
 * one of the constants in \ref EMeasure can be specified to clarify the
 * component in question when performing query operations.
 */
enum EMeasure : uint32_t {
    /// Invalid measure
    EInvalidMeasure,

    /// Solid angle measure
    ESolidAngle,

    /// Length measure
    ELength,

    /// Area measure
    EArea,

    /// Discrete measure
    EDiscrete,

    /// Specifies the number of supported measures
    EMeasureCount
};

extern MTS_EXPORT_RENDER std::ostream &operator<<(
    std::ostream &os, const ETransportMode &mode);

extern MTS_EXPORT_RENDER std::ostream &operator<<(
    std::ostream &os, const EMeasure &measure);

NAMESPACE_END(mitsuba)
