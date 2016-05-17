#pragma once

#include <mitsuba/mitsuba.h>
#include <string>
#include <vector>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(string)

/// Check if the given string starts with a specified prefix
inline bool starts_with(const std::string &string, const std::string &prefix) {
    size_t i = 0;
    while (prefix[i] == string[i] &&
           i < prefix.length() && i < string.length())
        ++i;
    return i == prefix.length();
}

/// Return a lower-case version of the given string (warning: not unicode compliant)
inline std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

/// Return a upper-case version of the given string (warning: not unicode compliant)
inline std::string toUpper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

/// Chop up the string given a set of delimiters (warning: not unicode compliant)
inline std::vector<std::string> tokenize(const std::string &string,
                                         const std::string &delim = ", ",
                                         bool includeEmpty = false) {
    std::string::size_type lastPos = 0, pos = string.find_first_of(delim, lastPos);
    std::vector<std::string> tokens;

    while (lastPos != std::string::npos) {
        if (pos != lastPos || includeEmpty)
            tokens.push_back(string.substr(lastPos, pos - lastPos));
        lastPos = pos;
        if (lastPos != std::string::npos) {
            lastPos += 1;
            pos = string.find_first_of(delim, lastPos);
        }
    }

    return tokens;
}

NAMESPACE_END(string)
NAMESPACE_END(mitsuba)
