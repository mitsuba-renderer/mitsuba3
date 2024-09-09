#include <mitsuba/core/logger.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/appender.h>
#include <mitsuba/core/formatter.h>

#include <thread>
#include <iostream>
#include <algorithm>
#include <mutex>

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

void Logger::log(LogLevel level, const Class *class_, const char *file,
                 int line, const std::string &msg) {

    if (level < m_log_level)
        return;
    else if (level >= d->error_level)
        detail::Throw(level, class_, file, line, msg);

    if (!d->formatter) {
        std::cerr << "PANIC: Logging has not been properly initialized!" << std::endl;
        abort();
    }

    std::string text = d->formatter->format(level, class_,
        Thread::thread(), file, line, msg);

    std::lock_guard<std::mutex> guard(d->mutex);
    for (auto entry : d->appenders)
        entry->append(level, text);
}

void Logger::log_progress(float progress, const std::string &name,
    const std::string &formatted, const std::string &eta, const void *ptr) {
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
    for (auto appender: d->appenders) {
        if (appender->class_()->derives_from(MI_CLASS(StreamAppender))) {
            auto sa = static_cast<StreamAppender *>(appender.get());
            if (sa->logs_to_file())
                return sa->read_log();
        }
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
    Thread::thread()->set_logger(logger);
#if defined(NDEBUG)
    logger->set_log_level(Info);
#else
    logger->set_log_level(Debug);
#endif
}

// Removal of logger from main thread is handled in thread.cpp.
void Logger::static_shutdown() { }

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

void Throw(LogLevel level, const Class *class_, const char *file,
           int line, const std::string &msg_) {
    // Trap if we're running in a debugger to facilitate debugging.
    #if defined(MI_THROW_TRAPS_DEBUGGER)
    util::trap_debugger();
    #endif

    DefaultFormatter formatter;
    formatter.set_has_date(false);
    formatter.set_has_log_level(false);
    formatter.set_has_thread(false);

    /* Tag beginning of exception text with UTF8 zero width space */
    const std::string zerowidth_space = "\xe2\x80\x8b";

    /* Separate nested exceptions by a newline */
    std::string msg = msg_;
    auto it = msg.find(zerowidth_space);
    if (it != std::string::npos)
        msg = msg.substr(0, it) + "\n  " + msg.substr(it + 3);

    std::string text =
        formatter.format(level, class_, Thread::thread(), file, line, msg);
    throw std::runtime_error(zerowidth_space + text);
}

NAMESPACE_END(detail)

MI_IMPLEMENT_CLASS(Logger, Object)

NAMESPACE_END(mitsuba)
