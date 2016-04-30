#pragma once

#include <mitsuba/core/object.h>
#include <iosfwd>

NAMESPACE_BEGIN(mitsuba)

/** \brief This class defines an abstract destination
 * for logging-relevant information
 *
 * \ingroup libcore
 */
class MTS_EXPORT_CORE Appender : public Object {
public:
    /// Append a line of text with the given log level
    virtual void append(ELogLevel level, const std::string &text) = 0;

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
    virtual void logProgress(Float progress, const std::string &name,
        const std::string &formatted, const std::string &eta,
        const void *ptr = nullptr) = 0;

    MTS_DECLARE_CLASS()
protected:
    /// Virtual destructor
    virtual ~Appender() { }
};

/** \brief %Appender implementation, which writes to an
 * arbitrary C++ output stream
 *
 * \ingroup libcore
 */
class MTS_EXPORT_CORE StreamAppender : public Appender {
public:
    /**
     * Create a new stream appender
     * \remark This constructor is not exposed in the Python bindings
     */
    StreamAppender(std::ostream *pStream);

    /// Create a new stream appender logging to a file
    StreamAppender(const std::string &filename);

    /// Append a line of text
    void append(ELogLevel level, const std::string &pText);

    /// Process a progress message
    void logProgress(Float progress, const std::string &name,
        const std::string &formatted, const std::string &eta,
        const void *ptr);

    /// Does this appender log to a file
    bool logsToFile() const { return m_isFile; }

    /// Return the contents of the log file as a string
    void readLog(std::string &target);

    /// Return a string representation
    std::string toString() const;

    MTS_DECLARE_CLASS()
protected:
    /// Virtual destructor
    virtual ~StreamAppender();
private:
    std::ostream *m_stream;
    std::string m_fileName;
    bool m_isFile;
    bool m_lastMessageWasProgress;
};

NAMESPACE_END(mitsuba)
