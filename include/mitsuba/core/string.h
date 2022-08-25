#pragma once

#include <mitsuba/mitsuba.h>
#include <cstring>
#include <string>
#include <sstream>
#include <ostream>
#include <algorithm>

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

/**
 * \brief Locale-independent string to floating point conversion analogous
 * to std::stof. (implemented using Daniel Lemire's fast_float library.)
 *
 * Parses a floating point number in a (potentially longer) string start..end-1.
 * The 'endptr' argument (if non-NULL) is used to return a pointer to the
 * character following the parsed floating point value.
 *
 * Throws an exception if the conversion is unsuccessful.
 */
template <typename T>
T parse_float(const char *start, const char *end, char **endptr);
extern template MI_EXPORT_LIB float parse_float<float>(
    const char *start, const char *end, char **endptr);
extern template MI_EXPORT_LIB double parse_float<double>(
    const char *start, const char *end, char **endptr);


/**
 * \brief Locale-independent string to floating point conversion analogous
 * to std::stof. (implemented using Daniel Lemire's fast_float library.)
 *
 * Throws an exception if the conversion is unsuccessful, or if the portion of
 * the string following the parsed number contains non-whitespace characters.
 */
template <typename T> T stof(const std::string &s);

extern template MI_EXPORT_LIB float  stof<float>(const std::string &str);
extern template MI_EXPORT_LIB double stof<double>(const std::string &str);

/**
 * \brief Locale-independent string to floating point conversion analogous
 * to std::strtof. (implemented using Daniel Lemire's fast_float library.)
 *
 * Throws an exception if the conversion is unsuccessful.
 */
template <typename T> T strtof(const char *s, char **endptr) {
    return parse_float<T>(s, s + strlen(s), endptr);
}

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
    for (size_t i = 0; i < s.length(); ++i)
        result[i] = (char) std::tolower(s[i]);
    return result;
}

/// Return a upper-case version of the given string (warning: not unicode compliant)
inline std::string to_upper(const std::string &s) {
    std::string result;
    result.resize(s.length());
    for (size_t i = 0; i < s.length(); ++i)
        result[i] = (char) std::toupper(s[i]);
    return result;
}

/// Chop up the string given a set of delimiters (warning: not unicode compliant)
extern MI_EXPORT_LIB std::vector<std::string> tokenize(const std::string &string,
                                                         const std::string &delim = ", ",
                                                         bool include_empty = false);

/// Indent every line of a string by some number of spaces
extern MI_EXPORT_LIB std::string indent(const std::string &string, size_t amount = 2);

/// Turn a type into a string representation and indent every line by some number of spaces
template <typename T>
inline std::string indent(const T &value, size_t amount = 2) {
    std::ostringstream oss;
    oss << value;
    std::string string = oss.str();
    return string::indent(string, amount);
}

extern MI_EXPORT_LIB std::string indent(const Object *value, size_t amount = 2);

template <typename T, typename T2 = Object, std::enable_if_t<std::is_base_of_v<T2, T>, int> = 0>
std::string indent(const T *value, size_t amount = 2) {
    return indent((const T2 *) value, amount);
}

inline bool replace_inplace(std::string &str, const std::string &source,
                            const std::string &target) {
    size_t pos = 0;
    bool found = false;
    while ((pos = str.find(source, pos)) != std::string::npos) {
        found = true;
        str.replace(pos, source.length(), target);
        pos += target.length();
    }
    return found;
}

/// Remove leading and trailing characters
extern MI_EXPORT_LIB std::string trim(const std::string &s,
                                        const std::string &whitespace = " \t");

/// Check if a list of keys contains a specific key
extern MI_EXPORT_LIB bool contains(const std::vector<std::string> &keys, const std::string &key);

NAMESPACE_END(string)
NAMESPACE_END(mitsuba)
