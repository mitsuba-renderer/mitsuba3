#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/fresolver.h>
#include <nanothread/nanothread.h>
#include <thread>
#include <mutex>
#include <vector>
#include <sstream>
#include <chrono>
#include <cstring>
#include <unordered_map>

// Required for native thread functions
#if defined(__linux__)
#  include <sys/prctl.h>
#  include <unistd.h>
#elif defined(__APPLE__)
#  include <pthread.h>
#elif defined(_WIN32)
#  include <windows.h>
#endif

NAMESPACE_BEGIN(mitsuba)

static size_t global_thread_count = 0;

static ref<Thread> main_thread = nullptr;
static thread_local ref<Thread> self = nullptr;
static std::atomic<uint32_t> thread_ctr { 0 };
#if defined(__linux__) || defined(__APPLE__)
static pthread_key_t this_thread_id;
#elif defined(_WIN32)
static __declspec(thread) int this_thread_id;
#endif
static std::mutex task_mutex;
static std::vector<Task *> registered_tasks;

static std::mutex thread_registry_mutex;
static std::unordered_map<std::string, Thread*> thread_registry;

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

    void set_thread_name_(const char* thread_name, DWORD thread_id = -1) {
        THREADNAME_INFO info;
        info.dwType     = 0x1000;
        info.szName     = thread_name;
        info.dwThreadID = thread_id;
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

    virtual void run() override { Log(Error, "The main thread is already running!"); }

    MI_DECLARE_CLASS()
protected:
    virtual ~MainThread() { }
};

/// Dummy class to associate a thread identity with a worker thread
class WorkerThread : public Thread {
public:
    WorkerThread(const std::string &prefix) : Thread(tfm::format("%s%i", prefix, m_counter++)) {
        std::lock_guard guard(thread_registry_mutex);
        thread_registry[this->name()] = this;
    }

    virtual void run() override { Throw("The worker thread is already running!"); }

    MI_DECLARE_CLASS()
protected:
    virtual ~WorkerThread() {
        std::lock_guard guard(thread_registry_mutex);
        thread_registry.erase(this->name());
    }
    static std::atomic<uint32_t> m_counter;
};

std::atomic<uint32_t> WorkerThread::m_counter{0};

struct ThreadNotifier {
    ThreadNotifier() {
        // Do not register the main thread
        if (m_counter > 0)
            Thread::register_external_thread("wrk");
        m_counter++;
    }
    ~ThreadNotifier() {
        if (self)
            Thread::unregister_external_thread();
        m_counter--;
    }
    void ensure_initialized() {}
    static std::atomic<uint32_t> m_counter;
};

std::atomic<uint32_t> ThreadNotifier::m_counter{0};
static thread_local ThreadNotifier notifier{};

struct Thread::ThreadPrivate {
    std::thread thread;
    std::thread::native_handle_type native_handle;
    std::string name;
    bool running = false;
    bool external_thread = false;
    bool critical = false;
    int core_affinity = -1;
    Thread::EPriority priority;
    ref<Logger> logger;
    ref<Thread> parent;
    ref<FileResolver> fresolver;

    ThreadPrivate(const std::string &name) : name(name) { }
};

Thread::Thread(const std::string &name)
 : d(new ThreadPrivate(name)) { }

Thread::~Thread() {
    if (d->running)
        Log(Warn, "Destructor called while thread '%s' was still running", d->name);
}

void Thread::set_critical(bool critical) {
    d->critical = critical;
}

bool Thread::is_critical() const {
    return d->critical;
}

const std::string &Thread::name() const {
    return d->name;
}

void Thread::set_name(const std::string &name) {
    d->name = name;
}

void Thread::set_logger(Logger *logger) {
    d->logger = logger;
}

Logger* Thread::logger() {
    return d->logger;
}

void Thread::set_file_resolver(FileResolver *fresolver) {
    d->fresolver = fresolver;
}

FileResolver* Thread::file_resolver() {
    return d->fresolver;
}

const FileResolver* Thread::file_resolver() const {
    return d->fresolver;
}

Thread* Thread::thread() {
    notifier.ensure_initialized();
    Thread *self_val = self;
    assert(self_val);
    return self_val;
}

Thread* Thread::get_main_thread() {
    Thread *main_val = main_thread;
    assert(main_val);
    return main_val;
}

bool Thread::has_initialized_thread() {
    return self != nullptr;
}

bool Thread::is_running() const {
    return d->running;
}

Thread* Thread::parent() {
    return d->parent;
}

const Thread* Thread::parent() const {
    return d->parent.get();
}

Thread::EPriority Thread::priority() const {
    return d->priority;
}

int Thread::core_affinity() const {
    return d->core_affinity;
}

uint32_t Thread::thread_id() {
#if defined(_WIN32)
    return this_thread_id;
#elif defined(__APPLE__) || defined(__linux__)
    /* pthread_self() doesn't provide nice increasing IDs, and syscall(SYS_gettid)
       causes a context switch. Thus, this function uses a thread-local variable
       to provide a nice linearly increasing sequence of thread IDs */
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pthread_getspecific(this_thread_id)));
#endif
}

bool Thread::set_priority(EPriority priority) {
    d->priority = priority;
    if (!d->running)
        return true;

#if defined(__linux__) || defined(__APPLE__)
    float factor;
    switch (priority) {
        case EIdlePriority: factor = 0.f; break;
        case ELowestPriority: factor = 0.2f; break;
        case ELowPriority: factor = 0.4f; break;
        case EHighPriority: factor = 0.6f; break;
        case EHighestPriority: factor = 0.8f; break;
        case ERealtimePriority: factor = 1.f; break;
        default: factor = 0.f; break;
    }

    const pthread_t thread_id = d->native_handle;
    struct sched_param param;
    int policy;
    int retval = pthread_getschedparam(thread_id, &policy, &param);
    if (retval) {
        Log(Warn, "pthread_getschedparam(): %s!", strerror(retval));
        return false;
    }

    int min = sched_get_priority_min(policy);
    int max = sched_get_priority_max(policy);

    if (min == max) {
        Log(Warn, "Could not adjust the thread priority -- valid range is zero!");
        return false;
    }
    param.sched_priority = (int) (min + (max-min)*factor);

    retval = pthread_setschedparam(thread_id, policy, &param);
    if (retval) {
        Log(Warn, "Could not adjust the thread priority to %i: %s!",
            param.sched_priority, strerror(retval));
        return false;
    }
#elif defined(_WIN32)
    int win32_priority;
    switch (priority) {
        case EIdlePriority:     win32_priority = THREAD_PRIORITY_IDLE; break;
        case ELowestPriority:   win32_priority = THREAD_PRIORITY_LOWEST; break;
        case ELowPriority:      win32_priority = THREAD_PRIORITY_BELOW_NORMAL; break;
        case EHighPriority:     win32_priority = THREAD_PRIORITY_ABOVE_NORMAL; break;
        case EHighestPriority:  win32_priority = THREAD_PRIORITY_HIGHEST; break;
        case ERealtimePriority: win32_priority = THREAD_PRIORITY_TIME_CRITICAL; break;
        default:                win32_priority = THREAD_PRIORITY_NORMAL; break;
    }

    // If the function succeeds, the return value is nonzero
    const HANDLE handle = d->native_handle;
    if (SetThreadPriority(handle, win32_priority) == 0) {
        Log(Warn, "Could not adjust the thread priority to %i: %s!",
            win32_priority, util::last_error());
        return false;
    }
#endif
    return true;
}

void Thread::set_core_affinity(int core_id) {
    d->core_affinity = core_id;
    if (!d->running)
        return;

#if defined(__APPLE__)
    /* CPU affinity not supported on OSX */
#elif defined(__linux__)
    int core_count = sysconf(_SC_NPROCESSORS_CONF),
        logical_core_count = core_count;

    size_t size = 0;
    cpu_set_t *cpuset = nullptr;
    int retval = 0;

    /* The kernel may expect a larger cpu_set_t than would be warranted by the
       physical core count. Keep querying with increasingly larger buffers if
       the pthread_getaffinity_np operation fails */
    for (int i = 0; i<10; ++i) {
        size = CPU_ALLOC_SIZE(logical_core_count);
        cpuset = CPU_ALLOC(logical_core_count);
        if (!cpuset) {
            Log(Warn, "Thread::set_core_affinity(): could not allocate cpu_set_t");
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
            logical_core_count *= 2;
        } else {
            break;
        }
    }

    if (retval) {
        Log(Warn, "Thread::set_core_affinity(): pthread_getaffinity_np(): could "
            "not read thread affinity map: %s", strerror(retval));
        return;
    }

    int actual_core_id = -1, available = 0;
    for (int i=0; i<logical_core_count; ++i) {
        if (!CPU_ISSET_S(i, size, cpuset))
            continue;
        if (available++ == core_id) {
            actual_core_id = i;
            break;
        }
    }

    if (actual_core_id == -1) {
        Log(Warn, "Thread::set_core_affinity(): out of bounds: %i/%i cores "
                   "available, requested #%i!",
            available, core_count, core_id);
        CPU_FREE(cpuset);
        return;
    }

    CPU_ZERO_S(size, cpuset);
    CPU_SET_S(actual_core_id, size, cpuset);

    retval = pthread_setaffinity_np(d->native_handle, size, cpuset);
    if (retval) {
        Log(Warn,
            "Thread::set_core_affinity(): pthread_setaffinity_np: failed: %s",
            strerror(retval));
        CPU_FREE(cpuset);
        return;
    }

    CPU_FREE(cpuset);
#elif defined(_WIN32)
    int core_count = util::core_count();
    const HANDLE handle = d->native_handle;

    DWORD_PTR mask;

    if (core_id != -1 && core_id < core_count)
        mask = (DWORD_PTR) 1 << core_id;
    else
        mask = (1 << core_count) - 1;

    if (!SetThreadAffinityMask(handle, mask))
        Log(Warn, "Thread::set_core_affinity(): SetThreadAffinityMask : failed");
#endif
}

void Thread::start() {
    if (d->running)
        Log(Error, "Thread is already running!");
    if (!self)
        Log(Error, "Threading has not been initialized!");

    Log(Debug, "Spawning thread \"%s\"", d->name);

    d->parent = Thread::thread();

    /* Inherit the parent thread's logger if none was set */
    if (!d->logger)
        d->logger = d->parent->logger();

    /* Inherit the parent thread's file resolver if none was set */
    if (!d->fresolver)
        d->fresolver = d->parent->file_resolver();

    d->running = true;

    d->thread = std::thread(&Thread::dispatch, this);
}

void Thread::dispatch() {
    d->native_handle = d->thread.native_handle();

    uint32_t id = thread_ctr++;
    #if defined(__linux__) || defined(__APPLE__)
        pthread_setspecific(this_thread_id, reinterpret_cast<void *>(id));
    #elif defined(_WIN32)
        this_thread_id = id;
    #endif

    self = this;

    if (d->priority != ENormalPriority)
        set_priority(d->priority);

    if (!d->name.empty()) {
        const std::string thread_name = "Mitsuba: " + name();
        #if defined(__linux__)
            pthread_setname_np(pthread_self(), thread_name.c_str());
        #elif defined(__APPLE__)
            pthread_setname_np(thread_name.c_str());
        #elif defined(_WIN32)
            set_thread_name_(thread_name.c_str());
        #endif
    }

    if (d->core_affinity != -1)
        set_core_affinity(d->core_affinity);

    try {
        run();
    } catch (std::exception &e) {
        LogLevel warn_log_level = logger()->error_level() == Error ? Warn : Info;
        Log(warn_log_level, "Fatal error: uncaught exception: \"%s\"", e.what());
        if (d->critical)
            abort();
    }

    exit();
}

void Thread::exit() {
    Log(Debug, "Thread \"%s\" has finished", d->name);
    d->running = false;
    Assert(self.get() == this);
    self = nullptr;
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

std::string Thread::to_string() const {
    std::ostringstream oss;
    oss << "Thread[" << std::endl
        << "  name = \"" << d->name << "\"," << std::endl
        << "  running = " << d->running << "," << std::endl
        << "  priority = " << d->priority << "," << std::endl
        << "  critical = " << d->critical << std::endl
        << "]";
    return oss.str();
}

bool Thread::register_external_thread(const std::string &prefix) {
    uint32_t id = thread_ctr++;
    self = new WorkerThread(prefix);
    #if defined(__linux__) || defined(__APPLE__)
        self->d->native_handle = pthread_self();
        pthread_setspecific(this_thread_id, reinterpret_cast<void *>(id));
    #elif defined(_WIN32)
        self->d->native_handle = GetCurrentThread();
        this_thread_id = id;
    #endif
    self->d->running = true;
    self->d->external_thread = true;

    // An external thread will re-use the main thread's Logger (thread safe)
    // and create a new FileResolver (since the FileResolver is not thread safe).
    self->d->logger = main_thread->d->logger;
    self->d->fresolver = new FileResolver();

    const std::string &thread_name = self->name();
    #if defined(__linux__)
        pthread_setname_np(pthread_self(), thread_name.c_str());
    #elif defined(__APPLE__)
        pthread_setname_np(thread_name.c_str());
    #elif defined(_WIN32)
        set_thread_name_(thread_name.c_str());
    #endif

    return true;
}

bool Thread::unregister_external_thread() {
    if (!self || !self->d->external_thread)
        return false;
    self->d->running = false;
    return true;
}

void Thread::register_task(Task *task) {
    std::lock_guard guard(task_mutex);
    registered_tasks.push_back(task);
}

void Thread::wait_for_tasks() {
    std::vector<Task *> tasks;
    {
        std::lock_guard guard(task_mutex);
        tasks.swap(registered_tasks);
    }
    for (Task *task : tasks)
        task_wait_and_release(task);
}

void Thread::static_initialization() {
    #if defined(__linux__) || defined(__APPLE__)
        pthread_key_create(&this_thread_id, nullptr);
    #endif

    global_thread_count = util::core_count();

    self = new MainThread();
    self->d->running = true;
    self->d->fresolver = new FileResolver();
    main_thread = self;
}

void Thread::static_shutdown() {
    for (auto& task : registered_tasks)
        task_wait_and_release(task);
    registered_tasks.clear();

    /* Remove references to Loggers and FileResolvers from worker threads.
     * _Important_: This might locally acquire the Python GIL due to reference
     * counting. To prevent deadlocks, it's important that Thread::static_shutdown
     * is run with the GIL acquired already. This ensures consistent lock
     * acquisition order and prevents deadlocks. */
    {
        std::lock_guard guard(thread_registry_mutex);
        for (auto &thread : thread_registry) {
            thread.second->d->logger    = nullptr;
            thread.second->d->fresolver = nullptr;
        }
        thread_registry.clear();
    }
    main_thread->d->logger    = nullptr;
    main_thread->d->fresolver = nullptr;
    main_thread->d->running   = false;
    self = nullptr;
    main_thread = nullptr;
    #if defined(__linux__) || defined(__APPLE__)
        pthread_key_delete(this_thread_id);
    #endif
}

size_t Thread::thread_count() { return global_thread_count; }

void Thread::set_thread_count(size_t count) {
    global_thread_count = count;
    // Main thread counts as one thread
    pool_set_size(nullptr, (uint32_t) (count - 1));
}

ThreadEnvironment::ThreadEnvironment() {
    Thread *thread = Thread::thread();
    Assert(thread);
    m_logger = thread->logger();
    m_file_resolver = thread->file_resolver();
}

ScopedSetThreadEnvironment::ScopedSetThreadEnvironment(ThreadEnvironment &env) {
    Thread *thread = Thread::thread();
    Assert(thread);
    m_logger = thread->logger();
    m_file_resolver = thread->file_resolver();
    thread->set_logger(env.m_logger);
    thread->set_file_resolver(env.m_file_resolver);
}

ScopedSetThreadEnvironment::~ScopedSetThreadEnvironment() {
    Thread *thread = Thread::thread();
    thread->set_logger(m_logger);
    thread->set_file_resolver(m_file_resolver);
}

MI_IMPLEMENT_CLASS(Thread, Object)
MI_IMPLEMENT_CLASS(MainThread, Thread)
MI_IMPLEMENT_CLASS(WorkerThread, Thread)

NAMESPACE_END(mitsuba)
