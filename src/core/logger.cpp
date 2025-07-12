#include <mitsuba/core/logger.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/appender.h>
#include <mitsuba/core/formatter.h>

#include <iostream>
#include <algorithm>
#include <mutex>
#include <string_view>

NAMESPACE_BEGIN(mitsuba)

struct Logger::LoggerPrivate {
    std::mutex mutex;
    LogLevel error_level = Error;
    std::vector<ref<Appender>> appenders;
    ref<Formatter> formatter;
};

Logger::Logger(LogLevel log_level)
    : m_log_level(log_level), d(new LoggerPrivate()) { }

Logger::~Logger() { }

void Logger::set_formatter(Formatter *formatter) {
    std::lock_guard<std::mutex> guard(d->mutex);
    d->formatter = formatter;
}

Formatter *Logger::formatter() {
    return d->formatter;
}

const Formatter *Logger::formatter() const {
    return d->formatter;
}

void Logger::set_log_level(LogLevel level) {
    Assert(level <= d->error_level);
    m_log_level = level;
}

void Logger::set_error_level(LogLevel level) {
    Assert(level <= Error && level >= m_log_level);
    d->error_level = level;
}

LogLevel Logger::error_level() const {
    return d->error_level;
}

#undef Throw

void Logger::log(LogLevel level, const char *cname, const char *fname,
                 int line, std::string_view msg) {

    if (level < m_log_level)
        return;
    else if (level >= d->error_level)
        detail::Throw(level, cname, fname, line, msg);

    if (!d->formatter) {
        std::cerr << "PANIC: Logging has not been properly initialized!" << std::endl;
        abort();
    }

    std::string text = d->formatter->format(level, cname,
        fname, line, msg);

    std::lock_guard<std::mutex> guard(d->mutex);
    for (auto entry : d->appenders)
        entry->append(level, text);
}

void Logger::log_progress(float progress, std::string_view name,
    std::string_view formatted, std::string_view eta, const void *ptr) {
    std::lock_guard<std::mutex> guard(d->mutex);
    for (auto entry : d->appenders)
        entry->log_progress(progress, name, formatted, eta, ptr);
}

void Logger::add_appender(Appender *appender) {
    std::lock_guard<std::mutex> guard(d->mutex);
    d->appenders.push_back(appender);
}

void Logger::remove_appender(Appender *appender) {
    std::lock_guard<std::mutex> guard(d->mutex);
    d->appenders.erase(std::remove(d->appenders.begin(),
        d->appenders.end(), ref<Appender>(appender)), d->appenders.end());
}

std::string Logger::read_log() {
    std::lock_guard<std::mutex> guard(d->mutex);
    for (Appender *appender: d->appenders) {
        StreamAppender *sa = dynamic_cast<StreamAppender*>(appender);
        if (sa && sa->logs_to_file())
            return sa->read_log();
    }
    Log(Error, "No stream appender with a file attachment could be found");
    return std::string(); /* Don't warn */
}

void Logger::clear_appenders() {
    std::lock_guard<std::mutex> guard(d->mutex);
    d->appenders.clear();
}

void Logger::static_initialization() {
    Logger *logger = new Logger(Info);
    ref<Appender> appender = new StreamAppender(&std::cout);
    ref<Formatter> formatter = new DefaultFormatter();
    logger->add_appender(appender);
    logger->set_formatter(formatter);
    set_logger(logger);
#if defined(NDEBUG)
    logger->set_log_level(Info);
#else
    logger->set_log_level(Debug);
#endif
}

// Removal of logger from main thread is handled in thread.cpp.
void Logger::static_shutdown() {
    set_logger(nullptr);
}

size_t Logger::appender_count() const {
    return d->appenders.size();
}

Appender *Logger::appender(size_t index) {
    return d->appenders[index];
}

const Appender *Logger::appender(size_t index) const {
    return d->appenders[index];
}

NAMESPACE_BEGIN(detail)

void Throw(LogLevel level, const char *cname, const char *fname,
           int line, std::string_view msg) {
    // Trap if we're running in a debugger to facilitate debugging.
    #if defined(MI_THROW_TRAPS_DEBUGGER)
    util::trap_debugger();
    #endif

    DefaultFormatter formatter;
    formatter.set_has_date(false);
    formatter.set_has_log_level(false);
    formatter.set_has_thread(false);

    throw std::runtime_error(formatter.format(level, cname, fname, line, msg));
}

NAMESPACE_END(detail)

static ref<Logger> __static_logger;

void set_logger(Logger *logger) { __static_logger = logger; }
Logger *logger() { return __static_logger.get(); }

NAMESPACE_END(mitsuba)
