#pragma once

#include <mitsuba/mitsuba.h>
#include <tinyformat.h>
#include <sstream>
#include <string>
#include <vector>

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

/// Generate a trap instruction if running in a debugger; otherwise, return.
extern MTS_EXPORT_CORE void trapDebugger();

/// Joins elements of <tt>v</tt> into a string, separated by an optional delimiter.
template <typename T>
MTS_EXPORT_CORE std::string mkString(const std::vector<T> v,
                                     const std::string &delimiter = ", ") {
    std::ostringstream oss;
    auto it = v.begin();
    while (it != v.end()) {
        oss << (*it);
        it++;
        if (it != v.end())
            oss << delimiter;
    }

    return oss.str();
}

NAMESPACE_END(util)
NAMESPACE_END(mitsuba)
