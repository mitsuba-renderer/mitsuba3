#pragma once

#include <mitsuba/mitsuba.h>

NAMESPACE_BEGIN(mitsuba)

#if defined(__WINDOWS__)
    /// Return a std::string version of GetLastError()
    extern std::string MTS_EXPORT_CORE lastErrorText();
#endif

/// Determine the number of available CPU cores (including virtual cores)
extern MTS_EXPORT_CORE int getCoreCount();

NAMESPACE_END(mitsuba)
