#include <mitsuba/core/formatter.h>
#include <mitsuba/core/logger.h>
#include <ctime>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

DefaultFormatter::DefaultFormatter()
    : m_haveDate(true), m_haveLogLevel(true), m_haveThread(true),
      m_haveClass(true) { }

std::string DefaultFormatter::format(ELogLevel level, const Class *theClass,
                                     const Thread *thread, const char *, int,
                                     const std::string &msg) {
    std::ostringstream oss;
    char buffer[128];

    /* Date/Time */
    if (m_haveDate) {
        time_t theTime = std::time(nullptr);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S ", std::localtime(&theTime));
        oss << buffer;
    }

    /* Log level */
    if (m_haveLogLevel) {
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
    if (thread && m_haveThread) {
        oss << thread->getName();

        for (int i=0; i<(5 - (int) thread->getName().size()); i++)
            oss << ' ';
    }

    /* Class */
    if (m_haveClass) {
        if (theClass)
            oss << "[" << theClass->getName() << "] ";
        //else if (line != -1 && file) // XXX
            //oss << "[" << fs::path(file).filename().string() << ":" << line << "] ";
    }

    /* Message */
    oss << msg;

    return oss.str();
}

MTS_IMPLEMENT_CLASS(Formatter, Object)
MTS_IMPLEMENT_CLASS(DefaultFormatter, Formatter)
NAMESPACE_END(mitsuba)
