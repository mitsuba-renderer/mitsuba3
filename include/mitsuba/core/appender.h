#pragma once

#include <mitsuba/core/object.h>
#include <iosfwd>
#include <string>

NAMESPACE_BEGIN(mitsuba)

/** \brief This class defines an abstract destination
 * for logging-relevant information
 */
class MI_EXPORT_LIB Appender : public Object {
public:
    /// Destructor
    ~Appender() = default;

    /// Append a line of text with the given log level
    virtual void append(LogLevel level, std::string_view text) = 0;

    /**
     * \brief Process a progress message
     *
     * \param progress
     *     Percentage value in [0, 100]
     *
     * \param name
     *     Title of the progress message
     *
     * \param formatted
     *     Formatted string representation of the message
     *
     * \param eta
     *     Estimated time until 100% is reached.
     *
     * \param ptr
     *     Custom pointer payload. This is used to express the
     *     context of a progress message.
     */
    virtual void log_progress(float progress,
                              std::string_view name,
                              std::string_view formatted,
                              std::string_view eta,
                              const void *ptr = nullptr) = 0;

    MI_DECLARE_CLASS(Appender)
};

/** \brief %Appender implementation, which writes to an
 * arbitrary C++ output stream
 */
class MI_EXPORT_LIB StreamAppender : public Appender {
public:
    /**
     * Create a new stream appender
     * \remark This constructor is not exposed in the Python bindings
     */
    StreamAppender(std::ostream *stream);

    /// Create a new stream appender logging to a file
    StreamAppender(std::string_view filename);

    /// Destructor
    ~StreamAppender();

    /// Append a line of text
    void append(LogLevel level, std::string_view text) override;

    /// Process a progress message
    void log_progress(float progress, std::string_view name,
                      std::string_view formatted, std::string_view eta,
                      const void *ptr) override;

    /// Does this appender log to a file
    bool logs_to_file() const { return m_is_file; }

    /// Return the contents of the log file as a string
    std::string read_log();

    /// Return a string representation
    std::string to_string() const override;

    MI_DECLARE_CLASS(StreamAppender)
private:
    std::ostream *m_stream;
    std::string m_fname;
    bool m_is_file;
    bool m_last_message_was_progress;
};

NAMESPACE_END(mitsuba)
