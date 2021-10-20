#include <mitsuba/core/string.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/logger.h>

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfortify-source"
#  pragma clang diagnostic ignored "-Wshift-count-overflow"
#endif
#include <fast_float/fast_float.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(string)

template <typename T> T strtof(const char *s, char **endptr) {
    const char *p = s;

    // Skip leading space
    do {
        char c = *p;
        if (c == ' ' || c == '\t')
            ++p;
        else
            break;
    } while (true);

    // Skip leading '+' signs (not handled by from_chars)
    if (*p == '+')
        ++p;

    T result;
    fast_float::from_chars_result status =
        fast_float::from_chars(p, p + strlen(p), result);

    bool success = status.ec == std::errc();

    if (likely(success)) {
        *endptr = (char *) status.ptr;
        p = status.ptr;
    } else {
        Throw("Floating point number \"%s\" could not be parsed!", s);
    }

    return result;
}

template <typename T> T stof(const std::string &s) {
    const char *p = s.c_str();

    // Skip leading space
    do {
        char c = *p;
        if (c == ' ' || c == '\t')
            ++p;
        else
            break;
    } while (true);

    // Skip leading '+' signs (not handled by from_chars)
    if (*p == '+')
        ++p;

    T result;
    fast_float::from_chars_result status =
        fast_float::from_chars(p, p + s.length(), result);

    bool success = status.ec == std::errc();

    if (likely(success)) {
        p = status.ptr;

        // Skip trailing space
        do {
            char c = *p;

            if (c == ' ' || c == '\t') {
                ++p;
            } else  {
                success = c == '\0';
                break;
            }
        } while (true);
    }

    if (unlikely(!success))
        Throw("Floating point number \"%s\" could not be parsed!", s);

    return result;
}

template MTS_EXPORT_CORE float  stof<float>(const std::string &);
template MTS_EXPORT_CORE double stof<double>(const std::string &);
template MTS_EXPORT_CORE float  strtof<float>(const char *, char **);
template MTS_EXPORT_CORE double strtof<double>(const char *, char **);

std::vector<std::string> tokenize(const std::string &string,
                                  const std::string &delim,
                                  bool include_empty) {
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

std::string indent(const std::string &string, size_t amount) {
    std::string result;
    result.reserve(string.size());
    for (size_t i = 0; i < string.length(); ++i) {
        char ch = string[i];
        result += ch;
        if (ch == '\n') {
            for (size_t j = 0; j < amount; ++j)
                result += ' ';
        }
    }
    return result;
}

std::string indent(const Object *value, size_t amount) {
    std::string string = value == nullptr ? std::string("nullptr")
                                          : value->to_string();
    return indent(string, amount);
}

std::string trim(const std::string &s, const std::string &whitespace) {
    auto it1 = s.find_first_not_of(whitespace);
    if (it1 == std::string::npos)
        return "";
    auto it2 = s.find_last_not_of(whitespace);
    return s.substr(it1, it2 - it1 + 1);
}

bool contains(const std::vector<std::string> &keys, const std::string &key) {
    for (auto& k: keys)
        if (k == key)
            return true;
    return false;
}

NAMESPACE_END(string)
NAMESPACE_END(mitsuba)
