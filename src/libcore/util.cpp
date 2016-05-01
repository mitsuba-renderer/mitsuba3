#include <mitsuba/core/util.h>
#include <mitsuba/core/logger.h>

#if defined(__OSX__)
#  include <sys/sysctl.h>
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
        Log(EError, "Could not detect the number of processors!");
    return (int)nprocs;
#else
    return sysconf(_SC_NPROCESSORS_CONF);
#endif
}

NAMESPACE_END(mitsuba)
