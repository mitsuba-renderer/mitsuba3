#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/tls.h>
#include <thread>
#include <sstream>
#include <chrono>

// Required for native thread functions
#if defined(__LINUX__)
#  include <sys/prctl.h>
#elif defined(__OSX__)
#  include <pthread.h>
#elif defined(__WINDOWS__)
#  include <windows.h>
#endif

NAMESPACE_BEGIN(mitsuba)

#if defined(__WINDOWS__)
std::string lastErrorText() {
    DWORD errCode = GetLastError();
    char *errorText = NULL;
    if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&errorText,
        0,
        NULL)) {
        return "Internal error while looking up an error code";
    }
    std::string result(errorText);
    LocalFree(errorText);
    return result;
}
#endif

int getCoreCount() {
#if defined(__WINDOWS__)
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    return sys_info.dwNumberOfProcessors;
#elif defined(__OSX__)
    int nprocs;
    size_t nprocsSize = sizeof(int);
    if (sysctlbyname("hw.activecpu", &nprocs, &nprocsSize, NULL, 0))
        SLog(EError, "Could not detect the number of processors!");
    return (int)nprocs;
#else
    return sysconf(_SC_NPROCESSORS_CONF);
#endif
}


static ThreadLocal<Thread> *self = nullptr;
static std::atomic<uint32_t> thread_id { 0 };
#if defined(__LINUX__) || defined(__OSX__)
static pthread_key_t this_thread_id;
#elif defined(__WINDOWS__)
static __declspec(thread) int this_thread_id;
#endif


#if defined(_MSC_VER)
namespace {
    // Helper function to set a native thread name. MSDN:
    // http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
    #pragma pack(push, 8)
        struct THREADNAME_INFO {
            DWORD dwType;     // Must be 0x1000.
            LPCSTR szName;    // Pointer to name (in user addr space).
            DWORD dwThreadID; // Thread ID (-1=caller thread).
            DWORD dwFlags;    // Reserved for future use, must be zero.
        };
    #pragma pack(pop)

    void SetThreadName(const char* threadName, DWORD dwThreadID = -1) {
        THREADNAME_INFO info;
        info.dwType     = 0x1000;
        info.szName     = threadName;
        info.dwThreadID = dwThreadID;
        info.dwFlags    = 0;
        __try {
            const DWORD MS_VC_EXCEPTION = 0x406D1388;
            RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR),
                           (ULONG_PTR *) &info);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
} // namespace
#endif // _MSC_VER

/// Dummy class to associate a thread identity with the main thread
class MainThread : public Thread {
public:
    MainThread() : Thread("main") { }
    virtual void run() override { Log(EError, "The main thread is already running!"); }
    MTS_DECLARE_CLASS()
protected:
    virtual ~MainThread() { }
};

struct Thread::ThreadPrivate {
    std::thread thread;
    std::string name;
    bool running = false;
    bool critical = false;
    int coreAffinity = -1;
    Thread::EPriority priority;
    ref<Logger> logger;
    ref<Thread> parent;

    ThreadPrivate(const std::string &name) : name(name) { }
};

Thread::Thread(const std::string &name)
 : d(new ThreadPrivate(name)) { }

Thread::~Thread() {
    if (d->running)
        Log(EWarn, "Destructor called while thread '%s' was still running", d->name.c_str());
}

void Thread::setCritical(bool critical) {
    d->critical = critical;
}

bool Thread::getCritical() const {
    return d->critical;
}

const std::string &Thread::getName() const {
    return d->name;
}

void Thread::setName(const std::string &name) {
    d->name = name;
}

void Thread::setLogger(Logger *logger) {
    d->logger = logger;
}

Logger* Thread::getLogger() {
    return d->logger;
}

Thread* Thread::getThread() {
    return *self;
}

bool Thread::isRunning() const {
    return d->running;
}

Thread* Thread::getParent() {
    return d->parent;
}

const Thread* Thread::getParent() const {
    return d->parent.get();
}

Thread::EPriority Thread::getPriority() const {
    return d->priority;
}

int Thread::getCoreAffinity() const {
    return d->coreAffinity;
}

uint32_t Thread::getID() {
#if defined(__WINDOWS__)
    return this_thread_id;
#elif defined(__OSX__) || defined(__LINUX__)
    /* pthread_self() doesn't provide nice increasing IDs, and syscall(SYS_gettid)
       causes a context switch. Thus, this function uses a thread-local variable
       to provide a nice linearly increasing sequence of thread IDs */
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pthread_getspecific(this_thread_id)));
#endif
}

bool Thread::setPriority(EPriority priority) {
    d->priority = priority;
    if (!d->running)
        return true;

#if defined(__LINUX__) || defined(__OSX__)
    Float factor;
    switch (priority) {
        case EIdlePriority: factor = 0.0f; break;
        case ELowestPriority: factor = 0.2f; break;
        case ELowPriority: factor = 0.4f; break;
        case EHighPriority: factor = 0.6f; break;
        case EHighestPriority: factor = 0.8f; break;
        case ERealtimePriority: factor = 1.0f; break;
        default: factor = 0.0f; break;
    }

    const pthread_t threadID = d->thread.native_handle();
    struct sched_param param;
    int policy;
    int retval = pthread_getschedparam(threadID, &policy, &param);
    if (retval) {
        Log(EWarn, "pthread_getschedparam(): %s!", strerror(retval));
        return false;
    }

    int min = sched_get_priority_min(policy);
    int max = sched_get_priority_max(policy);

    if (min == max) {
        Log(EWarn, "Could not adjust the thread priority -- valid range is zero!");
        return false;
    }
    param.sched_priority = (int) (min + (max-min)*factor);

    retval = pthread_setschedparam(threadID, policy, &param);
    if (retval) {
        Log(EWarn, "Could not adjust the thread priority to %i: %s!",
            param.sched_priority, strerror(retval));
        return false;
    }
#elif defined(__WINDOWS__)
    int win32Priority;
    switch (priority) {
        case EIdlePriority:     win32Priority = THREAD_PRIORITY_IDLE; break;
        case ELowestPriority:   win32Priority = THREAD_PRIORITY_LOWEST; break;
        case ELowPriority:      win32Priority = THREAD_PRIORITY_BELOW_NORMAL; break;
        case EHighPriority:     win32Priority = THREAD_PRIORITY_ABOVE_NORMAL; break;
        case EHighestPriority:  win32Priority = THREAD_PRIORITY_HIGHEST; break;
        case ERealtimePriority: win32Priority = THREAD_PRIORITY_TIME_CRITICAL; break;
        default:                win32Priority = THREAD_PRIORITY_NORMAL; break;
    }

    // If the function succeeds, the return value is nonzero
    const HANDLE handle = d->thread.native_handle();
    if (SetThreadPriority(handle, win32Priority) == 0) {
        Log(EWarn, "Could not adjust the thread priority to %i: %s!",
            win32Priority, lastErrorText().c_str());
        return false;
    }
#endif
    return true;
}

void Thread::setCoreAffinity(int coreID) {
    d->coreAffinity = coreID;
    if (!d->running)
        return;

#if defined(__OSX__)
    /* CPU affinity not supported on OSX */
#elif defined(__LINUX__)
    int nCores = sysconf(_SC_NPROCESSORS_CONF),
        nLogicalCores = nCores;

    size_t size = 0;
    cpu_set_t *cpuset = nullptr;
    int retval = 0;

    /* The kernel may expect a larger cpu_set_t than would
       be warranted by the physical core count. Keep querying with increasingly
       larger buffers if the pthread_getaffinity_np operation fails */
    for (int i = 0; i<6; ++i) {
        size = CPU_ALLOC_SIZE(nLogicalCores);
        cpuset = CPU_ALLOC(nLogicalCores);
        if (!cpuset) {
            Log(EWarn, "Thread::setCoreAffinity(): could not allocate cpu_set_t");
            return;
        }

        CPU_ZERO_S(size, cpuset);

        int retval = pthread_getaffinity_np(d->native_handle, size, cpuset);
        if (retval == 0)
            break;

        /* Something went wrong -- release memory */
        CPU_FREE(cpuset);

        if (retval == EINVAL) {
            /* Retry with a larger cpuset */
            nLogicalCores *= 2;
        } else {
            break;
        }
    }

    if (retval) {
        Log(EWarn, "Thread::setCoreAffinity(): pthread_getaffinity_np(): could "
            "not read thread affinity map: %s", strerror(retval));
        return;
    }

    int actualCoreID = -1, available = 0;
    for (int i=0; i<nLogicalCores; ++i) {
        if (!CPU_ISSET_S(i, size, cpuset))
            continue;
        if (available++ == coreID) {
            actualCoreID = i;
            break;
        }
    }

    if (actualCoreID == -1) {
        Log(EWarn, "Thread::setCoreAffinity(): out of bounds: %i/%i cores available, requested #%i!",
            available, nCores, coreID);
        CPU_FREE(cpuset);
        return;
    }

    CPU_ZERO_S(size, cpuset);
    CPU_SET_S(actualCoreID, size, cpuset);

    retval = pthread_setaffinity_np(d->native_handle, size, cpuset);
    if (retval) {
        Log(EWarn, "Thread::setCoreAffinity(): pthread_setaffinity_np: failed: %s", strerror(retval));
        CPU_FREE(cpuset);
        return;
    }

    CPU_FREE(cpuset);
#elif defined(__WINDOWS__)
    int nCores = getCoreCount();
    const HANDLE handle = d->thread.native_handle();

    DWORD_PTR mask;

    if (coreID != -1 && coreID < nCores)
        mask = (DWORD_PTR) 1 << coreID;
    else
        mask = (1 << nCores) - 1;

    if (!SetThreadAffinityMask(handle, mask))
        Log(EWarn, "Thread::setCoreAffinity(): SetThreadAffinityMask : failed");
#endif
}

void Thread::start() {
    if (d->running)
        Log(EError, "Thread is already running!");
    if (!self)
        Log(EError, "Threading has not been initialized!");

    Log(EDebug, "Spawning thread \"%s\"", d->name.c_str());

    d->parent = Thread::getThread();

    /* Inherit the parent thread's logger if none was set */
    if (!d->logger)
        d->logger = d->parent->getLogger();

    /* Inherit the parent thread's file resolver if none was set */
    //if (!d->fresolver) XXX
        //d->fresolver = d->parent->getFileResolver();

    d->running = true;

    incRef();
    d->thread = std::thread(&Thread::dispatch, this);
}

void Thread::dispatch() {
    ThreadLocalBase::registerThread();

    uint32_t id = thread_id++;
    #if defined(__LINUX__) || defined(__OSX__)
        pthread_setspecific(this_thread_id, reinterpret_cast<void *>(id));
    #elif defined(__WINDOWS__)
        this_thread_id = id;
    #endif

    *self = this;

    if (d->priority != ENormalPriority)
        setPriority(d->priority);

    if (!d->name.empty()) {
        const std::string threadName = "Mitsuba: " + getName();
        #if defined(__LINUX__)
            pthread_setname_np(pthread_self(), threadName.c_str());
            // prctl(PR_SET_NAME, threadName.c_str());
        #elif defined(__OSX__)
            pthread_setname_np(threadName.c_str());
        #elif defined(__WINDOWS__)
            SetThreadName(threadName.c_str());
        #endif
    }

    if (d->coreAffinity != -1)
        setCoreAffinity(d->coreAffinity);

    try {
        run();
    } catch (std::exception &e) {
        ELogLevel warnLogLevel = getLogger()->getErrorLevel() == EError ? EWarn : EInfo;
        Log(warnLogLevel, "Fatal error: uncaught exception: \"%s\"", e.what());
        if (d->critical)
            abort();
    }

    exit();

}

void Thread::exit() {
    Log(EDebug, "Thread \"%s\" has finished", d->name.c_str());
    d->running = false;
    Assert(*self == this);
    ThreadLocalBase::unregisterThread();
    decRef();
}

void Thread::join() {
    d->thread.join();
}

void Thread::detach() {
    d->thread.detach();
}

void Thread::sleep(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void Thread::yield() {
    std::this_thread::yield();
}

std::string Thread::toString() const {
    std::ostringstream oss;
    oss << "Thread[" << std::endl
        << "  name = \"" << d->name << "\"," << std::endl
        << "  running = " << d->running << "," << std::endl
        << "  priority = " << d->priority << "," << std::endl
        << "  critical = " << d->critical << std::endl
        << "]";
    return oss.str();
}

void Thread::staticInitialization() {
    #if defined(__OSX__)
        //__mts_autorelease_init(); XXX
    #endif
    #if defined(__LINUX__) || defined(__OSX__)
        pthread_key_create(&this_thread_id, nullptr);
    #endif
    ThreadLocalBase::staticInitialization();
    ThreadLocalBase::registerThread();

    self = new ThreadLocal<Thread>();
    Thread *mainThread = new MainThread();
    mainThread->d->running = true;
    //mainThread->d->fresolver = new FileResolver();
    *self = mainThread;
}

void Thread::staticShutdown() {
    getThread()->d->running = false;
    ThreadLocalBase::unregisterThread();
    delete self;
    self = nullptr;
    ThreadLocalBase::staticShutdown();

    #if defined(__LINUX__) || defined(__OSX__)
        pthread_key_delete(this_thread_id);
    #endif
    #if defined(__OSX__)
    //  __mts_autorelease_shutdown(); // XXX
    #endif
}

MTS_IMPLEMENT_CLASS(Thread, Object)
MTS_IMPLEMENT_CLASS(MainThread, Thread)
NAMESPACE_END(mitsuba)
