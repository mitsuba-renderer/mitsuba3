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

template <typename T> T parse_float(const char *s, const char *end, char **endptr) noexcept {
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

    T result = 0;
    fast_float::from_chars_result status =
        fast_float::from_chars(p, end, result);

    *endptr = (char *) (status.ec == std::errc() ? status.ptr : s);

    return result;
}

template <typename T> T stof(std::string_view s) {
    const char *start = s.data(),
               *end   = start + s.length();

    char *p = nullptr;
    T result = parse_float<T>(start, end, &p);
    bool success = false;

    // Skip trailing space
    while (p < end) {
        char c = *p;
        if (c == ' ' || c == '\t') {
            ++p;
        } else {
            success = false;
            break;
        }
    }

    if (p == end)
        success = true;

    if (unlikely(!success))
        Throw("Floating point number \"%s\" could not be parsed!", s);

    return result;
}

template MI_EXPORT_LIB float  stof<float>(std::string_view);
template MI_EXPORT_LIB double stof<double>(std::string_view);
template MI_EXPORT_LIB float  parse_float<float>(const char *, const char *, char **) noexcept;
template MI_EXPORT_LIB double parse_float<double>(const char *, const char *, char **) noexcept;

std::vector<std::string> tokenize(std::string_view string,
                                  std::string_view delim,
                                  bool include_empty) {
    std::string_view::size_type last_pos = 0, pos = string.find_first_of(delim, last_pos);
    std::vector<std::string> tokens;

    while (last_pos != std::string::npos) {
        std::string substr(string.substr(last_pos, pos - last_pos));
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

std::string indent(std::string_view string, size_t amount) {
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

std::string trim(std::string_view s, std::string_view whitespace) {
    auto it1 = s.find_first_not_of(whitespace);
    if (it1 == std::string::npos)
        return "";
    auto it2 = s.find_last_not_of(whitespace);
    return std::string(s.substr(it1, it2 - it1 + 1));
}

bool contains(const std::vector<std::string> &keys, std::string_view key) {
    for (auto& k: keys)
        if (k == key)
            return true;
    return false;
}

NAMESPACE_END(string)
NAMESPACE_END(mitsuba)
