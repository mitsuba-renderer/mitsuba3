#include <mitsuba/core/string.h>
#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(string)

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
