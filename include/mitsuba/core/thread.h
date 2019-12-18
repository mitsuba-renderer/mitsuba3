#pragma once

#include <mitsuba/core/object.h>
#include <memory>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Cross-platform thread implementation
 *
 * Mitsuba threads are internally implemented via the <tt>std::thread</tt>
 * class defined in C++11. This wrapper class is needed to attach additional
 * state (Loggers, Path resolvers, etc.) that is inherited when a thread
 * launches another thread.
 */
class MTS_EXPORT_CORE Thread : public Object {
public:

    class TaskObserver; /* Used internally to keep track of TBB threads */
    friend class ScopedThreadEnvironment;

    /// Possible priority values for \ref Thread::set_priority()
    enum EPriority {
        EIdlePriority,
        ELowestPriority,
        ELowPriority,
        ENormalPriority,
        EHighPriority,
        EHighestPriority,
        ERealtimePriority
    };

    /**
     * \brief Create a new thread object
     * \param name An identifying name of this thread
     *   (will be shown in debug messages)
     */
    Thread(const std::string &name);

    /**
     * \brief Set the thread priority
     *
     * This does not always work -- for instance, Linux requires root
     * privileges for this operation.
     *
     * \return \c true upon success.
     */
    bool set_priority(EPriority priority);

    /// Return the thread priority
    EPriority priority() const;

    /**
     * \brief Set the core affinity
     *
     * This function provides a hint to the operating system scheduler that
     * the thread should preferably run on the specified processor core. By
     * default, the parameter is set to -1, which means that there is no
     * affinity.
     */
    void set_core_affinity(int core);

    /// Return the core affinity
    int core_affinity() const;

    /**
     * \brief Specify whether or not this thread is critical
     *
     * When an thread marked critical crashes from an uncaught exception, the
     * whole process is brought down. The default is \c false.
     */
    void set_critical(bool critical);

    /// Return the value of the critical flag
    bool is_critical() const;

    /// Return a unique ID that is associated with this thread
    static uint32_t thread_id();

    /// Return the name of this thread
    const std::string &name() const;

    /// Set the name of this thread
    void set_name(const std::string &name);

    /// Return the parent thread
    Thread *parent();

    /// Return the parent thread (const version)
    const Thread *parent() const;

    /// Set the file resolver associated with the current thread
    void set_file_resolver(FileResolver *resolver);

    /// Return the file resolver associated with the current thread
    FileResolver *file_resolver();

    /// Return the parent thread (const version)
    const FileResolver *file_resolver() const;

    /// Set the logger instance used to process log messages from this thread
    void set_logger(Logger *logger);

    /// Return the thread's logger instance
    Logger *logger();

    /// Return the current thread
    static Thread *thread();

    /// Is this thread still running?
    bool is_running() const;

    /// Start the thread
    void start();

    /**
     * \brief Detach the thread and release resources
     *
     * After a call to this function, \ref join()
     * cannot be used anymore. This releases resources, which
     * would otherwise be held until a call to \ref join().
     */
    void detach();

    /// Wait until the thread finishes
    void join();

    /// Return a string representation
    virtual std::string to_string() const override;

    /// Sleep for a certain amount of time (in milliseconds)
    static void sleep(uint32_t ms);

    /// Initialize the threading system
    static void static_initialization();

    /// Shut down the threading system
    static void static_shutdown();

    /**
     * \brief Register a new thread (e.g. TBB, Python) with Mituba thread system.
     * Returns true upon success.
     */
    static bool register_external_thread(const std::string &prefix);

    /// Unregister a thread (e.g. TBB, Python) from Mitsuba's thread system.
    static bool unregister_external_thread();

    MTS_DECLARE_CLASS()
protected:
    /// Protected destructor
    virtual ~Thread();

    /// Initialize thread execution environment and then call \ref run()
    void dispatch();

    /**
     * Exit the thread, should be called from
     * inside the thread
     */
    void exit();

    /// Yield to another processor
    void yield();

    /// The thread's run method
    virtual void run() = 0;

private:
    struct ThreadPrivate;
    std::unique_ptr<ThreadPrivate> d;
};

/**
 * \brief Captures a thread environment (logger, file resolver, profiler flags).
 * Used with \ref ScopedSetThreadEnvironment
 */
class MTS_EXPORT_CORE ThreadEnvironment {
    friend class ScopedSetThreadEnvironment;
public:
    ThreadEnvironment();
    ~ThreadEnvironment() = default;
    ThreadEnvironment(const ThreadEnvironment &) = default;
    ThreadEnvironment& operator=(const ThreadEnvironment &) = default;
private:
    ref<Logger> m_logger;
    ref<FileResolver> m_file_resolver;
#if defined(MTS_ENABLE_PROFILER)
    uint64_t m_profiler_flags;
#endif
};

/// RAII-style class to temporarily switch to another thread's logger/file resolver
class MTS_EXPORT_CORE ScopedSetThreadEnvironment {
public:
    ScopedSetThreadEnvironment(ThreadEnvironment &env);
    ~ScopedSetThreadEnvironment();

    ScopedSetThreadEnvironment(const ScopedSetThreadEnvironment &) = delete;
    ScopedSetThreadEnvironment& operator=(const ScopedSetThreadEnvironment &) = delete;

private:
    ref<Logger> m_logger;
    ref<FileResolver> m_file_resolver;
#if defined(MTS_ENABLE_PROFILER)
    uint64_t m_profiler_flags;
#endif
};

extern MTS_EXPORT_CORE size_t __global_thread_count;

NAMESPACE_END(mitsuba)
