#include <mitsuba/core/logger.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/appender.h>
#include <mitsuba/core/formatter.h>

#include <thread>
#include <vector>
#include <iostream>
#include <algorithm>
#include <mutex>
#include <stdarg.h>

#if defined(__OSX__)
# include <sys/sysctl.h>
# include <unistd.h>
#elif defined(__WINDOWS__)
# include <windows.h>
#endif

NAMESPACE_BEGIN(mitsuba)

struct Logger::LoggerPrivate {
    std::mutex mutex;
    ELogLevel errorLevel = EError;
    std::vector<ref<Appender>> appenders;
    ref<Formatter> formatter;
};

Logger::Logger(ELogLevel logLevel)
    : m_logLevel(logLevel), d(new LoggerPrivate()) { }

Logger::~Logger() { }

void Logger::setFormatter(Formatter *formatter) {
    std::lock_guard<std::mutex> guard(d->mutex);
    d->formatter = formatter;
}

Formatter *Logger::getFormatter() {
    return d->formatter;
}

const Formatter *Logger::getFormatter() const {
    return d->formatter;
}

void Logger::setLogLevel(ELogLevel level) {
    Assert(level <= d->errorLevel);
    m_logLevel = level;
}

void Logger::setErrorLevel(ELogLevel level) {
    Assert(level <= EError && level >= m_logLevel);
    d->errorLevel = level;
}

ELogLevel Logger::getErrorLevel() const {
    return d->errorLevel;
}

void Logger::log(ELogLevel level, const Class *theClass,
    const char *file, int line, const char *fmt, ...) {

    if (level < m_logLevel)
        return;

    char tmp[512], *msg = tmp;
    va_list iterator;

#if defined(__WINDOWS__)
    va_start(iterator, fmt);
    size_t size = _vscprintf(fmt, iterator) + 1;

    if (size >= sizeof(tmp))
        msg = new char[size];

    vsnprintf_s(msg, size, size-1, fmt, iterator);
    va_end(iterator);
#else
    va_start(iterator, fmt);
    size_t size = vsnprintf(tmp, sizeof(tmp), fmt, iterator);
    va_end(iterator);

    if (size >= sizeof(tmp)) {
        /* Overflow! -- dynamically allocate memory */
        msg = new char[size+1];
        va_start(iterator, fmt);
        vsnprintf(msg, size+1, fmt, iterator);
        va_end(iterator);
    }
#endif

    if (!d->formatter) {
        std::cerr << "PANIC: Logging has not been properly initialized!" << std::endl;
        abort();
    }

    std::string text = d->formatter->format(level, theClass,
        Thread::getThread(), msg, file, line);

    if (msg != tmp)
        delete[] msg;

    if (level < d->errorLevel) {
        std::lock_guard<std::mutex> guard(d->mutex);
        for (auto entry : d->appenders)
            entry->append(level, text);
    } else {
#if defined(__LINUX__)
        /* A critical error occurred: trap if we're running in a debugger */

        char exePath[PATH_MAX];
        memset(exePath, 0, PATH_MAX);
        std::string procPath = util::formatString("/proc/%i/exe", getppid());

        if (readlink(procPath.c_str(), exePath, PATH_MAX) != -1) {
            if (strstr(exePath, "bin/gdb") || strstr(exePath "bin/lldb")) {
                #if defined(__i386__) || defined(__x86_64__)
                    __asm__ ("int $3");
                #else
                    __builtin_trap();
                #endif
            }
        }
#elif defined(__OSX__)
        int                 mib[4];
        struct kinfo_proc   info;
        size_t              size;
        info.kp_proc.p_flag = 0;
        mib[0] = CTL_KERN;
        mib[1] = KERN_PROC;
        mib[2] = KERN_PROC_PID;
        mib[3] = getpid();
        size = sizeof(info);
        sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, nullptr, 0);

        if (info.kp_proc.p_flag & P_TRACED)
            __asm__ ("int $3");
#elif defined(__WINDOWS__)
        if (IsDebuggerPresent())
            __debugbreak();
#endif

        DefaultFormatter formatter;
        formatter.setHaveDate(false);
        formatter.setHaveLogLevel(false);
        text = formatter.format(level, theClass,
            Thread::getThread(), msg, file, line);
        throw std::runtime_error(text);
    }
}

void Logger::logProgress(Float progress, const std::string &name,
    const std::string &formatted, const std::string &eta, const void *ptr) {
    std::lock_guard<std::mutex> guard(d->mutex);
    for (auto entry : d->appenders)
        entry->logProgress(progress, name, formatted, eta, ptr);
}

void Logger::addAppender(Appender *appender) {
    std::lock_guard<std::mutex> guard(d->mutex);
    d->appenders.push_back(appender);
}

void Logger::removeAppender(Appender *appender) {
    std::lock_guard<std::mutex> guard(d->mutex);
    d->appenders.erase(std::remove(d->appenders.begin(),
        d->appenders.end(), ref<Appender>(appender)), d->appenders.end());
}

std::string Logger::readLog() {
    std::lock_guard<std::mutex> guard(d->mutex);
    for (auto appender: d->appenders) {
        if (appender->getClass()->derivesFrom(MTS_CLASS(StreamAppender))) {
            auto sa = static_cast<StreamAppender *>(appender.get());
            if (sa->logsToFile())
                return sa->readLog();
        }
    }
    Log(EError, "No stream appender with a file attachment could be found");
    return std::string(); /* Don't warn */
}

void Logger::clearAppenders() {
    std::lock_guard<std::mutex> guard(d->mutex);
    d->appenders.clear();
}

void Logger::staticInitialization() {
    Logger *logger = new Logger(EInfo);
    ref<Appender> appender = new StreamAppender(&std::cout);
    ref<Formatter> formatter = new DefaultFormatter();
    logger->addAppender(appender);
    logger->setFormatter(formatter);
    Thread::getThread()->setLogger(logger);
}

void Logger::staticShutdown() {
    Thread::getThread()->setLogger(nullptr);
}

size_t Logger::getAppenderCount() const {
    return d->appenders.size();
}

Appender *Logger::getAppender(size_t index) {
    return d->appenders[index];
}

const Appender *Logger::getAppender(size_t index) const {
    return d->appenders[index];
}

MTS_IMPLEMENT_CLASS(Logger, Object)
NAMESPACE_END(mitsuba)
