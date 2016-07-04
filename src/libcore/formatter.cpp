#include <mitsuba/core/formatter.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/filesystem.h>
#include <ctime>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

DefaultFormatter::DefaultFormatter()
    : m_hasDate(true), m_hasLogLevel(true), m_hasThread(true),
      m_hasClass(true) { }

std::string DefaultFormatter::format(ELogLevel level, const Class *class_,
                                     const Thread *thread, const char *file, int line,
                                     const std::string &msg) {
    std::ostringstream oss;
    char buffer[128];

    /* Date/Time */
    if (m_hasDate) {
        time_t theTime = std::time(nullptr);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S ", std::localtime(&theTime));
        oss << buffer;
    }

    /* Log level */
    if (m_hasLogLevel) {
        switch (level) {
            case ETrace: oss << "TRACE "; break;
            case EDebug: oss << "DEBUG "; break;
            case EInfo:  oss << "INFO  "; break;
            case EWarn:  oss << "WARN  "; break;
            case EError: oss << "ERROR "; break;
            default:     oss << "CUSTM "; break;
        }
    }

    /* Thread */
    if (thread && m_hasThread) {
        oss << thread->name();

        for (int i=0; i<(5 - (int) thread->name().size()); i++)
            oss << ' ';
    }

    /* Class */
    if (m_hasClass) {
        if (class_)
            oss << "[" << class_->name() << "] ";
        else if (line != -1 && file)
            oss << "[" << fs::path(file).filename() << ":" << line << "] ";
    }

    /* Message */
    oss << msg;

    return oss.str();
}

MTS_IMPLEMENT_CLASS(Formatter, Object)
MTS_IMPLEMENT_CLASS(DefaultFormatter, Formatter)
NAMESPACE_END(mitsuba)
