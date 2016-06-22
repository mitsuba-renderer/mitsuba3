#pragma once

#include <mitsuba/core/string.h>
#include <mitsuba/core/thread.h>
#include <memory>
#include <tinyformat.h>

NAMESPACE_BEGIN(mitsuba)

/// Available Log message types
enum ELogLevel : int {
    ETrace = 0,   ///< Trace message, for extremely verbose debugging
    EDebug = 100, ///< Debug message, usually turned off
    EInfo = 200,  ///< More relevant debug / information message
    EWarn = 300,  ///< Warning message
    EError = 400  ///< Error message, causes an exception to be thrown
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
class MTS_EXPORT_CORE Logger : public Object {
public:
    /// Construct a new logger with the given minimum log level
    Logger(ELogLevel logLevel = EDebug);

    /**
     * \brief Process a log message
     * \param level Log level of the message
     * \param theClass Class descriptor of the message creator
     * \param filename Source file of the message creator
     * \param line Source line number of the message creator
     * \param fmt printf-style string formatter
     * \note This function is not exposed in the Python bindings.
     *       Instead, please use \cc mitsuba.core.Log
     */
    void log(ELogLevel level, const Class *theClass, const char *filename,
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
    void logProgress(Float progress, const std::string &name,
        const std::string &formatted, const std::string &eta,
        const void *ptr = nullptr);

    /// Set the log level (everything below will be ignored)
    void setLogLevel(ELogLevel level);

    /**
     * \brief Set the error log level (this level and anything above will throw
     * exceptions).
     *
     * The value provided here can be used for instance to turn warnings into
     * errors. But \a level must always be less than \ref EError, i.e. it isn't
     * possible to cause errors not to throw an exception.
     */
    void setErrorLevel(ELogLevel level);

    /// Return the current log level
    ELogLevel getLogLevel() const { return m_logLevel; }

    /// Return the current error level
    ELogLevel getErrorLevel() const;

    /// Add an appender to this logger
    void addAppender(Appender *appender);

    /// Remove an appender from this logger
    void removeAppender(Appender *appender);

    /// Remove all appenders from this logger
    void clearAppenders();

    /// Return the number of registered appenders
    size_t getAppenderCount() const;

    /// Return one of the appenders
    Appender *getAppender(size_t index);

    /// Return one of the appenders
    const Appender *getAppender(size_t index) const;

    /// Set the logger's formatter implementation
    void setFormatter(Formatter *formatter);

    /// Return the logger's formatter implementation
    Formatter *getFormatter();

    /// Return the logger's formatter implementation (const)
    const Formatter *getFormatter() const;

    /**
     * \brief Return the contents of the log file as a string
     *
     * Throws a runtime exception upon failure
     */
    std::string readLog();

    /// Initialize logging
    static void staticInitialization();

    /// Shutdown logging
    static void staticShutdown();

    MTS_DECLARE_CLASS()

protected:
    /// Virtual destructor
    virtual ~Logger();

private:
    struct LoggerPrivate;
    ELogLevel m_logLevel;
    std::unique_ptr<LoggerPrivate> d;
};

NAMESPACE_BEGIN(detail)

[[noreturn]] extern MTS_EXPORT_CORE
void Throw(ELogLevel level, const Class *theClass, const char *file,
           int line, const std::string &msg);

template <typename... Args> MTS_FORCEINLINE
static void Log(ELogLevel level, const Class *theClass,
                         const char *filename, int line, Args &&... args) {
    auto logger = mitsuba::Thread::getThread()->getLogger();
    if (logger && level >= logger->getLogLevel())
        logger->log(level, theClass, filename, line, tfm::format(std::forward<Args>(args)...));
}

NAMESPACE_END(detail)

/// Write a log message to the console
#define Log(level, ...)                                                        \
    do {                                                                       \
        mitsuba::detail::Log(level, m_theClass, __FILE__, __LINE__,            \
                               ##__VA_ARGS__);                                 \
    } while (0)

/// Throw an exception
#define Throw(...)                                                             \
    do {                                                                       \
        mitsuba::detail::Throw(EError, m_theClass, __FILE__, __LINE__,         \
                      tfm::format(__VA_ARGS__));                               \
    } while (0)

#if !defined(NDEBUG)
/// Assert that a condition is true
#define MTS_ASSERT1(cond) do { \
        if (!(cond)) Throw("Assertion \"%s\" failed in " \
            "%s:%i", #cond, __FILE__, __LINE__); \
    } while (0)

/// Assertion with a customizable error explanation
#define MTS_ASSERT2(cond, explanation) do { \
        if (!(cond)) Throw("Assertion \"%s\" failed in " \
            "%s:%i (" explanation ")", #cond, __FILE__, __LINE__); \
    } while (0)

/// Expose both of the above macros using overloading, i.e. <tt>Assert(cond)</tt> or <tt>Assert(cond, explanation)</tt>
#define Assert(...) MTS_EXPAND(MTS_EXPAND(MTS_CAT(MTS_ASSERT, \
                MTS_VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))
#else
#define Assert(...) ((void) 0)
#endif

/// Throw an exception reporting that the given function is not implemented
#define NotImplementedError(funcName) \
    Throw("%s::" funcName "(): Not implemented!", getClass()->getName());

NAMESPACE_END(mitsuba)
