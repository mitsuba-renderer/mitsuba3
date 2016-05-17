#include <mitsuba/core/logger.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/appender.h>
#include <mitsuba/core/formatter.h>
#include <mitsuba/core/util.h>

#include <thread>
#include <vector>
#include <iostream>
#include <algorithm>
#include <mutex>

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

#undef Throw

void Logger::log(ELogLevel level, const Class *theClass, const char *file,
                 int line, const std::string &msg) {

    if (level < m_logLevel)
        return;
    else if (level >= d->errorLevel)
        detail::Throw(level, theClass, file, line, msg);

    if (!d->formatter) {
        std::cerr << "PANIC: Logging has not been properly initialized!" << std::endl;
        abort();
    }

    std::string text = d->formatter->format(level, theClass,
        Thread::getThread(), file, line, msg);

    std::lock_guard<std::mutex> guard(d->mutex);
    for (auto entry : d->appenders)
        entry->append(level, text);
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

NAMESPACE_BEGIN(detail)

void Throw(ELogLevel level, const Class *theClass, const char *file,
           int line, const std::string &msg) {
    /* Trap if we're running in a debugger to facilitate debugging */
    util::trapDebugger();

    DefaultFormatter formatter;
    formatter.setHaveDate(false);
    formatter.setHaveLogLevel(false);
    formatter.setHaveThread(false);
    std::string text =
        formatter.format(level, theClass, Thread::getThread(), file, line, msg);
    throw std::runtime_error(text);
}

NAMESPACE_END(detail)

MTS_IMPLEMENT_CLASS(Logger, Object)
NAMESPACE_END(mitsuba)
