#pragma once

#if defined(MI_ENABLE_CUDA)

#include <iomanip>
#include <mitsuba/core/platform.h>

// =====================================================
//       Various opaque handles and enumerations
// =====================================================

using CUdeviceptr            = void*;
using CUstream               = void*;
using OptixPipeline          = void *;
using OptixModule            = void *;
using OptixProgramGroup      = void *;
using OptixResult            = int;
using OptixTraversableHandle = unsigned long long;
using OptixBuildOperation    = int;
using OptixBuildInputType    = int;
using OptixVertexFormat      = int;
using OptixIndicesFormat     = int;
using OptixTransformFormat   = int;
using OptixAccelPropertyType = int;
using OptixProgramGroupKind  = int;
using OptixDeviceContext     = void*;
using OptixTask              = void*;
using OptixDenoiserStructPtr = void*;

// =====================================================
//            Commonly used OptiX constants
// =====================================================

#define OPTIX_BUILD_INPUT_TYPE_TRIANGLES         0x2141
#define OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES 0x2142
#define OPTIX_BUILD_INPUT_TYPE_INSTANCES         0x2143
#define OPTIX_BUILD_OPERATION_BUILD              0x2161

#define OPTIX_GEOMETRY_FLAG_NONE           0
#define OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT 1

#define OPTIX_INDICES_FORMAT_UNSIGNED_INT3 0x2103
#define OPTIX_VERTEX_FORMAT_FLOAT3         0x2121
#define OPTIX_SBT_RECORD_ALIGNMENT         16ull
#define OPTIX_SBT_RECORD_HEADER_SIZE       32

#define OPTIX_COMPILE_DEFAULT_MAX_REGISTER_COUNT 0
#define OPTIX_COMPILE_OPTIMIZATION_DEFAULT       0
#define OPTIX_COMPILE_OPTIMIZATION_LEVEL_0       0x2340
#define OPTIX_COMPILE_DEBUG_LEVEL_NONE           0x2350
#define OPTIX_COMPILE_DEBUG_LEVEL_MINIMAL        0x2351
#define OPTIX_COMPILE_DEBUG_LEVEL_MODERATE       0x2353
#define OPTIX_COMPILE_DEBUG_LEVEL_FULL           0x2352

#define OPTIX_BUILD_FLAG_ALLOW_COMPACTION  2
#define OPTIX_BUILD_FLAG_PREFER_FAST_TRACE 4
#define OPTIX_PROPERTY_TYPE_COMPACTED_SIZE 0x2181

#define OPTIX_EXCEPTION_FLAG_NONE           0
#define OPTIX_EXCEPTION_FLAG_STACK_OVERFLOW 1
#define OPTIX_EXCEPTION_FLAG_TRACE_DEPTH    2
#define OPTIX_EXCEPTION_FLAG_USER           4
#define OPTIX_EXCEPTION_FLAG_DEBUG          8

#define OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_ANY 0
#define OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS 1
#define OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING (1u << 1)

#define OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM   (1 << 0)
#define OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE (1 << 31)

#define OPTIX_PROGRAM_GROUP_KIND_MISS      0x2422
#define OPTIX_PROGRAM_GROUP_KIND_HITGROUP  0x2424

#define OPTIX_INSTANCE_FLAG_NONE              0
#define OPTIX_INSTANCE_FLAG_DISABLE_TRANSFORM (1u << 6)

#define OPTIX_RAY_FLAG_NONE                   0
#define OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT (1u << 2)
#define OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT     (1u << 3)

#define OPTIX_MODULE_COMPILE_STATE_COMPLETED 0x2364

// =====================================================
//          Commonly used OptiX data structures
// =====================================================

struct OptixMotionOptions {
    unsigned short numKeys;
    unsigned short flags;
    float timeBegin;
    float timeEnd;
};

struct OptixAccelBuildOptions {
    unsigned int buildFlags;
    OptixBuildOperation operation;
    OptixMotionOptions motionOptions;
};

struct OptixAccelBufferSizes {
    size_t outputSizeInBytes;
    size_t tempSizeInBytes;
    size_t tempUpdateSizeInBytes;
};

struct OptixBuildInputTriangleArray {
    const CUdeviceptr* vertexBuffers;
    unsigned int numVertices;
    OptixVertexFormat vertexFormat;
    unsigned int vertexStrideInBytes;
    CUdeviceptr indexBuffer;
    unsigned int numIndexTriplets;
    OptixIndicesFormat indexFormat;
    unsigned int indexStrideInBytes;
    CUdeviceptr preTransform;
    const unsigned int* flags;
    unsigned int numSbtRecords;
    CUdeviceptr sbtIndexOffsetBuffer;
    unsigned int sbtIndexOffsetSizeInBytes;
    unsigned int sbtIndexOffsetStrideInBytes;
    unsigned int primitiveIndexOffset;
    OptixTransformFormat transformFormat;
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
};

struct OptixBuildInput {
    OptixBuildInputType type;
    union {
        OptixBuildInputTriangleArray triangleArray;
        OptixBuildInputCustomPrimitiveArray customPrimitiveArray;
        OptixBuildInputInstanceArray instanceArray;
        char pad[1024];
    };
};

struct OptixInstance {
    float transform[12];
    unsigned int instanceId;
    unsigned int sbtOffset;
    unsigned int visibilityMask;
    unsigned int flags;
    OptixTraversableHandle traversableHandle;
    unsigned int pad[2];
};

struct OptixPayloadType {
    unsigned int numPayloadValues;
    const unsigned int *payloadSemantics;
};

struct OptixModuleCompileOptions {
    int maxRegisterCount;
    int optLevel;
    int debugLevel;
    const void *boundValues;
    unsigned int numBoundValues;
    unsigned int numPayloadTypes;
    OptixPayloadType *payloadTypes;
};

struct OptixPipelineCompileOptions {
    int usesMotionBlur;
    unsigned int traversableGraphFlags;
    int numPayloadValues;
    int numAttributeValues;
    unsigned int exceptionFlags;
    const char* pipelineLaunchParamsVariableName;
    unsigned int usesPrimitiveTypeFlags;
};

struct OptixAccelEmitDesc {
    CUdeviceptr result;
    OptixAccelPropertyType type;
};

struct OptixProgramGroupSingleModule {
    OptixModule module;
    const char* entryFunctionName;
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
    OptixProgramGroupKind kind;
    unsigned int flags;
    union {
        OptixProgramGroupSingleModule raygen;
        OptixProgramGroupSingleModule miss;
        OptixProgramGroupSingleModule exception;
        OptixProgramGroupHitgroup hitgroup;
    };
};

struct OptixProgramGroupOptions {
    OptixPayloadType *payloadType;
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

enum OptixPixelFormat {
    OPTIX_PIXEL_FORMAT_HALF2  = 0x2207,
    OPTIX_PIXEL_FORMAT_HALF3  = 0x2201,
    OPTIX_PIXEL_FORMAT_HALF4  = 0x2202,
    OPTIX_PIXEL_FORMAT_FLOAT2 = 0x2208,
    OPTIX_PIXEL_FORMAT_FLOAT3 = 0x2203,
    OPTIX_PIXEL_FORMAT_FLOAT4 = 0x2204,
    OPTIX_PIXEL_FORMAT_UCHAR3 = 0x2205,
    OPTIX_PIXEL_FORMAT_UCHAR4 = 0x2206
};

struct OptixImage2D {
    CUdeviceptr data;
    unsigned int width;
    unsigned int height;
    unsigned int rowStrideInBytes;
    unsigned int pixelStrideInBytes;
    OptixPixelFormat format;
};

enum OptixDenoiserModelKind {
    OPTIX_DENOISER_MODEL_KIND_HDR = 0x2323,
    OPTIX_DENOISER_MODEL_KIND_TEMPORAL = 0x2325
};

struct OptixDenoiserOptions {
    unsigned int guideAlbedo;
    unsigned int guideNormal;
};

struct OptixDenoiserSizes {
    size_t stateSizeInBytes;
    size_t withOverlapScratchSizeInBytes;
    size_t withoutOverlapScratchSizeInBytes;
    unsigned int overlapWindowSizeInPixels;
};

struct OptixDenoiserParams {
    unsigned int denoiseAlpha;
    CUdeviceptr hdrIntensity;
    float blendFactor;
    CUdeviceptr hdrAverageColor;
};

struct OptixDenoiserGuideLayer {
    OptixImage2D albedo;
    OptixImage2D normal;
    OptixImage2D flow;
};

struct OptixDenoiserLayer {
    OptixImage2D input;
    OptixImage2D previousOutput;
    OptixImage2D output;
};

// =====================================================
//             Commonly used OptiX functions
// =====================================================

#if defined(OPTIX_API_IMPL)
#  define D(name, ...) OptixResult (*name)(__VA_ARGS__) = nullptr;
#else
#  define D(name, ...) extern MI_EXPORT_LIB OptixResult (*name)(__VA_ARGS__)
#endif

D(optixAccelComputeMemoryUsage, OptixDeviceContext,
  const OptixAccelBuildOptions *, const OptixBuildInput *, unsigned int,
  OptixAccelBufferSizes *);
D(optixAccelBuild, OptixDeviceContext, CUstream, const OptixAccelBuildOptions *,
  const OptixBuildInput *, unsigned int, CUdeviceptr, size_t, CUdeviceptr,
  size_t, OptixTraversableHandle *, const OptixAccelEmitDesc *, unsigned int);
D(optixModuleCreateFromPTXWithTasks, OptixDeviceContext,
  const OptixModuleCompileOptions *, const OptixPipelineCompileOptions *,
  const char *, size_t, char *, size_t *, OptixModule *, OptixTask *);
D(optixModuleGetCompilationState, OptixModule, int *);
D(optixTaskExecute, OptixTask, OptixTask *, unsigned int, unsigned int *);
D(optixProgramGroupCreate, OptixDeviceContext, const OptixProgramGroupDesc *,
  unsigned int, const OptixProgramGroupOptions *, char *, size_t *,
  OptixProgramGroup *);
D(optixSbtRecordPackHeader, OptixProgramGroup, void *);
D(optixAccelCompact, OptixDeviceContext, CUstream, OptixTraversableHandle,
  CUdeviceptr, size_t, OptixTraversableHandle *);
D(optixDenoiserCreate, OptixDeviceContext, OptixDenoiserModelKind,
  const OptixDenoiserOptions *, OptixDenoiserStructPtr *);
D(optixDenoiserDestroy, OptixDenoiserStructPtr);
D(optixDenoiserComputeMemoryResources, const OptixDenoiserStructPtr,
  unsigned int, unsigned int, OptixDenoiserSizes *);
D(optixDenoiserSetup, OptixDenoiserStructPtr, CUstream, unsigned int,
  unsigned int, CUdeviceptr, size_t, CUdeviceptr, size_t);
D(optixDenoiserInvoke, OptixDenoiserStructPtr, CUstream,
  const OptixDenoiserParams *, CUdeviceptr, size_t,
  const OptixDenoiserGuideLayer *, const OptixDenoiserLayer *, unsigned int,
  unsigned int, unsigned int, CUdeviceptr, size_t);
D(optixDenoiserComputeIntensity, OptixDenoiserStructPtr, CUstream,
  const OptixImage2D *inputImage, CUdeviceptr, CUdeviceptr, size_t);

#undef D

NAMESPACE_BEGIN(mitsuba)
extern MI_EXPORT_LIB void optix_initialize();

/**
 * \brief RAII wrapper which sets the CUDA context associated to the OptiX
 * context for the current scope.
 */
struct scoped_optix_context {
    scoped_optix_context();
    ~scoped_optix_context();
};
NAMESPACE_END(mitsuba)

#endif // defined(MI_ENABLE_CUDA)
