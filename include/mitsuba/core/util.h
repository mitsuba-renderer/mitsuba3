#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/logger.h>
#include <tinyformat.h>
#include <sstream>
#include <string>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(util)

#if defined(_WIN32)
    /// Return a std::string version of GetLastError()
    extern std::string MI_EXPORT_LIB last_error();
#endif

/// Determine the number of available CPU cores (including virtual cores)
extern MI_EXPORT_LIB int core_count();

/**
 * \brief Convert a time difference (in seconds) to a string representation
 * \param time Time difference in (fractional) sections
 * \param precise When set to true, a higher-precision string representation
 * is generated.
 */
extern MI_EXPORT_LIB std::string time_string(float time, bool precise = false);

/// Turn a memory size into a human-readable string
extern MI_EXPORT_LIB std::string mem_string(size_t size, bool precise = false);

/// Returns 'true' if the application is running inside a debugger
extern MI_EXPORT_LIB bool detect_debugger();

/// Generate a trap instruction if running in a debugger; otherwise, return.
extern MI_EXPORT_LIB void trap_debugger();

/// Return the absolute path to <tt>libmitsuba-core.dylib/so/dll<tt>
extern MI_EXPORT_LIB fs::path library_path();

/// Determine the width of the terminal window that is used to run Mitsuba
extern MI_EXPORT_LIB int terminal_width();

/// Return human-readable information about the Mitsuba build
extern MI_EXPORT_LIB std::string info_build(int thread_count);

/// Return human-readable information about the version
extern MI_EXPORT_LIB std::string info_copyright();

/// Return human-readable information about the enabled processor features
extern MI_EXPORT_LIB std::string info_features();

struct Version {
    unsigned int major_version, minor_version, patch_version;

    Version() = default;

    Version(int major, int minor, int patch)
        : major_version(major), minor_version(minor), patch_version(patch) { }

    Version(const char *value) {
        auto list = string::tokenize(value, " .");
        if (list.size() != 3)
            Throw("Version number must consist of three period-separated parts!");
        major_version = std::stoul(list[0]);
        minor_version = std::stoul(list[1]);
        patch_version = std::stoul(list[2]);
    }

    bool operator==(const Version &v) const {
        return std::tie(major_version, minor_version, patch_version) ==
               std::tie(v.major_version, v.minor_version, v.patch_version);
    }

    bool operator!=(const Version &v) const {
        return std::tie(major_version, minor_version, patch_version) !=
               std::tie(v.major_version, v.minor_version, v.patch_version);
    }

    bool operator<(const Version &v) const {
        return std::tie(major_version, minor_version, patch_version) <
               std::tie(v.major_version, v.minor_version, v.patch_version);
    }

    bool operator<=(const Version &v) const {
        return std::tie(major_version, minor_version, patch_version) <=
               std::tie(v.major_version, v.minor_version, v.patch_version);
    }

    bool operator>(const Version &v) const {
        return std::tie(major_version, minor_version, patch_version) >
               std::tie(v.major_version, v.minor_version, v.patch_version);
    }

    bool operator>=(const Version &v) const {
        return std::tie(major_version, minor_version, patch_version) >=
               std::tie(v.major_version, v.minor_version, v.patch_version);
    }

    friend std::ostream &operator<<(std::ostream &os, const Version &v) {
        os << v.major_version << "." << v.minor_version << "." << v.patch_version;
        return os;
    }
};

NAMESPACE_END(util)
NAMESPACE_END(mitsuba)
