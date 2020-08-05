#if defined(MTS_ENABLE_OPTIX)

#if defined(_WIN32)
// Exclude unnecessary headers from the subsequent includes
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#include <cfgmgr32.h>
// Automatically link the libary
#pragma comment( lib, "Cfgmgr32.lib" )
#else
#  include <dlfcn.h>
#endif

#include <mitsuba/core/logger.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/render/optix_api.h>
#include <mitsuba/render/optix/shapes.h>

#if !defined(MTS_USE_OPTIX_HEADERS)
// Driver API
const char* (*optixGetErrorName)(OptixResult) = nullptr;
const char* (*optixGetErrorString)(OptixResult) = nullptr;
OptixResult (*optixDeviceContextCreate)(CUcontext, const OptixDeviceContextOptions*, OptixDeviceContext*) = nullptr;
OptixResult (*optixDeviceContextDestroy)(OptixDeviceContext) = nullptr;
OptixResult (*optixModuleCreateFromPTX)(OptixDeviceContext, const OptixModuleCompileOptions*, const OptixPipelineCompileOptions*, const char*, size_t, char*, size_t*, OptixModule*) = nullptr;
OptixResult (*optixModuleDestroy)(OptixModule) = nullptr;
OptixResult (*optixProgramGroupCreate)(OptixDeviceContext, const OptixProgramGroupDesc*, unsigned int, const OptixProgramGroupOptions*, char*, size_t*, OptixProgramGroup*) = nullptr;
OptixResult (*optixProgramGroupDestroy)(OptixProgramGroup) = nullptr;
OptixResult (*optixPipelineCreate)(OptixDeviceContext, const OptixPipelineCompileOptions*, const OptixPipelineLinkOptions*, const OptixProgramGroup*, unsigned int, char*, size_t*, OptixPipeline*) = nullptr;
OptixResult (*optixPipelineDestroy)(OptixPipeline) = nullptr;
OptixResult (*optixAccelComputeMemoryUsage)(OptixDeviceContext, const OptixAccelBuildOptions*, const OptixBuildInput*, unsigned int, OptixAccelBufferSizes*) = nullptr;
OptixResult (*optixAccelBuild)(OptixDeviceContext, CUstream, const OptixAccelBuildOptions*, const OptixBuildInput*,unsigned int, CUdeviceptr, size_t, CUdeviceptr, size_t, OptixTraversableHandle*, const OptixAccelEmitDesc*, unsigned int) = nullptr;
OptixResult (*optixAccelCompact)(OptixDeviceContext, CUstream, OptixTraversableHandle, CUdeviceptr, size_t, OptixTraversableHandle*) = nullptr;
OptixResult (*optixSbtRecordPackHeader)(OptixProgramGroup, void*) = nullptr;
OptixResult (*optixLaunch)(OptixPipeline, CUstream, CUdeviceptr, size_t, const OptixShaderBindingTable*, unsigned int, unsigned int, unsigned int) = nullptr;
OptixResult (*optixQueryFunctionTable)(int, unsigned int, OptixQueryFunctionTableOptions*, const void**, void*, size_t) = nullptr;
#endif

#if !defined(MTS_USE_OPTIX_HEADERS)
static void *optix_handle = nullptr;
#endif

static bool optix_init_attempted = false;
static bool optix_init_success = false;

NAMESPACE_BEGIN(mitsuba)

bool optix_initialize() {
    if (optix_init_attempted)
        return optix_init_success;
    optix_init_attempted = true;

#if !defined(MTS_USE_OPTIX_HEADERS)
    Log(LogLevel::Info, "Dynamic loading of the Optix library ..");

    optix_handle = nullptr;

# if defined(_WIN32)
#    define dlsym(ptr, name) GetProcAddress((HMODULE) ptr, name)
    const char* optix_fname = "nvoptix.dll";
# elif defined(__linux__)
    const char *optix_fname  = "libnvoptix.so.1";
# else
    const char *optix_fname  = "libnvoptix.dylib";
# endif

#  if !defined(_WIN32)
    // Don't dlopen libnvoptix.so if it was loaded by another library
    if (dlsym(RTLD_NEXT, "optixLaunch"))
        optix_handle = RTLD_NEXT;

    if (!optix_handle)
        optix_handle = dlopen(optix_fname, RTLD_NOW);
#  else
    filesystem::path optix_dll_path;

    // First look for the DLL in the System32 folder
    char system_path[MAX_PATH];
    if (GetSystemDirectoryA(system_path, MAX_PATH) != 0)
        optix_dll_path = filesystem::path(system_path) / optix_fname;

    // If couldn't be found, look in the folders next to the opengl driver
    // For this, we need to query the registry to find the location of the OpenGL driver
    if (!exists(optix_dll_path)) {
        // Get list of display adapter devices
        auto diplay_adapter_guid = "{4d36e968-e325-11ce-bfc1-08002be10318}";
        auto flags = CM_GETIDLIST_FILTER_CLASS | CM_GETIDLIST_FILTER_PRESENT;
        unsigned long device_list_size = 0;
        CM_Get_Device_ID_List_SizeA(&device_list_size, diplay_adapter_guid, flags);
        char* device_names = new char[device_list_size];
        CM_Get_Device_ID_ListA(diplay_adapter_guid, device_names, device_list_size, flags);

        // Iterate over the display adapter devices until we find the DLL
        size_t offset = 0;
        while (offset < device_list_size - 1 && !exists(optix_dll_path)) {
            char* device_name = device_names + offset;
            offset += strlen(device_name) + 1;

            // Query device instance handle
            unsigned long dev_handle = 0;
            if (CM_Locate_DevNodeA(&dev_handle, device_name, CM_LOCATE_DEVNODE_NORMAL))
                continue;

            // Open registery key
            HKEY reg_key = 0;
            if (CM_Open_DevNode_Key(dev_handle, KEY_QUERY_VALUE, 0,
                                    RegDisposition_OpenExisting, &reg_key,
                                    CM_REGISTRY_SOFTWARE))
                continue;

            // Retrieve OpenGL diver path from the registry
            const char* value_name = "OpenGLDriverName";
            unsigned long value_size = 0;
            if (RegQueryValueExA(reg_key, value_name, 0, 0, 0, &value_size) != ERROR_SUCCESS) {
                RegCloseKey(reg_key);
                continue;
            }
            char* reg_dll_path = new char[value_size];
            if (RegQueryValueExA(reg_key, value_name, 0, 0,
                                 (LPBYTE) reg_dll_path,
                                 &value_size) != ERROR_SUCCESS) {
                RegCloseKey(reg_key);
                free(reg_dll_path);
                continue;
            }

            // Replace the opengl DLL name with the optix DLL name
            optix_dll_path = filesystem::path(reg_dll_path).parent_path() / optix_fname;

            RegCloseKey(reg_key);
            free(reg_dll_path);
        }
        free(device_names);
    }

    if (exists(optix_dll_path))
        optix_handle = LoadLibraryA((LPCSTR)optix_dll_path.string().c_str());
#  endif

    if (!optix_handle) {
        Log(LogLevel::Error,
            "optix_initialize(): %s could not be loaded.", optix_fname);
        return false;
    }

    // Load optixQueryFunctionTable from library
    optixQueryFunctionTable = decltype(optixQueryFunctionTable)(
        dlsym(optix_handle, "optixQueryFunctionTable"));

    if (!optixQueryFunctionTable) {
        Log(LogLevel::Error,
            "optix_initialize(): could not find symbol optixQueryFunctionTable");
        return false;
    }

    void *function_table[36];
    optixQueryFunctionTable(OPTIX_ABI_VERSION, 0, 0, 0, &function_table, sizeof(function_table));

    #define LOAD(name, index) name = (decltype(name)) function_table[index];

    LOAD(optixGetErrorName, 0);
    LOAD(optixGetErrorString, 1);
    LOAD(optixDeviceContextCreate, 2);
    LOAD(optixDeviceContextDestroy, 3);
    LOAD(optixModuleCreateFromPTX, 12);
    LOAD(optixModuleDestroy, 13);
    LOAD(optixProgramGroupCreate, 14);
    LOAD(optixProgramGroupDestroy, 15);
    LOAD(optixPipelineCreate, 17);
    LOAD(optixPipelineDestroy, 18);
    LOAD(optixAccelComputeMemoryUsage, 20);
    LOAD(optixAccelBuild, 21);
    LOAD(optixAccelCompact, 25);
    LOAD(optixSbtRecordPackHeader, 27);
    LOAD(optixLaunch, 28);
#else
    rt_check(optixInit());
#endif
    optix_init_success = true;
    return true;
}

void optix_shutdown() {
#if !defined(MTS_USE_OPTIX_HEADERS)
    if (!optix_init_success)
        return;

    #define Z(x) x = nullptr
    Z(optixGetErrorName); Z(optixGetErrorString); Z(optixDeviceContextCreate);
    Z(optixDeviceContextDestroy); Z(optixModuleCreateFromPTX); Z(optixModuleDestroy);
    Z(optixProgramGroupCreate); Z(optixProgramGroupDestroy);
    Z(optixPipelineCreate); Z(optixPipelineDestroy); Z(optixAccelComputeMemoryUsage);
    Z(optixAccelBuild); Z(optixAccelCompact); Z(optixSbtRecordPackHeader);
    Z(optixLaunch); Z(optixQueryFunctionTable);
    #undef Z

#if !defined(_WIN32)
    if (optix_handle != RTLD_NEXT)
        dlclose(optix_handle);
#else
    FreeLibrary((HMODULE) optix_handle);
#endif

    optix_handle = nullptr;
#endif

    optix_init_success = false;
    optix_init_attempted = false;
}

void __rt_check(OptixResult errval, const char *file, const int line) {
    if (errval != OPTIX_SUCCESS) {
        const char *message = optixGetErrorString(errval);
        if (errval == 1546)
            message = "Failed to load OptiX library! Very likely, your NVIDIA graphics "
                "driver is too old and not compatible with the version of OptiX that is "
                "being used. In particular, OptiX 6.5 requires driver revision R435.80 or newer.";
        fprintf(stderr, "rt_check(): OptiX API error = %04d (%s) in %s:%i.\n",
                (int) errval, message, file, line);
        exit(EXIT_FAILURE);
    }
}

void __rt_check_log(OptixResult errval, const char *file, const int line) {
    if (errval != OPTIX_SUCCESS) {
        const char *message = optixGetErrorString(errval);
        fprintf(stderr, "rt_check(): OptiX API error = %04d (%s) in %s:%i.\n",
                (int) errval, message, file, line);
        fprintf(stderr, "\tLog: %s%s", optix_log_buffer,
                optix_log_buffer_size > sizeof(optix_log_buffer) ? "<TRUNCATED>" : "");
        exit(EXIT_FAILURE);
    }
}

NAMESPACE_END(mitsuba)

#endif // defined(MTS_ENABLE_OPTIX)
