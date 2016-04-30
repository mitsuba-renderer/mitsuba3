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
    /// Possible priority values for \ref Thread::setPriority()
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
    bool setPriority(EPriority priority);

    /// Return the thread priority
    EPriority getPriority() const;

    /**
     * \brief Set the core affinity
     *
     * This function provides a hint to the operating system scheduler that
     * the thread should preferably run on the specified processor core. By
     * default, the parameter is set to -1, which means that there is no
     * affinity.
     */
    void setCoreAffinity(int core);

    /// Return the core affinity
    int getCoreAffinity() const;

    /**
     * \brief Specify whether or not this thread is critical
     *
     * When an thread marked critical crashes from an uncaught exception, the
     * whole process is brought down. The default is \c false.
     */
    void setCritical(bool critical);

    /// Return the value of the critical flag
    bool getCritical() const;

    /// Return a unique ID that is associated with this thread
    static int getID();

    /// Return the name of this thread
    const std::string &getName() const;

    /// Set the name of this thread
    void setName(const std::string &name);

    /// Return the parent thread
    Thread *getParent();

    /// Return the parent thread (const version)
    const Thread *getParent() const;

    /// Set the logger instance used to process log messages from this thread
    void setLogger(Logger *logger);

    /// Return the thread's logger instance
    Logger *getLogger();

#if 0
    /// Set the thread's file resolver
    void setFileResolver(FileResolver *fresolver);

    /// Return the thread's file resolver
    FileResolver *getFileResolver();
#endif

    /// Return the current thread
    static Thread *getThread();

    /// Is this thread still running?
    bool isRunning() const;

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
    virtual std::string toString() const;

    /// Sleep for a certain amount of time
    static void sleep(unsigned int ms);

    /// Initialize the threading system
    static void staticInitialization();

    /// Shut down the threading system
    static void staticShutdown();

    MTS_DECLARE_CLASS()
protected:
    /// Virtual destructor
    virtual ~Thread();

    /// Thread dispatch function
    static void dispatch(Thread *thread);

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

NAMESPACE_END(mitsuba)
