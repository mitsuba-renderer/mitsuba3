#pragma once

#include <mitsuba/mitsuba.h>
#include <string>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(util)

#if defined(__WINDOWS__)
    /// Return a std::string version of GetLastError()
    extern std::string MTS_EXPORT_CORE getLastError();
#endif

/// Determine the number of available CPU cores (including virtual cores)
extern MTS_EXPORT_CORE int getCoreCount();

/**
 * \brief Convert a time difference (in seconds) to a string representation
 * \param time Time difference in (fractional) sections
 * \param precise When set to true, a higher-precision string representation
 * is generated.
 */
extern MTS_EXPORT_CORE std::string timeString(Float time, bool precise = false);

/// Turn a memory size into a human-readable string
extern MTS_EXPORT_CORE std::string memString(size_t size, bool precise = false);

NAMESPACE_END(util)
NAMESPACE_END(mitsuba)
