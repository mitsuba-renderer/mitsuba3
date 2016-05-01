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
     * \param theClass  Originating class or nullptr
     * \param thread    Thread, which is reponsible for creating the message
     * \param text      Text content associated with the log message
     * \param file      File, which is responsible for creating the message
     * \param line      Associated line within the source file
     */

    virtual std::string format(ELogLevel level, const Class *theClass,
            const Thread *thread, const std::string &text,
            const char *file, int line) = 0;

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

    std::string format(ELogLevel level, const Class *theClass,
            const Thread *thread, const std::string &text,
            const char *file, int line) override;

    /// Should date information be included? The default is yes.
    void setHaveDate(bool value) { m_haveDate = value; }
    /// \sa setHaveDate
    bool getHaveDate() { return m_haveDate; }

    /// Should thread information be included? The default is yes.
    void setHaveThread(bool value) { m_haveThread = value; }
    /// \sa setHaveThread
    bool getHaveThread() { return m_haveThread; }

    /// Should log level information be included? The default is yes.
    void setHaveLogLevel(bool value) { m_haveLogLevel = value; }
    /// \sa setHaveLogLevel
    bool getHaveLogLevel() { return m_haveLogLevel; }

    /// Should class information be included? The default is yes.
    void setHaveClass(bool value) { m_haveClass = value; }
    /// \sa setHaveClass
    bool getHaveClass() { return m_haveClass; }

    MTS_DECLARE_CLASS()

protected:
    /// Virtual destructor
    virtual ~DefaultFormatter() { }

protected:
    bool m_haveDate;
    bool m_haveLogLevel;
    bool m_haveThread;
    bool m_haveClass;
};

NAMESPACE_END(mitsuba)
