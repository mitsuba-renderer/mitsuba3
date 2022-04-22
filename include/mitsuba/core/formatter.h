#pragma once

#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

/** \brief Abstract interface for converting log information into
 * a human-readable format
 */
class MI_EXPORT_LIB Formatter : public Object {
public:
    /**
     * \brief Turn a log message into a human-readable format
     * \param level     The importance of the debug message
     * \param class_    Originating class or \c nullptr
     * \param thread    Thread, which is responsible for creating the message
     * \param file      File, which is responsible for creating the message
     * \param line      Associated line within the source file
     * \param msg       Text content associated with the log message
     */

    virtual std::string format(LogLevel level, const Class *class_,
                               const Thread *thread, const char *file, int line,
                               const std::string &msg) = 0;

    MI_DECLARE_CLASS()
protected:
    /// Protected destructor
    virtual ~Formatter() = default;
};

/** \brief The default formatter used to turn log messages into
 * a human-readable form
 */
class MI_EXPORT_LIB DefaultFormatter : public Formatter {
    friend class Logger;
public:
    /// Create a new default formatter
    DefaultFormatter();

    std::string format(LogLevel level, const Class *class_,
                       const Thread *thread, const char *file, int line,
                       const std::string &msg) override;

    /// Should date information be included? The default is yes.
    void set_has_date(bool value) { m_has_date = value; }
    /// \sa set_has_date
    bool has_date() { return m_has_date; }

    /// Should thread information be included? The default is yes.
    void set_has_thread(bool value) { m_has_thread = value; }
    /// \sa set_has_thread
    bool has_thread() { return m_has_thread; }

    /// Should log level information be included? The default is yes.
    void set_has_log_level(bool value) { m_has_log_level = value; }
    /// \sa set_has_log_level
    bool has_log_level() { return m_has_log_level; }

    /// Should class information be included? The default is yes.
    void set_has_class(bool value) { m_has_class = value; }
    /// \sa set_has_class
    bool has_class() { return m_has_class; }

    MI_DECLARE_CLASS()
protected:
    bool m_has_date;
    bool m_has_log_level;
    bool m_has_thread;
    bool m_has_class;
};

NAMESPACE_END(mitsuba)
