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

/// Various sizes related to the denoiser.
///
/// \see #optixDenoiserComputeMemoryResources()
struct OptixDenoiserSizes
{
    size_t       stateSizeInBytes;
    size_t       withOverlapScratchSizeInBytes;
    size_t       withoutOverlapScratchSizeInBytes;
    unsigned int overlapWindowSizeInPixels;
};

/// Various parameters used by the denoiser
///
/// \see #optixDenoiserInvoke()
/// \see #optixDenoiserComputeIntensity()
/// \see #optixDenoiserComputeAverageColor()
struct OptixDenoiserParams
{
    /// if set to nonzero value, denoise alpha channel (if present) in first inputLayer image
    unsigned int denoiseAlpha;

    /// average log intensity of input image (default null pointer). points to a single float.
    /// with the default (null pointer) denoised results will not be optimal for very dark or
    /// bright input images.
    CUdeviceptr  hdrIntensity;

    /// blend factor.
    /// If set to 0 the output is 100% of the denoised input. If set to 1, the output is 100% of
    /// the unmodified input. Values between 0 and 1 will linearly interpolate between the denoised
    /// and unmodified input.
    float        blendFactor;

    /// this parameter is used when the OPTIX_DENOISER_MODEL_KIND_AOV model kind is set.
    /// average log color of input image, separate for RGB channels (default null pointer).
    /// points to three floats. with the default (null pointer) denoised results will not be
    /// optimal.
    CUdeviceptr  hdrAverageColor;
};

/// Input kinds used by the denoiser.
///
/// RGB(A) values less than zero will be clamped to zero.
/// Albedo values must be in the range [0..1] (values less than zero will be clamped to zero).
/// The normals must be transformed into screen space. The z component is not used.
/// \see #OptixDenoiserOptions::inputKind
enum OptixDenoiserInputKind
{
    OPTIX_DENOISER_INPUT_RGB               = 0x2301,
    OPTIX_DENOISER_INPUT_RGB_ALBEDO        = 0x2302,
    OPTIX_DENOISER_INPUT_RGB_ALBEDO_NORMAL = 0x2303,
};

/// Model kind used by the denoiser.
///
/// \see #optixDenoiserSetModel()
enum OptixDenoiserModelKind
{
    /// Use the model provided by the associated pointer.  See the programming guide for a
    /// description of how to format the data.
    OPTIX_DENOISER_MODEL_KIND_USER = 0x2321,

    /// Use the built-in model appropriate for low dynamic range input.
    OPTIX_DENOISER_MODEL_KIND_LDR = 0x2322,

    /// Use the built-in model appropriate for high dynamic range input.
    OPTIX_DENOISER_MODEL_KIND_HDR = 0x2323,

    /// Use the built-in model appropriate for high dynamic range input and support for AOVs
    OPTIX_DENOISER_MODEL_KIND_AOV = 0x2324,

};

/// Options used by the denoiser
///
/// \see #optixDenoiserCreate()
struct OptixDenoiserOptions
{
    /// The kind of denoiser input.
    OptixDenoiserInputKind inputKind;
};

/// Pixel formats used by the denoiser.
///
/// \see #OptixImage2D::format
enum OptixPixelFormat
{
    OPTIX_PIXEL_FORMAT_HALF3  = 0x2201,  ///< three halfs, RGB
    OPTIX_PIXEL_FORMAT_HALF4  = 0x2202,  ///< four halfs, RGBA
    OPTIX_PIXEL_FORMAT_FLOAT3 = 0x2203,  ///< three floats, RGB
    OPTIX_PIXEL_FORMAT_FLOAT4 = 0x2204,  ///< four floats, RGBA
    OPTIX_PIXEL_FORMAT_UCHAR3 = 0x2205,  ///< three unsigned chars, RGB
    OPTIX_PIXEL_FORMAT_UCHAR4 = 0x2206   ///< four unsigned chars, RGBA
};

/// Image descriptor used by the denoiser.
///
/// \see #optixDenoiserInvoke(), #optixDenoiserComputeIntensity()
struct OptixImage2D
{
    /// Pointer to the actual pixel data.
    CUdeviceptr data;
    /// Width of the image (in pixels)
    unsigned int width;
    /// Height of the image (in pixels)
    unsigned int height;
    /// Stride between subsequent rows of the image (in bytes).
    unsigned int rowStrideInBytes;
    /// Stride between subsequent pixels of the image (in bytes).
    /// For now, only 0 or the value that corresponds to a dense packing of pixels (no gaps) is supported.
    unsigned int pixelStrideInBytes;
    /// Pixel format.
    OptixPixelFormat format;
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
D(optixModuleDestroy, OptixModule);
D(optixTaskExecute, OptixTask, OptixTask *, unsigned int, unsigned int *);
D(optixProgramGroupCreate, OptixDeviceContext, const OptixProgramGroupDesc *,
  unsigned int, const OptixProgramGroupOptions *, char *, size_t *,
  OptixProgramGroup *);
D(optixProgramGroupDestroy, OptixProgramGroup);
D(optixSbtRecordPackHeader, OptixProgramGroup, void *);
D(optixAccelCompact, OptixDeviceContext, CUstream, OptixTraversableHandle,
  CUdeviceptr, size_t, OptixTraversableHandle *);

#undef D

NAMESPACE_BEGIN(mitsuba)
extern MI_EXPORT_LIB void optix_initialize();
extern MI_EXPORT_LIB void optix_shutdown();
NAMESPACE_END(mitsuba)

#endif // defined(MI_ENABLE_CUDA)
