#pragma once

#include <mitsuba/mitsuba.h>
#include <string>
#include <vector>
#include <algorithm>
#include <ostream>

/// Turns a vector of elements into a human-readable representation
template <typename T, typename Alloc>
std::ostream &operator<<(std::ostream &os, const std::vector<T, Alloc> &v) {
    auto it = v.begin();
    while (it != v.end()) {
        os << *it;
        it++;
        if (it != v.end())
            os << ", ";
    }
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
inline std::string to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

/// Return a upper-case version of the given string (warning: not unicode compliant)
inline std::string to_upper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

/// Chop up the string given a set of delimiters (warning: not unicode compliant)
inline std::vector<std::string> tokenize(const std::string &string,
                                         const std::string &delim = ", ",
                                         bool include_empty = false) {
    std::string::size_type last_pos = 0, pos = string.find_first_of(delim, last_pos);
    std::vector<std::string> tokens;

    while (last_pos != std::string::npos) {
        std::string substr = string.substr(last_pos, pos - last_pos);
        if (!substr.empty() || include_empty)
            tokens.push_back(std::move(substr));
        last_pos = pos;
        if (last_pos != std::string::npos) {
            last_pos += 1;
            pos = string.find_first_of(delim, last_pos);
        }
    }

    return tokens;
}

/// Indent every line of a string by some number of spaces
inline std::string indent(const std::string &string, int amount = 2) {
    std::string result;
    result.reserve(string.size());
    for (size_t i = 0; i<string.length(); ++i) {
        char ch = string[i];
        result += ch;
        if (ch == '\n') {
            for (int j = 0; j < amount; ++j)
                result += ' ';
        }
    }
    return result;
}

inline std::string trim(const std::string &s,
                        const std::string &whitespace = " \t") {
    auto it1 = s.find_first_not_of(whitespace);
    if (it1 == std::string::npos)
        return "";
    auto it2 = s.find_last_not_of(whitespace);
    return s.substr(it1, it2 - it1 + 1);
}

NAMESPACE_END(string)
NAMESPACE_END(mitsuba)
