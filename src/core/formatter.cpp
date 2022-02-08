#include <mitsuba/core/formatter.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/filesystem.h>
#include <ctime>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

DefaultFormatter::DefaultFormatter()
    : m_has_date(true), m_has_log_level(true), m_has_thread(true),
      m_has_class(true) { }

std::string DefaultFormatter::format(mitsuba::LogLevel level, const Class *class_,
                                     const Thread *thread, const char *file, int line,
                                     const std::string &msg) {
    std::ostringstream oss;
    std::istringstream iss(msg);
    char buffer[128];
    std::string msg_line;
    time_t time_ = std::time(nullptr);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S ", std::localtime(&time_));
    int line_idx = 0;

    while (std::getline(iss, msg_line) || line_idx == 0) {
        if (line_idx > 0)
            oss << '\n';

        /* Date/Time */
        if (m_has_date)
            oss << buffer;

        /* Log level */
        if (m_has_log_level) {
            switch (level) {
                case Trace: oss << "TRACE "; break;
                case Debug: oss << "DEBUG "; break;
                case Info:  oss << "INFO  "; break;
                case Warn:  oss << "WARN  "; break;
                case Error: oss << "ERROR "; break;
                default:     oss << "CUSTM "; break;
            }
        }

        /* Thread */
        if (thread && m_has_thread) {
            oss << thread->name();

            for (int i=0; i<(6 - (int) thread->name().size()); i++)
                oss << ' ';
        }

        /* Class */
        if (m_has_class) {
            if (class_)
                oss << "[" << class_->name() << "] ";
            else if (line != -1 && file)
                oss << "[" << fs::path(file).filename() << ":" << line << "] ";
        }

        /* Message */
        oss << msg_line;
        line_idx++;
    }

    return oss.str();
}

MI_IMPLEMENT_CLASS(Formatter, Object)
MI_IMPLEMENT_CLASS(DefaultFormatter, Formatter)

NAMESPACE_END(mitsuba)
