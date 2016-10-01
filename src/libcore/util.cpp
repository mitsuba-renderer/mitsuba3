#include <mitsuba/core/util.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/filesystem.h>
#include <cmath>

#if defined(__LINUX__)
#  if !defined(_GNU_SOURCE)
#    define _GNU_SOURCE
#  endif
#  include <dlfcn.h>
#  include <unistd.h>
#  include <limits.h>
#elif defined(__OSX__)
#  include <sys/sysctl.h>
#  include <mach-o/dyld.h>
#  include <unistd.h>
#elif defined(__WINDOWS__)
#  include <windows.h>
#endif

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(util)

#if defined(__WINDOWS__)
std::string lastError() {
    DWORD errCode = GetLastError();
    char *errorText = nullptr;
    if (!FormatMessageA(
          FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &errorText,
        0,
        nullptr)) {
        return "Internal error while looking up an error code";
    }
    std::string result(errorText);
    LocalFree(errorText);
    return result;
}
#endif

static int __cached_core_count = 0;

int coreCount() {
    // assumes atomic word size memory access
    if (__cached_core_count)
        return __cached_core_count;

#if defined(__WINDOWS__)
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    __cached_core_count = sys_info.dwNumberOfProcessors;
    return sys_info.dwNumberOfProcessors;
#elif defined(__OSX__)
    int nprocs;
    size_t nprocsSize = sizeof(int);
    if (sysctlbyname("hw.activecpu", &nprocs, &nprocsSize, NULL, 0))
        Throw("Could not detect the number of processors!");
    __cached_core_count = nprocs;
    return nprocs;
#else
    /* Determine the number of present cores */
    int nCores = sysconf(_SC_NPROCESSORS_CONF);

    /* Don't try to query CPU affinity if running inside Valgrind */
    if (getenv("VALGRIND_OPTS") == NULL) {
        /* Some of the cores may not be available to the user
           (e.g. on certain cluster nodes) -- determine the number
           of actual available cores here. */
        int nLogicalCores = nCores;
        size_t size = 0;
        cpu_set_t *cpuset = NULL;
        int retval = 0;

        /* The kernel may expect a larger cpu_set_t than would
           be warranted by the physical core count. Keep querying
           with increasingly larger buffers if the
           pthread_getaffinity_np operation fails */
        for (int i = 0; i<10; ++i) {
            size = CPU_ALLOC_SIZE(nLogicalCores);
            cpuset = CPU_ALLOC(nLogicalCores);
            if (!cpuset) {
                Throw("getCoreCount(): could not allocate cpu_set_t");
                goto done;
            }
            CPU_ZERO_S(size, cpuset);

            int retval = pthread_getaffinity_np(pthread_self(), size, cpuset);
            if (retval == 0)
                break;
            CPU_FREE(cpuset);
            nLogicalCores *= 2;
        }

        if (retval) {
            Throw("getCoreCount(): pthread_getaffinity_np(): could "
                "not read thread affinity map: %s", strerror(retval));
            goto done;
        }

        int availableCores = 0;
        for (int i=0; i<nLogicalCores; ++i)
            availableCores += CPU_ISSET_S(i, size, cpuset) ? 1 : 0;
        nCores = availableCores;
        CPU_FREE(cpuset);
    }

done:
    __cached_core_count = nCores;
    return nCores;
#endif
}

void trapDebugger() {
#if defined(__LINUX__)
    char exePath[PATH_MAX];
    memset(exePath, 0, PATH_MAX);
    std::string procPath = tfm::format("/proc/%i/exe", getppid());

    if (readlink(procPath.c_str(), exePath, PATH_MAX) != -1) {
        if (strstr(exePath, "bin/gdb") || strstr(exePath, "bin/lldb")) {
            #if defined(__i386__) || defined(__x86_64__)
                __asm__ ("int $3");
            #else
                __builtin_trap();
            #endif
        }
    }
#elif defined(__OSX__)
    int                 mib[4];
    struct kinfo_proc   info;
    size_t              size;
    info.kp_proc.p_flag = 0;
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();
    size = sizeof(info);
    sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, nullptr, 0);

    if (info.kp_proc.p_flag & P_TRACED)
        __asm__ ("int $3");
#elif defined(__WINDOWS__)
    if (IsDebuggerPresent())
        __debugbreak();
#endif
}

std::string timeString(Float value, bool precise) {
    struct Order { Float factor; const char* suffix; };
    const Order orders[] = {
        { 0,    "ms"}, { 1000, "s"},
        { 60,   "m"},  { 60,   "h"},
        { 24,   "d"},  { 7,    "w"},
        { (Float) 52.1429,     "y"}
    };

    if (std::isnan(value))
        return "nan";
    else if (std::isinf(value))
        return "inf";
    else if (value < 0)
        return "-" + timeString(-value, precise);

    int i = 0;
    for (i = 0; i < 6 && value > orders[i+1].factor; ++i)
        value /= orders[i+1].factor;

    return tfm::format(precise ? "%.5g%s" : "%.3g%s", value, orders[i].suffix);
}

std::string memString(size_t size, bool precise) {
    const char *orders[] = {
        "B", "KiB", "MiB", "GiB",
        "TiB", "PiB", "EiB"
    };
    Float value = (Float) size;

    int i = 0;
    for (i = 0; i < 6 && value > 1024.0f; ++i)
        value /= 1024.0f;

    return tfm::format(precise ? "%.5g %s" : "%.3g %s", value, orders[i]);
}

#if defined(__WINDOWS__) || defined(__LINUX__)
    void MTS_EXPORT __dummySymbol() { }
#endif

fs::path libraryPath() {
    fs::path result;
#if defined(__LINUX__)
    Dl_info info;
    if (dladdr((const void *) &__dummySymbol, &info) != 0)
        result = fs::path(info.dli_fname);
#elif defined(__OSX__)
    uint32_t imageCount = _dyld_image_count();
    for (uint32_t i=0; i<imageCount; ++i) {
        const char *imageName = _dyld_get_image_name(i);
        if (string::endsWith(imageName, "libmitsuba-core.dylib")) {
            result = fs::path(imageName);
            break;
        }
    }
#elif defined(__WINDOWS__)
    std::vector<WCHAR> lpFilename(MAX_PATH);

    // Module handle to this DLL. If the function fails it sets handle to nullptr.
    // In that case GetModuleFileName will get the name of the executable which
    // is acceptable soft-failure behavior.
    HMODULE handle;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                     | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCWSTR>(&__dummySymbol), &handle);

    // Try to get the path with the default MAX_PATH length (260 chars)
    DWORD nSize = GetModuleFileNameW(handle, &lpFilename[0], MAX_PATH);

    // Adjust the buffer size in case if was too short
    while (nSize != 0 && nSize == lpFilename.size()) {
        lpFilename.resize(nSize * 2);
        nSize = GetModuleFileNameW(handle, &lpFilename[0],
            static_cast<DWORD>(lpFilename.size()));
    }

    // There is an error if and only if the function returns 0
    if (nSize != 0)
        result = fs::path(lpFilename.data());
#endif
    if (result.empty())
        Throw("Could not detect the core library path!");
    return fs::absolute(result);
}

NAMESPACE_END(util)
NAMESPACE_END(mitsuba)
