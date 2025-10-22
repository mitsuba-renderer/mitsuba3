#include <mitsuba/core/formatter.h>
#include <mitsuba/core/logger.h>
#include <ctime>
#include <cstring>

NAMESPACE_BEGIN(mitsuba)

DefaultFormatter::DefaultFormatter()
    : m_has_date(true), m_has_log_level(true), m_has_thread(true),
      m_has_class(true) { }

std::string DefaultFormatter::format(mitsuba::LogLevel level,
                                     const char *class_name, const char *file,
                                     int line, std::string_view msg) {
    std::string result;
    result.reserve(256); // Pre-allocate to avoid reallocations

    // Date/Time
    if (m_has_date) {
        time_t time_ = std::time(nullptr);
        char time_buffer[128];
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S ", std::localtime(&time_));
        result.append(time_buffer);
    }

    // Log level
    if (m_has_log_level) {
        const char *level_str;
        switch (level) {
            case Trace: level_str = "TRACE "; break;
            case Debug: level_str = "DEBUG "; break;
            case Info:  level_str = "INFO  "; break;
            case Warn:  level_str = "WARN  "; break;
            case Error: level_str = "ERROR "; break;
            default:    level_str = "CUSTM "; break;
        }
        result.append(level_str);
    }

    // Class/File info
    if (m_has_class) {
        if (class_name) {
            result.push_back('[');
            result.append(class_name);
            result.append("] ");
        } else if (line != -1 && file) {
            // Extract filename without heap allocation
            const char *filename = file;
            const char *last_sep = strrchr(file, '/');
            #ifdef _WIN32
                const char *last_sep_win = strrchr(file, '\\');
                if (last_sep_win && (!last_sep || last_sep_win > last_sep))
                    last_sep = last_sep_win;
            #endif
            if (last_sep)
                filename = last_sep + 1;

            result.push_back('[');
            result.append(filename);
            result.push_back(':');
            result.append(std::to_string(line));
            result.append("] ");
        }
    }

    // Message
    result.append(msg);

    return result;
}

NAMESPACE_END(mitsuba)
