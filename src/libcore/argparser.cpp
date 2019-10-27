#include <mitsuba/core/argparser.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/string.h>

NAMESPACE_BEGIN(mitsuba)

void ArgParser::Arg::append(const std::string &value) {
    if (m_present) {
        if (!m_next)
            m_next = new Arg(*this);
        m_next->append(value);
    } else {
        m_present = true;
        m_value = value;
    }
}

size_t ArgParser::Arg::count() const {
    const Arg *value = this;
    size_t nargs = 0;
    while (value) {
        nargs += value->m_present ? 1 : 0;
        value = value->next();
    }
    return nargs;
}

int ArgParser::Arg::as_int() const {
    char *start = (char *) m_value.data();
    char *end   = (char *) m_value.data() + m_value.size();
    long result = std::strtol(start, &start, 10);
    if (start != end)
        Throw("Argument \"%s\": value \"%s\" is not an integer!", m_prefixes[0], m_value);
    return (int) result;
}

double ArgParser::Arg::as_float() const {
    char *start = (char *) m_value.data();
    char *end   = (char *) m_value.data() + m_value.size();
    double result = std::strtod(start, &start);
    if (start != end)
        Throw("Argument \"%s\": value \"%s\" is not a valid floating point value!", m_prefixes[0], m_value);
    return result;
}

void ArgParser::parse(int argc, const char **argv) {
    std::vector<std::string> cmdline(argc);
    for (int i = 0; i < argc; ++i)
        cmdline[i] = argv[i];

    if (!cmdline.empty())
        m_executable_name = cmdline[0];

    for (size_t i = 1; i < cmdline.size(); ++i) {
        bool found = false;

        for (Arg *arg: m_args) {
            for (const std::string &prefix : arg->m_prefixes) {
                const bool long_form = string::starts_with(prefix, "--");
                const bool short_form = string::starts_with(prefix, "-") && !long_form;
                const bool other = prefix.empty() && arg->m_extra;
                const bool prefix_found = string::starts_with(cmdline[i], prefix);

                if (short_form && prefix_found) {
                    std::string suffix = cmdline[i].substr(prefix.length());
                    if (!suffix.empty()) {
                        if (!arg->m_extra)
                            suffix = "-" + suffix;
                        cmdline.insert(cmdline.begin() + i + 1, suffix);
                    }
                    found = true;
                } else if (long_form && prefix_found) {
                    found = true;
                } else if (other && !string::starts_with(cmdline[i], "-")) {
                    cmdline.insert(cmdline.begin() + i + 1, cmdline[i]);
                    found = true;
                }

                if (found) {
                    if (arg->m_extra) {
                        if (i + 1 >= cmdline.size() || string::starts_with(cmdline[i+1], "-"))
                            Throw("Missing/invalid argument for argument \"%s\"", prefix);
                        arg->append(cmdline[++i]);
                    } else {
                        arg->append();
                    }
                    break;
                }
            }
            if (found)
                break;
        }

        if (!found)
            Throw("Argument \"%s\" was not recognized!", cmdline[i]);
    }
}

NAMESPACE_END(mitsuba)
