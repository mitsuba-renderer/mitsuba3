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

    // Format timestamp once
    char time_buffer[128];
    if (m_has_date) {
        time_t time_ = std::time(nullptr);
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S ", std::localtime(&time_));
    }

    // Get log level string
    const char *level_str = nullptr;
    if (m_has_log_level) {
        switch (level) {
            case Trace: level_str = "TRACE "; break;
            case Debug: level_str = "DEBUG "; break;
            case Info:  level_str = "INFO  "; break;
            case Warn:  level_str = "WARN  "; break;
            case Error: level_str = "ERROR "; break;
            default:    level_str = "CUSTM "; break;
        }
    }

    // Extract filename without heap allocation
    const char *filename = file;
    if (file && !class_name && line != -1) {
        const char *last_sep = strrchr(file, '/');
        #ifdef _WIN32
            const char *last_sep_win = strrchr(file, '\\');
            if (last_sep_win && (!last_sep || last_sep_win > last_sep))
                last_sep = last_sep_win;
        #endif
        if (last_sep)
            filename = last_sep + 1;
    }

    // Process message line by line
    size_t pos = 0;
    size_t line_idx = 0;

    while (pos < msg.size() || line_idx == 0) {
        // Find next newline
        size_t newline = msg.find('\n', pos);
        size_t line_end = (newline != std::string_view::npos) ? newline : msg.size();

        if (line_idx > 0)
            result.push_back('\n');

        // Date/Time
        if (m_has_date)
            result.append(time_buffer);

        // Log level
        if (m_has_log_level)
            result.append(level_str);

        // Class
        if (m_has_class) {
            if (class_name) {
                result.push_back('[');
                result.append(class_name);
                result.append("] ");
            } else if (line != -1 && file) {
                result.push_back('[');
                result.append(filename);
                result.push_back(':');
                result.append(std::to_string(line));
                result.append("] ");
            }
        }

        // Message line
        result.append(msg.substr(pos, line_end - pos));

        line_idx++;
        pos = (newline != std::string_view::npos) ? newline + 1 : msg.size();

        // Break if we've processed all content
        if (pos >= msg.size() && line_idx > 0)
            break;
    }

    return result;
}

NAMESPACE_END(mitsuba)
