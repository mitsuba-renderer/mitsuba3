#pragma once

#include <mitsuba/core/string.h>
#include <mitsuba/core/thread.h>
#include <tinyformat.h>

NAMESPACE_BEGIN(mitsuba)

/// Available Log message types
enum LogLevel : int {
    Trace = 0,   /// Trace message, for extremely verbose debugging
    Debug = 100, /// Debug message, usually turned off
    Info  = 200, /// More relevant debug / information message
    Warn  = 300, /// Warning message
    Error = 400  /// Error message, causes an exception to be thrown
};

/**
 * \brief Responsible for processing log messages
 *
 * Upon receiving a log message, the Logger class invokes a Formatter to
 * convert it into a human-readable form. Following that, it sends this
 * information to every registered Appender.
 *
 * \ingroup libcore
 */
class MI_EXPORT_LIB Logger : public Object {
public:
    /// Construct a new logger with the given minimum log level
    Logger(LogLevel log_level = Debug);

    /**
     * \brief Process a log message
     * \param level      Log level of the message
     * \param class_     Class descriptor of the message creator
     * \param filename   Source file of the message creator
     * \param line       Source line number of the message creator
     * \param fmt        printf-style string formatter
     *
     * \note This function is not exposed in the Python bindings.
     *       Instead, please use \cc mitsuba.core.Log
     */
    void log(LogLevel level, const Class *class_, const char *filename,
             int line, const std::string &message);

    /**
     * \brief Process a progress message
     * \param progress Percentage value in [0, 100]
     * \param name Title of the progress message
     * \param formatted Formatted string representation of the message
     * \param eta Estimated time until 100% is reached.
     * \param ptr Custom pointer payload. This is used to express the
     *    context of a progress message. When rendering a scene, it
     *    will usually contain a pointer to the associated \c RenderJob.
     */
    void log_progress(float progress, const std::string &name,
        const std::string &formatted, const std::string &eta,
        const void *ptr = nullptr);

    /// Set the log level (everything below will be ignored)
    void set_log_level(LogLevel level);

    /**
     * \brief Set the error log level (this level and anything above will throw
     * exceptions).
     *
     * The value provided here can be used for instance to turn warnings into
     * errors. But \a level must always be less than \ref Error, i.e. it isn't
     * possible to cause errors not to throw an exception.
     */
    void set_error_level(LogLevel level);

    /// Return the current log level
    LogLevel log_level() const { return m_log_level; }

    /// Return the current error level
    LogLevel error_level() const;

    /// Add an appender to this logger
    void add_appender(Appender *appender);

    /// Remove an appender from this logger
    void remove_appender(Appender *appender);

    /// Remove all appenders from this logger
    void clear_appenders();

    /// Return the number of registered appenders
    size_t appender_count() const;

    /// Return one of the appenders
    Appender *appender(size_t index);

    /// Return one of the appenders
    const Appender *appender(size_t index) const;

    /// Set the logger's formatter implementation
    void set_formatter(Formatter *formatter);

    /// Return the logger's formatter implementation
    Formatter *formatter();

    /// Return the logger's formatter implementation (const)
    const Formatter *formatter() const;

    /**
     * \brief Return the contents of the log file as a string
     *
     * Throws a runtime exception upon failure
     */
    std::string read_log();

    /// Initialize logging
    static void static_initialization();

    /// Shutdown logging
    static void static_shutdown();

    MI_DECLARE_CLASS()
protected:
    /// Protected destructor
    virtual ~Logger();

private:
    struct LoggerPrivate;
    LogLevel m_log_level;
    std::unique_ptr<LoggerPrivate> d;
};

NAMESPACE_BEGIN(detail)

[[noreturn]] extern MI_EXPORT_LIB
void Throw(LogLevel level, const Class *class_, const char *file,
           int line, const std::string &msg);

template <typename... Args> MI_INLINE
static void Log(LogLevel level, const Class *class_,
                const char *filename, int line, Args &&... args) {
    auto logger = mitsuba::Thread::thread()->logger();
    if (logger && level >= logger->log_level())
        logger->log(level, class_, filename, line, tfm::format(std::forward<Args>(args)...));
}

NAMESPACE_END(detail)

/// Write a log message to the console
#define Log(level, ...)                                                        \
    do {                                                                       \
        mitsuba::detail::Log(level, m_class, __FILE__, __LINE__,               \
                               ##__VA_ARGS__);                                 \
    } while (0)

/// Throw an exception
#define Throw(...)                                                             \
    do {                                                                       \
        mitsuba::detail::Throw(Error, m_class, __FILE__, __LINE__,            \
                      tfm::format(__VA_ARGS__));                               \
    } while (0)

#if !defined(NDEBUG)
/// Assert that a condition is true
#define MI_ASSERT1(cond) do { \
        if (!(cond)) Throw("Assertion \"%s\" failed in " \
            "%s:%i", #cond, __FILE__, __LINE__); \
    } while (0)

/// Assertion with a customizable error explanation
#define MI_ASSERT2(cond, explanation) do { \
        if (!(cond)) Throw("Assertion \"%s\" failed in " \
            "%s:%i (" explanation ")", #cond, __FILE__, __LINE__); \
    } while (0)

/// Expose both of the above macros using overloading, i.e. <tt>Assert(cond)</tt> or <tt>Assert(cond, explanation)</tt>
#define Assert(...) MI_EXPAND(MI_EXPAND(MI_CAT(MI_ASSERT, \
                MI_VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))
#else
#define Assert(...) ((void) 0)
#endif  // !defined(NDEBUG)

/// Throw an exception reporting that the given function is not implemented
#define NotImplementedError(Name) \
    Throw("%s::" Name "(): not implemented!", class_()->name());

NAMESPACE_END(mitsuba)
