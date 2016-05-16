#pragma once

#include <mitsuba/mitsuba.h>
#include <string>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(string)

inline bool starts_with(const std::string &string, const std::string &pattern) {
    size_t i = 0;
    while (pattern[i] == string[i] &&
           i < pattern.length() && i < string.length())
        ++i;
    return i == pattern.length();
}

NAMESPACE_END(string)
NAMESPACE_END(mitsuba)
