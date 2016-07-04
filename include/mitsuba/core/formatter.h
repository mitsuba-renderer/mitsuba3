#pragma once

#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

/** \brief Abstract interface for converting log information into
 * a human-readable format
 *
 * \ingroup libcore
 */
class MTS_EXPORT_CORE Formatter : public Object {
public:
    /**
     * \brief Turn a log message into a human-readable format
     * \param level     The importance of the debug message
     * \param class_    Originating class or \c nullptr
     * \param thread    Thread, which is reponsible for creating the message
     * \param file      File, which is responsible for creating the message
     * \param line      Associated line within the source file
     * \param msg       Text content associated with the log message
     */

    virtual std::string format(ELogLevel level, const Class *class_,
                               const Thread *thread, const char *file, int line,
                               const std::string &msg) = 0;

    MTS_DECLARE_CLASS()
protected:
    /// Virtual destructor
    virtual ~Formatter() { }
};

/** \brief The default formatter used to turn log messages into
 * a human-readable form
 *
 * \ingroup libcore
 */
class MTS_EXPORT_CORE DefaultFormatter : public Formatter {
    friend class Logger;
public:
    /// Create a new default formatter
    DefaultFormatter();

    std::string format(ELogLevel level, const Class *class_,
                       const Thread *thread, const char *file, int line,
                       const std::string &msg) override;

    /// Should date information be included? The default is yes.
    void setHasDate(bool value) { m_hasDate = value; }
    /// \sa setHasDate
    bool hasDate() { return m_hasDate; }

    /// Should thread information be included? The default is yes.
    void setHasThread(bool value) { m_hasThread = value; }
    /// \sa setHasThread
    bool hasThread() { return m_hasThread; }

    /// Should log level information be included? The default is yes.
    void setHasLogLevel(bool value) { m_hasLogLevel = value; }
    /// \sa setHasLogLevel
    bool hasLogLevel() { return m_hasLogLevel; }

    /// Should class information be included? The default is yes.
    void setHasClass(bool value) { m_hasClass = value; }
    /// \sa setHasClass
    bool hasClass() { return m_hasClass; }

    MTS_DECLARE_CLASS()

protected:
    bool m_hasDate;
    bool m_hasLogLevel;
    bool m_hasThread;
    bool m_hasClass;
};

NAMESPACE_END(mitsuba)
