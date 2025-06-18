#pragma once

#include <mitsuba/core/object.h>

extern "C" { struct Task; };

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Dummy thread class for backward compatibility
 *
 * This class has been largely stripped down and only maintains essential
 * methods for file resolver and logger access, plus static initialization.
 * Use std::thread or the nanothread-based thread pool for actual threading needs.
 */
class MI_EXPORT_LIB Thread : public Object {
public:
    /// Create a dummy thread object (for compatibility)
    Thread();

    /// Destructor
    ~Thread();

    /// Return the global file resolver
    static FileResolver *file_resolver();

    /// Return the global logger instance
    static Logger *logger();

    /**
     * \brief Set the global logger used by Mitsuba
     * \deprecated Use mitsuba::set_logger() directly
     */
    [[deprecated]]
    static void set_logger(Logger *logger);

    /// Return the current thread (dummy implementation)
    static Thread *thread();

    /// Register nanothread Task to prevent internal resources leakage
    static void register_task(Task *task);

    /// Wait for previously registered nanothread tasks to complete
    static void wait_for_tasks();

    /// Initialize the threading system
    static void static_initialization();

    /// Shut down the threading system
    static void static_shutdown();

    MI_DECLARE_CLASS(Thread)
};


NAMESPACE_END(mitsuba)
