#pragma once

#include <mitsuba/mitsuba.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <ostream>

/// Turns a vector of elements into a human-readable representation
template <typename T, typename Alloc>
std::ostream &operator<<(std::ostream &os, const std::vector<T, Alloc> &v) {
    auto it = v.begin();
    os << "[";
    while (it != v.end()) {
        os << *it;
        it++;
        if (it != v.end())
            os << ", ";
    }
    os << "]";
    return os;
}

NAMESPACE_BEGIN(mitsuba)

using ::operator<<;

NAMESPACE_BEGIN(string)

/// Check if the given string starts with a specified prefix
inline bool starts_with(const std::string &string, const std::string &prefix) {
    if (prefix.size() > string.size())
        return false;
    return std::equal(prefix.begin(), prefix.end(), string.begin());
}

/// Check if the given string ends with a specified suffix
inline bool ends_with(const std::string &string, const std::string &suffix) {
    if (suffix.size() > string.size())
        return false;
    return std::equal(suffix.rbegin(), suffix.rend(), string.rbegin());
}

/// Return a lower-case version of the given string (warning: not unicode compliant)
inline std::string to_lower(const std::string &s) {
    std::string result;
    result.resize(s.length());
    std::transform(s.begin(), s.end(), result.begin(), ::tolower);
    return result;
}

/// Return a upper-case version of the given string (warning: not unicode compliant)
inline std::string to_upper(const std::string &s) {
    std::string result;
    result.resize(s.length());
    std::transform(s.begin(), s.end(), result.begin(), ::toupper);
    return result;
}

/// Chop up the string given a set of delimiters (warning: not unicode compliant)
extern MTS_EXPORT_CORE std::vector<std::string> tokenize(const std::string &string,
                                                         const std::string &delim = ", ",
                                                         bool include_empty = false);

/// Indent every line of a string by some number of spaces
extern MTS_EXPORT_CORE std::string indent(const std::string &string, size_t amount = 2);

/// Turn a type into a string representation and indent every line by some number of spaces
template <typename T>
inline std::string indent(const T &value, size_t amount = 2) {
    std::ostringstream oss;
    oss << value;
    std::string string = oss.str();
    return string::indent(string, amount);
}

extern MTS_EXPORT_CORE std::string indent(const Object *value, size_t amount = 2);

template <typename T, typename T2 = Object, std::enable_if_t<std::is_base_of_v<T2, T>, int> = 0>
std::string indent(const T *value, size_t amount = 2) {
    return indent((const T2 *) value, amount);
}

inline void replace_inplace(std::string &str, const std::string &source,
                            const std::string &target) {
    size_t pos = 0;
    while ((pos = str.find(source, pos)) != std::string::npos) {
        str.replace(pos, source.length(), target);
        pos += target.length();
    }
}

/// Remove leading and trailing characters
extern MTS_EXPORT_CORE std::string trim(const std::string &s,
                                        const std::string &whitespace = " \t");

/// Check if a list of keys contains a specific key
extern MTS_EXPORT_CORE bool contains(const std::vector<std::string> &keys, const std::string &key);

NAMESPACE_END(string)
NAMESPACE_END(mitsuba)
