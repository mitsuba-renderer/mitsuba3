#pragma once

#include <iomanip>
#include <mitsuba/core/platform.h>

#if defined(MTS_USE_OPTIX_HEADERS)
# include <optix.h>
# include <optix_stubs.h>
#else
# define OPTIX_ABI_VERSION 22
# define OPTIX_SBT_RECORD_ALIGNMENT 16ull
# define OPTIX_SBT_RECORD_HEADER_SIZE ( (size_t)32 )
# define OPTIX_SUCCESS 0
# define OPTIX_ERROR_HOST_OUT_OF_MEMORY 7002
# define OPTIX_COMPILE_DEFAULT_MAX_REGISTER_COUNT 0
# define OPTIX_COMPILE_OPTIMIZATION_LEVEL_0 0
# define OPTIX_COMPILE_OPTIMIZATION_DEFAULT 3
# define OPTIX_COMPILE_DEBUG_LEVEL_LINEINFO 1
# define OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS 1u
# define OPTIX_EXCEPTION_FLAG_NONE 0u
# define OPTIX_EXCEPTION_FLAG_STACK_OVERFLOW 1u
# define OPTIX_EXCEPTION_FLAG_TRACE_DEPTH 2u
# define OPTIX_EXCEPTION_FLAG_USER 4u
# define OPTIX_EXCEPTION_FLAG_DEBUG 8u
# define OPTIX_PROGRAM_GROUP_KIND_RAYGEN 0x2421
# define OPTIX_PROGRAM_GROUP_KIND_MISS 0x2422
# define OPTIX_PROGRAM_GROUP_KIND_EXCEPTION 0x2423
# define OPTIX_PROGRAM_GROUP_KIND_HITGROUP 0x2424
# define OPTIX_COMPILE_DEBUG_LEVEL_NONE 0
# define OPTIX_COMPILE_DEBUG_LEVEL_LINEINFO 1
# define OPTIX_COMPILE_DEBUG_LEVEL_FULL 2
# define OPTIX_BUILD_FLAG_ALLOW_COMPACTION 2u
# define OPTIX_BUILD_OPERATION_BUILD 0x2161
# define OPTIX_BUILD_OPERATION_UPDATE 0x2162
# define OPTIX_PROPERTY_TYPE_COMPACTED_SIZE 0x2181
# define OPTIX_GEOMETRY_FLAG_NONE 0
# define OPTIX_BUILD_INPUT_TYPE_TRIANGLES 0x2141
# define OPTIX_VERTEX_FORMAT_FLOAT3 0x2121
# define OPTIX_INDICES_FORMAT_UNSIGNED_INT3 0x2103

using CUcontext = struct CUctx_st *;
using CUstream  = struct CUstream_st *;
using CUdeviceptr = void*;
using OptixResult = int;
using OptixDeviceContext = struct OptixDeviceContext_t*;
using OptixPipeline      = struct OptixPipeline_t*;
using OptixModule        = struct OptixModule_t*;
using OptixProgramGroup  = struct OptixProgramGroup_t*;
using OptixQueryFunctionTableOptions = void*;
using OptixLogCallback = void (*)(unsigned int, const char*, const char*, void*);

struct OptixDeviceContextOptions {
    OptixLogCallback logCallbackFunction;
    void* logCallbackData;
    int logCallbackLevel;
};

struct OptixPipelineLinkOptions {
    unsigned int maxTraceDepth;
    int debugLevel;
    int overrideUsesMotionBlur;
};

struct OptixPipelineCompileOptions {
    int usesMotionBlur;
    unsigned int traversableGraphFlags;
    int numPayloadValues;
    int numAttributeValues;
    unsigned int exceptionFlags;
    const char* pipelineLaunchParamsVariableName;
};

struct OptixModuleCompileOptions {
    int maxRegisterCount;
    int optLevel;
    int debugLevel;
};

struct OptixProgramGroupOptions {
    int placeholder;
};

struct OptixProgramGroupSingleModule {
    OptixModule module;
    const char* entryFunctionName;
};

struct OptixProgramGroupCallables {
    OptixModule moduleDC;
    const char* entryFunctionNameDC;
    OptixModule moduleCC;
    const char* entryFunctionNameCC;
};

struct OptixProgramGroupHitgroup {
    OptixModule moduleCH;
    const char* entryFunctionNameCH;
    OptixModule moduleAH;
    const char* entryFunctionNameAH;
    OptixModule moduleIS;
    const char* entryFunctionNameIS;
};

struct OptixProgramGroupDesc {
    int kind;
    unsigned int flags;
    union {
        OptixProgramGroupSingleModule raygen;
        OptixProgramGroupSingleModule miss;
        OptixProgramGroupSingleModule exception;
        OptixProgramGroupCallables callables;
        OptixProgramGroupHitgroup hitgroup;
    };
};

struct OptixShaderBindingTable {
    CUdeviceptr raygenRecord;
    CUdeviceptr exceptionRecord;
    CUdeviceptr  missRecordBase;
    unsigned int missRecordStrideInBytes;
    unsigned int missRecordCount;
    CUdeviceptr  hitgroupRecordBase;
    unsigned int hitgroupRecordStrideInBytes;
    unsigned int hitgroupRecordCount;
    CUdeviceptr  callablesRecordBase;
    unsigned int callablesRecordStrideInBytes;
    unsigned int callablesRecordCount;
};

struct OptixMotionOptions {
    unsigned short numKeys;
    unsigned short flags;
    float timeBegin;
    float timeEnd;
};

struct OptixAccelBuildOptions {
    unsigned int buildFlags;
    int operation;
    OptixMotionOptions motionOptions;
};

struct OptixAccelBufferSizes {
    size_t outputSizeInBytes;
    size_t tempSizeInBytes;
    size_t tempUpdateSizeInBytes;
};

struct OptixAccelEmitDesc {
    CUdeviceptr result;
    int type;
};

struct OptixBuildInputTriangleArray {
    const CUdeviceptr* vertexBuffers;
    unsigned int numVertices;
    int vertexFormat;
    unsigned int vertexStrideInBytes;
    CUdeviceptr indexBuffer;
    unsigned int numIndexTriplets;
    int indexFormat;
    unsigned int indexStrideInBytes;
    CUdeviceptr preTransform;
    const unsigned int* flags;
    unsigned int numSbtRecords;
    CUdeviceptr sbtIndexOffsetBuffer;
    unsigned int sbtIndexOffsetSizeInBytes;
    unsigned int sbtIndexOffsetStrideInBytes;
    unsigned int primitiveIndexOffset;
};

struct OptixBuildInputCustomPrimitiveArray {
    const CUdeviceptr* aabbBuffers;
    unsigned int numPrimitives;
    unsigned int strideInBytes;
    const unsigned int* flags;
    unsigned int numSbtRecords;
    CUdeviceptr sbtIndexOffsetBuffer;
    unsigned int sbtIndexOffsetSizeInBytes;
    unsigned int sbtIndexOffsetStrideInBytes;
    unsigned int primitiveIndexOffset;
};

struct OptixBuildInputInstanceArray {
    CUdeviceptr instances;
    unsigned int numInstances;
    CUdeviceptr aabbs;
    unsigned int numAabbs;
};

struct OptixBuildInput {
    int type;
    union {
        OptixBuildInputTriangleArray triangleArray;
        OptixBuildInputCustomPrimitiveArray aabbArray;
        OptixBuildInputInstanceArray instanceArray;
        char pad[1024];
    };
};

using OptixTraversableHandle = unsigned long long;

// Driver API
extern const char* (*optixGetErrorName)(OptixResult);
extern const char* (*optixGetErrorString)(OptixResult);
extern OptixResult (*optixDeviceContextCreate)(CUcontext, const OptixDeviceContextOptions*, OptixDeviceContext*);
extern OptixResult (*optixDeviceContextDestroy)(OptixDeviceContext);
extern OptixResult (*optixModuleCreateFromPTX)(OptixDeviceContext, const OptixModuleCompileOptions*, const OptixPipelineCompileOptions*, const char*, size_t, char*, size_t*, OptixModule*);
extern OptixResult (*optixModuleDestroy)(OptixModule);
extern OptixResult (*optixProgramGroupCreate)(OptixDeviceContext, const OptixProgramGroupDesc*, unsigned int, const OptixProgramGroupOptions*, char*, size_t*, OptixProgramGroup*);
extern OptixResult (*optixProgramGroupDestroy)(OptixProgramGroup);
extern OptixResult (*optixPipelineCreate)(OptixDeviceContext, const OptixPipelineCompileOptions*, const OptixPipelineLinkOptions*, const OptixProgramGroup*, unsigned int, char*, size_t*, OptixPipeline*);
extern OptixResult (*optixPipelineDestroy)(OptixPipeline);
extern OptixResult (*optixAccelComputeMemoryUsage)(OptixDeviceContext, const OptixAccelBuildOptions*, const OptixBuildInput*, unsigned int, OptixAccelBufferSizes*);
extern OptixResult (*optixAccelBuild)(OptixDeviceContext, CUstream, const OptixAccelBuildOptions*, const OptixBuildInput*,unsigned int, CUdeviceptr, size_t, CUdeviceptr, size_t, OptixTraversableHandle*, const OptixAccelEmitDesc*, unsigned int);
extern OptixResult (*optixAccelCompact)(OptixDeviceContext, CUstream,  OptixTraversableHandle, CUdeviceptr, size_t, OptixTraversableHandle*);
extern OptixResult (*optixSbtRecordPackHeader)(OptixProgramGroup, void*);
extern OptixResult (*optixLaunch)(OptixPipeline, CUstream, CUdeviceptr, size_t, const OptixShaderBindingTable*, unsigned int, unsigned int, unsigned int);
extern OptixResult (*optixQueryFunctionTable)(int, unsigned int, OptixQueryFunctionTableOptions*, const void**, void*, size_t);
#endif

NAMESPACE_BEGIN(mitsuba)
/// Try to load the OptiX library
extern bool optix_init();
extern void optix_shutdown();

static size_t optix_log_buffer_size;
static char optix_log_buffer[2024];

#define rt_check(err)     __rt_check(err, __FILE__, __LINE__)
#define rt_check_log(err) __rt_check_log(err, __FILE__, __LINE__)

void __rt_check(OptixResult errval, const char *file, const int line);
void __rt_check_log(OptixResult errval, const char *file, const int line);

NAMESPACE_END(mitsuba)