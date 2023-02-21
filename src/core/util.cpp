#include <drjit/packet.h>

#include <mitsuba/core/util.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/vector.h>

#if defined(__linux__)
#  if !defined(_GNU_SOURCE)
#    define _GNU_SOURCE
#  endif
#  include <dlfcn.h>
#  include <unistd.h>
#  include <limits.h>
#  include <sys/ioctl.h>
#elif defined(__APPLE__)
#  include <sys/sysctl.h>
#  include <mach-o/dyld.h>
#  include <unistd.h>
#  include <sys/ioctl.h>
#elif defined(_WIN32)
#  include <windows.h>
#endif

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(util)

#if defined(_WIN32)
std::string last_error() {
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

int core_count() {
    // assumes atomic word size memory access
    if (__cached_core_count)
        return __cached_core_count;

#if defined(_WIN32)
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    __cached_core_count = sys_info.dwNumberOfProcessors;
    return sys_info.dwNumberOfProcessors;
#elif defined(__APPLE__)
    int nprocs;
    size_t nprocsSize = sizeof(int);
    if (sysctlbyname("hw.activecpu", &nprocs, &nprocsSize, NULL, 0))
        Throw("Could not detect the number of processors!");
    __cached_core_count = nprocs;
    return nprocs;
#else
    /* Determine the number of present cores */
    int ncores = sysconf(_SC_NPROCESSORS_CONF);

    /* Don't try to query CPU affinity if running inside Valgrind */
    if (getenv("VALGRIND_OPTS") == NULL) {
        /* Some of the cores may not be available to the user
           (e.g. on certain cluster nodes) -- determine the number
           of actual available cores here. */
        int ncores_logical = ncores;
        size_t size = 0;
        cpu_set_t *cpuset = NULL;
        int retval = 0;

        /* The kernel may expect a larger cpu_set_t than would
           be warranted by the physical core count. Keep querying
           with increasingly larger buffers if the
           pthread_getaffinity_np operation fails */
        for (int i = 0; i<10; ++i) {
            size = CPU_ALLOC_SIZE(ncores_logical);
            cpuset = CPU_ALLOC(ncores_logical);
            if (!cpuset) {
                Throw("core_count(): could not allocate cpu_set_t");
                goto done;
            }
            CPU_ZERO_S(size, cpuset);

            int retval = pthread_getaffinity_np(pthread_self(), size, cpuset);
            if (retval == 0)
                break;
            CPU_FREE(cpuset);
            ncores_logical *= 2;
        }

        if (retval) {
            Throw("core_count(): pthread_getaffinity_np(): could "
                  "not read thread affinity map: %s", strerror(retval));
            goto done;
        }

        int ncores_avail = 0;
        for (int i=0; i<ncores_logical; ++i)
            ncores_avail += CPU_ISSET_S(i, size, cpuset) ? 1 : 0;
        ncores = ncores_avail;
        CPU_FREE(cpuset);
    }

done:
    __cached_core_count = ncores;
    return ncores;
#endif
}

bool detect_debugger() {
#if defined(__linux__)
    char exePath[PATH_MAX];
    memset(exePath, 0, PATH_MAX);
    std::string procPath = tfm::format("/proc/%i/exe", getppid());

    if (readlink(procPath.c_str(), exePath, PATH_MAX) != -1) {
        if (strstr(exePath, "bin/gdb") || strstr(exePath, "bin/lldb"))
            return true;
    }
#elif defined(__APPLE__)
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
        return true;
#elif defined(_WIN32)
    if (IsDebuggerPresent())
        return true;
#endif
    return false;
}

void trap_debugger() {
    if (!detect_debugger())
        return;

#if defined(__linux__) || defined(__APPLE__)
    __builtin_trap();
#elif defined(_WIN32)
    __debugbreak();
#endif
}

std::string time_string(float value, bool precise) {
    struct Order { float factor; const char* suffix; };
    const Order orders[] = {
        { 0,    "ms"}, { 1000, "s"},
        { 60,   "m"},  { 60,   "h"},
        { 24,   "d"},  { 7,    "w"},
        { 52.1429f, "y"}
    };

    if (dr::isnan(value))
        return "nan";
    else if (dr::isinf(value))
        return "inf";
    else if (value < 0)
        return "-" + time_string(-value, precise);

    int i = 0;
    for (i = 0; i < 6 && value > orders[i+1].factor; ++i)
        value /= orders[i+1].factor;

    return tfm::format(precise ? "%.5g%s" : "%.3g%s", value, orders[i].suffix);
}

std::string mem_string(size_t size, bool precise) {
    const char *orders[] = {
        "B", "KiB", "MiB", "GiB",
        "TiB", "PiB", "EiB"
    };
    float value = (float) size;

    int i = 0;
    for (i = 0; i < 6 && value > 1024.f; ++i)
        value /= 1024.f;

    return tfm::format(precise ? "%.5g %s" : "%.3g %s", value, orders[i]);
}

#if defined(_WIN32) || defined(__linux__)
    void MI_EXPORT __dummySymbol() { }
#endif

fs::path library_path() {
    fs::path result;
#if defined(__linux__)
    Dl_info info;
    if (dladdr((const void *) &__dummySymbol, &info) != 0)
        result = fs::path(info.dli_fname);
#elif defined(__APPLE__)
    uint32_t imageCount = _dyld_image_count();
    for (uint32_t i=0; i<imageCount; ++i) {
        const char *imageName = _dyld_get_image_name(i);
        if (string::ends_with(imageName, "libmitsuba.dylib")) {
            result = fs::path(imageName);
            break;
        }
    }
#elif defined(_WIN32)
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
        Throw("Could not detect the mitsuba library path!");
    return fs::absolute(result);
}

int terminal_width() {
    static int cached_width = -1;

    if (cached_width == -1) {
#if defined(_WIN32)
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h != INVALID_HANDLE_VALUE && h != nullptr) {
            CONSOLE_SCREEN_BUFFER_INFO bufferInfo = {0};
            GetConsoleScreenBufferInfo(h, &bufferInfo);
            cached_width = bufferInfo.dwSize.X - 1;
        }
#else
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) >= 0)
            cached_width = w.ws_col;
#endif
        if (cached_width == -1)
            cached_width = 80;
    }

    return cached_width;
}

std::string info_build(int thread_count) {
    constexpr size_t PacketSize = dr::Packet<float>::Size;

    std::ostringstream oss;
    oss << "Mitsuba version " << MI_VERSION << " (";
    oss << MI_BRANCH << "[" << MI_HASH << "], ";
#if defined(_WIN32)
    oss << "Windows, ";
#elif defined(__linux__)
    oss << "Linux, ";
#elif defined(__APPLE__)
    oss << "Mac OS, ";
#else
    oss << "Unknown, ";
#endif
    oss << (sizeof(size_t) * 8) << "bit, ";
    oss << thread_count << " thread" << (thread_count > 1 ? "s" : "");
    oss << ", " << PacketSize << "-wide SIMD";
    oss << ")";

    return oss.str();
}

std::string info_copyright() {
    std::ostringstream oss;
    oss << "Copyright " << MI_YEAR << ", " << MI_AUTHORS;
    return oss.str();
}

std::string info_features() {
    std::ostringstream oss;

    oss << "Enabled processor features:";

#if defined(MI_ENABLE_CUDA)
    oss << " cuda";
#endif

#if defined(MI_ENABLE_LLVM)
    oss << " llvm";
#endif

    if (has_avx512)          oss << " avx512";
    if (has_avx2)            oss << " avx2";
    if (has_avx)             oss << " avx";
    if (has_fma)             oss << " fma";
    if (has_f16c)            oss << " f16c";
    if (has_sse42)           oss << " sse4.2";
    if (has_x86_64)          oss << " x86_64";
    if (has_x86_32)          oss << " x86";
    if (has_neon)            oss << " neon";
    if (has_arm_32)          oss << " arm";
    if (has_arm_64)          oss << " aarch64";

    return oss.str();
}

NAMESPACE_END(util)
NAMESPACE_END(mitsuba)
