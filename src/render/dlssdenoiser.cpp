#if defined(MI_ENABLE_CUDA) && defined(MI_ENABLE_DLSS)

#include <mitsuba/render/dlssdenoiser.h>

#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/util.h>

#include <drjit-core/jit.h>

#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <unordered_map>

#include <nvsdk_ngx.h>
#include <nvsdk_ngx_defs_dlssd.h>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

NAMESPACE_BEGIN(mitsuba)

// =====================================================
//     Minimal subset of the CUDA driver API
// =====================================================
//
// Mitsuba does not link against the CUDA driver, all entry points are resolved
// dynamically through Dr.Jit (which loads the driver library itself). Only the
// handful of array/texture functions that DLSS needs are declared here, the
// struct layouts mirror those of the CUDA driver API.

NAMESPACE_BEGIN(cuda)

using CUarray      = void *;
using CUtexObject  = unsigned long long;
using CUsurfObject = unsigned long long;
using CUstream     = void *;
using CUresult     = int;

#define CUDA_SUCCESS                    0
#define CU_AD_FORMAT_FLOAT              0x20
#define CU_RESOURCE_TYPE_ARRAY          0
#define CU_TR_ADDRESS_MODE_CLAMP        1
#define CU_TRSF_NORMALIZED_COORDINATES  2
#define CU_MEMORYTYPE_DEVICE            2
#define CU_MEMORYTYPE_ARRAY             3

struct CUDA_ARRAY_DESCRIPTOR {
    size_t Width;
    size_t Height;
    int Format;
    unsigned int NumChannels;
};

struct CUDA_RESOURCE_DESC {
    int resType;
    union {
        struct { CUarray hArray; } array;
        struct { int reserved[32]; } reserved;
    } res;
    unsigned int flags;
};

struct CUDA_TEXTURE_DESC {
    int addressMode[3];
    int filterMode;
    unsigned int flags;
    unsigned int maxAnisotropy;
    int mipmapFilterMode;
    float mipmapLevelBias;
    float minMipmapLevelClamp;
    float maxMipmapLevelClamp;
    float borderColor[4];
    int reserved[12];
};

struct CUDA_MEMCPY2D {
    size_t srcXInBytes;
    size_t srcY;
    int srcMemoryType;
    const void *srcHost;
    const void *srcDevice;
    CUarray srcArray;
    size_t srcPitch;
    size_t dstXInBytes;
    size_t dstY;
    int dstMemoryType;
    void *dstHost;
    void *dstDevice;
    CUarray dstArray;
    size_t dstPitch;
    size_t WidthInBytes;
    size_t Height;
};

static CUresult (*ArrayCreate)(CUarray *, const CUDA_ARRAY_DESCRIPTOR *) = nullptr;
static CUresult (*ArrayDestroy)(CUarray) = nullptr;
static CUresult (*TexObjectCreate)(CUtexObject *, const CUDA_RESOURCE_DESC *,
                                   const CUDA_TEXTURE_DESC *, const void *) = nullptr;
static CUresult (*TexObjectDestroy)(CUtexObject) = nullptr;
static CUresult (*SurfObjectCreate)(CUsurfObject *, const CUDA_RESOURCE_DESC *) = nullptr;
static CUresult (*SurfObjectDestroy)(CUsurfObject) = nullptr;
static CUresult (*Memcpy2DAsync)(const CUDA_MEMCPY2D *, CUstream) = nullptr;
static CUresult (*GetErrorString)(CUresult, const char **) = nullptr;

/// Resolve the CUDA driver entry points listed above (idempotent)
static void initialize() {
    if (ArrayCreate)
        return;

    #define L(name, symbol)                                                    \
        name = (decltype(name)) jit_cuda_lookup(symbol)

    L(TexObjectCreate,  "cuTexObjectCreate");
    L(TexObjectDestroy, "cuTexObjectDestroy");
    L(SurfObjectCreate, "cuSurfObjectCreate");
    L(SurfObjectDestroy,"cuSurfObjectDestroy");
    L(ArrayDestroy,     "cuArrayDestroy");
    L(GetErrorString,   "cuGetErrorString");
    // Versioned entry points
    L(Memcpy2DAsync,    "cuMemcpy2DAsync_v2");
    L(ArrayCreate,      "cuArrayCreate_v2");

    #undef L
}

static void check(CUresult rv, const char *name) {
    if (rv == CUDA_SUCCESS)
        return;
    const char *msg = nullptr;
    if (GetErrorString)
        GetErrorString(rv, &msg);
    Throw("DLSSDenoiser: %s() failed: %s (%i)", name, msg ? msg : "unknown", rv);
}

NAMESPACE_END(cuda)

#define cuda_check(call) cuda::check(cuda::call, #call)

/// Scoped activation of the CUDA context that Dr.Jit uses
struct scoped_cuda_context {
    scoped_cuda_context() { jit_cuda_push_context(jit_cuda_context()); }
    ~scoped_cuda_context() { jit_cuda_pop_context(); }
};

// =====================================================
//              NGX driver entry points
// =====================================================
//
// The NGX component of the NVIDIA display driver (``_nvngx.dll`` on Windows,
// ``libnvidia-ngx.so.1`` on Linux) provides the actual DLSS implementation. It
// is loaded dynamically and queried for the entry points below, which is the
// same approach that the Cycles renderer of Blender takes. The signatures of
// the driver-level entry points differ slightly from the declarations in the
// DLSS SDK headers, hence the explicit function types.
//
// Adapted from `intern/cycles/integrator/denoiser_dlss.cpp` of Blender
// (SPDX-FileCopyrightText: 2025 NVIDIA Corporation, Apache-2.0).

struct NGXDriver {
    using tInit_Ext1 = NVSDK_NGX_Result (*)(unsigned long long, const wchar_t *,
                                            NVSDK_NGX_CUDADevice *,
                                            NVSDK_NGX_Version,
                                            const NVSDK_NGX_FeatureCommonInfo *);
    using tShutdown1 = NVSDK_NGX_Result (*)(NVSDK_NGX_CUDADevice *, unsigned int &);
    using tGetFeatureRequirements = decltype(&NVSDK_NGX_CUDA_GetFeatureRequirements);
    using tCreateFeature1 = decltype(&NVSDK_NGX_CUDA_CreateFeature1);
    using tEvaluateFeature = decltype(&NVSDK_NGX_CUDA_EvaluateFeature);
    using tReleaseFeature = decltype(&NVSDK_NGX_CUDA_ReleaseFeature);
    using tAllocateParameters = decltype(&NVSDK_NGX_CUDA_AllocateParameters);
    using tDestroyParameters = decltype(&NVSDK_NGX_CUDA_DestroyParameters);

    tInit_Ext1 Init_Ext1 = nullptr;
    tShutdown1 Shutdown1 = nullptr;
    tGetFeatureRequirements GetFeatureRequirements = nullptr;
    tCreateFeature1 CreateFeature1 = nullptr;
    tEvaluateFeature EvaluateFeature = nullptr;
    tReleaseFeature ReleaseFeature = nullptr;
    tAllocateParameters AllocateParameters = nullptr;
    tDestroyParameters DestroyParameters = nullptr;

    explicit operator bool() const {
        return Init_Ext1 && Shutdown1 && CreateFeature1 && EvaluateFeature &&
               ReleaseFeature && AllocateParameters && DestroyParameters;
    }

    bool init() {
        if (*this)
            return true;

#if defined(_WIN32)
        WCHAR ngx_path[MAX_PATH] = L"";
        {
            HKEY ngx_key = nullptr;
            LSTATUS result = RegOpenKeyExW(
                HKEY_LOCAL_MACHINE,
                L"System\\CurrentControlSet\\Services\\nvlddmkm\\Parameters\\NGXCore",
                0, KEY_READ, &ngx_key);
            if (result != ERROR_SUCCESS)
                result = RegOpenKeyExW(
                    HKEY_LOCAL_MACHINE,
                    L"System\\CurrentControlSet\\Services\\nvlddmkm\\NGXCore",
                    0, KEY_READ, &ngx_key);
            if (result == ERROR_SUCCESS) {
                DWORD ngx_path_size = sizeof(ngx_path);
                result = RegQueryValueExW(ngx_key, L"NGXPath", 0, nullptr,
                                          reinterpret_cast<LPBYTE>(ngx_path),
                                          &ngx_path_size);
                RegCloseKey(ngx_key);
            }
            if (result != ERROR_SUCCESS)
                return false;

#  if defined(_M_ARM64)
            wcscat_s(ngx_path, L"\\_arm64_nvngx.dll");
#  else
            wcscat_s(ngx_path, L"\\_nvngx.dll");
#  endif
        }

        void *const module = (void *) LoadLibraryW(ngx_path);
        if (!module)
            return false;
        #define find(symbol)                                                   \
            reinterpret_cast<t##symbol>(GetProcAddress(                        \
                static_cast<HMODULE>(module), "NVSDK_NGX_CUDA_" #symbol))
#else
        void *const module = dlopen("libnvidia-ngx.so.1", RTLD_NOW);
        if (!module)
            return false;
        #define find(symbol)                                                   \
            reinterpret_cast<t##symbol>(                                       \
                dlsym(module, "NVSDK_NGX_CUDA_" #symbol))
#endif

        Init_Ext1              = find(Init_Ext1);
        Shutdown1              = find(Shutdown1);
        GetFeatureRequirements = find(GetFeatureRequirements);
        CreateFeature1         = find(CreateFeature1);
        EvaluateFeature        = find(EvaluateFeature);
        ReleaseFeature         = find(ReleaseFeature);
        AllocateParameters     = find(AllocateParameters);
        DestroyParameters      = find(DestroyParameters);

        #undef find

        if (*this)
            return true;

        *this = NGXDriver();
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(module));
#else
        dlclose(module);
#endif
        return false;
    }
};

static NGXDriver ngx;
static std::mutex ngx_mutex;

/**
 * Application ID used to identify Mitsuba to the NGX driver.
 *
 * NVIDIA assigns these IDs to applications, Mitsuba does not have one. The
 * placeholder below can be overridden through the ``MI_DLSS_APPLICATION_ID``
 * environment variable.
 */
static unsigned long long ngx_application_id() {
    if (const char *env = std::getenv("MI_DLSS_APPLICATION_ID"))
        return std::strtoull(env, nullptr, 10);
    return 0x4D495453ull; // 'MITS'
}

/// Directory in which NGX may store its logs and other temporary files
static std::wstring ngx_data_path() {
    std::filesystem::path path;
    if (const char *env = std::getenv("MI_DLSS_APP_DATA_PATH"))
        path = std::filesystem::path(env);
    else
        path = std::filesystem::temp_directory_path() / "mitsuba-dlss";

    std::error_code ec;
    std::filesystem::create_directories(path, ec);

    return path.wstring();
}

/**
 * Directory in which the NGX driver should look for the DLSS Ray
 * Reconstruction library, in addition to the directory of the executable.
 */
static std::wstring ngx_library_path() {
    if (const char *env = std::getenv("MI_DLSS_LIBRARY_PATH"))
        return std::filesystem::path(env).wstring();
    return std::filesystem::path(util::library_path().parent_path().native())
        .wstring();
}

static void ngx_log_callback(const char *message, NVSDK_NGX_Logging_Level,
                             NVSDK_NGX_Feature) {
    Log(Debug, "DLSS: %s", message);
}

/**
 * Information that is common to all NGX calls: the directories in which the
 * DLSS implementation library should be looked for (in addition to the
 * directory of the running executable), and the logging callback.
 *
 * The result borrows ``paths``, which must outlive it.
 */
static NVSDK_NGX_FeatureCommonInfo
ngx_feature_info(const wchar_t *const *paths, unsigned int n_paths) {
    NVSDK_NGX_FeatureCommonInfo info = {};
    info.PathListInfo.Path = paths;
    info.PathListInfo.Length = n_paths;
    // NGX diagnostics are forwarded to Mitsuba's logger, and only requested
    // when Mitsuba itself runs at a verbose log level. Note that the NGX
    // component of the driver prints a fair amount of information to the
    // console during initialization no matter what is configured here.
    Logger *log = mitsuba::logger();
    bool verbose = log && log->log_level() <= Debug;

    info.LoggingInfo.LoggingCallback = &ngx_log_callback;
    info.LoggingInfo.MinimumLoggingLevel = verbose
                                               ? NVSDK_NGX_LOGGING_LEVEL_ON
                                               : NVSDK_NGX_LOGGING_LEVEL_OFF;
    return info;
}

// =====================================================
//                   CUDA textures
// =====================================================

MI_VARIANT
void DLSSDenoiser<Float, Spectrum>::CUDATexture::init(uint32_t width_,
                                                      uint32_t height_,
                                                      uint32_t n_channels_) {
    // CUDA arrays only support 1, 2 or 4 channels per element
    Assert(n_channels_ == 1 || n_channels_ == 2 || n_channels_ == 4);

    width = width_;
    height = height_;
    n_channels = n_channels_;

    cuda::CUDA_ARRAY_DESCRIPTOR desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = CU_AD_FORMAT_FLOAT;
    desc.NumChannels = n_channels;

    cuda_check(ArrayCreate((cuda::CUarray *) &array, &desc));

    cuda::CUDA_TEXTURE_DESC tex_desc = {};
    tex_desc.addressMode[0] = CU_TR_ADDRESS_MODE_CLAMP;
    tex_desc.addressMode[1] = CU_TR_ADDRESS_MODE_CLAMP;
    tex_desc.addressMode[2] = CU_TR_ADDRESS_MODE_CLAMP;
    tex_desc.flags = CU_TRSF_NORMALIZED_COORDINATES;

    cuda::CUDA_RESOURCE_DESC res_desc = {};
    res_desc.resType = CU_RESOURCE_TYPE_ARRAY;
    res_desc.res.array.hArray = (cuda::CUarray) array;

    cuda_check(TexObjectCreate((cuda::CUtexObject *) &texture_handle, &res_desc,
                               &tex_desc, nullptr));
    cuda_check(SurfObjectCreate((cuda::CUsurfObject *) &surface_handle, &res_desc));
}

MI_VARIANT void DLSSDenoiser<Float, Spectrum>::CUDATexture::destroy() {
    if (!array)
        return;

    cuda::SurfObjectDestroy((cuda::CUsurfObject) surface_handle);
    cuda::TexObjectDestroy((cuda::CUtexObject) texture_handle);
    cuda::ArrayDestroy((cuda::CUarray) array);

    surface_handle = 0;
    texture_handle = 0;
    array = nullptr;
}

MI_VARIANT
void DLSSDenoiser<Float, Spectrum>::CUDATexture::upload(const void *src) const {
    cuda::CUDA_MEMCPY2D op = {};
    op.srcMemoryType = CU_MEMORYTYPE_DEVICE;
    op.srcDevice = src;
    op.srcPitch = width * n_channels * sizeof(float);
    op.dstMemoryType = CU_MEMORYTYPE_ARRAY;
    op.dstArray = (cuda::CUarray) array;
    op.WidthInBytes = width * n_channels * sizeof(float);
    op.Height = height;

    cuda_check(Memcpy2DAsync(&op, (cuda::CUstream) jit_cuda_stream()));
}

MI_VARIANT
void DLSSDenoiser<Float, Spectrum>::CUDATexture::download(void *dst) const {
    cuda::CUDA_MEMCPY2D op = {};
    op.srcMemoryType = CU_MEMORYTYPE_ARRAY;
    op.srcArray = (cuda::CUarray) array;
    op.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    op.dstDevice = dst;
    op.dstPitch = width * n_channels * sizeof(float);
    op.WidthInBytes = width * n_channels * sizeof(float);
    op.Height = height;

    cuda_check(Memcpy2DAsync(&op, (cuda::CUstream) jit_cuda_stream()));
}

// =====================================================
//                    DLSSDenoiser
// =====================================================

MI_VARIANT bool DLSSDenoiser<Float, Spectrum>::is_available() {
    if constexpr (!dr::is_cuda_v<Float>) {
        return false;
    } else {
        // `GetFeatureRequirements` is fairly expensive, cache the outcome
        static std::unordered_map<int, bool> cached;

        std::lock_guard<std::mutex> guard(ngx_mutex);

        try {
            int device = jit_cuda_device_raw();

            if (auto it = cached.find(device); it != cached.end())
                return it->second;

            // The NGX driver only exposes `GetFeatureRequirements` in
            // sufficiently recent drivers (590 or newer)
            bool result = false;
            if (ngx.init() && ngx.GetFeatureRequirements) {
                const std::wstring data_path = ngx_data_path(),
                                   library_path = ngx_library_path();
                const wchar_t *const search_paths[] = { library_path.c_str() };
                NVSDK_NGX_FeatureCommonInfo feature_info =
                    ngx_feature_info(search_paths, 1);

                NVSDK_NGX_FeatureDiscoveryInfo info = {};
                info.SDKVersion = NVSDK_NGX_Version_API;
                info.FeatureID = NVSDK_NGX_Feature_RayReconstruction;
                info.Identifier.IdentifierType =
                    NVSDK_NGX_Application_Identifier_Type_Application_Id;
                info.Identifier.v.ApplicationId = ngx_application_id();
                info.ApplicationDataPath = data_path.c_str();
                info.FeatureInfo = &feature_info;

                // A zero-initialized requirement means 'supported'
                NVSDK_NGX_FeatureRequirement req = {};

                NVSDK_NGX_Result rv =
                    ngx.GetFeatureRequirements(device, &info, &req);

                result = NVSDK_NGX_SUCCEED(rv) &&
                         req.FeatureSupported ==
                             NVSDK_NGX_FeatureSupportResult_Supported;
            }

            cached.emplace(device, result);
            return result;
        } catch (const std::exception &e) {
            Log(Debug, "DLSSDenoiser::is_available(): %s", e.what());
            return false;
        }
    }
}

MI_VARIANT
DLSSDenoiser<Float, Spectrum>::DLSSDenoiser(const ScalarVector2u &input_size,
                                            const ScalarVector2u &output_size,
                                            const std::string &quality)
    : m_input_size(input_size),
      m_output_size(dr::all(output_size == 0) ? input_size : output_size),
      m_quality(quality) {
    if constexpr (!dr::is_cuda_v<Float>)
        Throw("DLSSDenoiser is only available in CUDA mode!");

    if (dr::any(m_input_size == 0))
        Throw("DLSSDenoiser: the input size must be non-zero!");

    if (dr::any(m_output_size < m_input_size))
        Throw("DLSSDenoiser: the output size (%u x %u) cannot be smaller than "
              "the input size (%u x %u), DLSS does not downscale!",
              m_output_size.x(), m_output_size.y(), m_input_size.x(),
              m_input_size.y());

    // DLSS refuses to create a feature below this resolution
    if (m_input_size.x() < 32 || m_input_size.y() < 32)
        Throw("DLSSDenoiser: the input size (%u x %u) is too small, DLSS "
              "requires at least 32 x 32 pixels!",
              m_input_size.x(), m_input_size.y());

    NVSDK_NGX_PerfQuality_Value perf_quality = NVSDK_NGX_PerfQuality_Value_DLAA;
    const float upscale_factor =
        m_output_size.x() / (float) m_input_size.x();
    if (upscale_factor > 1.f) {
        if (m_quality == "high")
            perf_quality = NVSDK_NGX_PerfQuality_Value_MaxQuality;
        else if (m_quality == "balanced")
            perf_quality = NVSDK_NGX_PerfQuality_Value_Balanced;
        else if (m_quality == "fast")
            perf_quality = upscale_factor >= 3.f
                               ? NVSDK_NGX_PerfQuality_Value_UltraPerformance
                               : NVSDK_NGX_PerfQuality_Value_MaxPerf;
        else
            Throw("DLSSDenoiser: unknown quality mode \"%s\", expected one of "
                  "\"high\", \"balanced\" or \"fast\"!", m_quality);
    } else if (m_quality != "high" && m_quality != "balanced" &&
               m_quality != "fast") {
        Throw("DLSSDenoiser: unknown quality mode \"%s\", expected one of "
              "\"high\", \"balanced\" or \"fast\"!", m_quality);
    }

    if (!is_available())
        Throw("DLSSDenoiser: DLSS Ray Reconstruction is not available on this "
              "system. It requires an NVIDIA RTX GPU, driver version 590 or "
              "newer, and the DLSS Ray Reconstruction library (nvngx_dlssd.dll "
              "or libnvidia-ngx-dlssd.so) to be present next to the "
              "executable or in the directory given by the "
              "MI_DLSS_LIBRARY_PATH environment variable.");

    cuda::initialize();

    scoped_cuda_context context_guard;

    const std::wstring data_path = ngx_data_path(),
                       library_path = ngx_library_path();
    const wchar_t *const search_paths[] = { library_path.c_str() };

    NVSDK_NGX_FeatureCommonInfo feature_info =
        ngx_feature_info(search_paths, 1);

    m_device = new NVSDK_NGX_CUDADevice{ jit_cuda_context(), jit_cuda_stream() };

    {
        std::lock_guard<std::mutex> guard(ngx_mutex);
        NVSDK_NGX_Result rv =
            ngx.Init_Ext1(ngx_application_id(), data_path.c_str(), m_device,
                          NVSDK_NGX_Version_API, &feature_info);
        if (NVSDK_NGX_FAILED(rv)) {
            delete m_device;
            m_device = nullptr;
            Throw("DLSSDenoiser: failed to initialize the NGX driver (0x%x)!",
                  (unsigned) rv);
        }
    }

    NVSDK_NGX_Parameter *params = nullptr;
    if (NVSDK_NGX_FAILED(ngx.AllocateParameters(&params))) {
        release();
        Throw("DLSSDenoiser: failed to allocate NGX parameters!");
    }

    params->Set(NVSDK_NGX_Parameter_Width, m_input_size.x());
    params->Set(NVSDK_NGX_Parameter_Height, m_input_size.y());
    params->Set(NVSDK_NGX_Parameter_OutWidth, m_output_size.x());
    params->Set(NVSDK_NGX_Parameter_OutHeight, m_output_size.y());
    params->Set(NVSDK_NGX_Parameter_PerfQualityValue, perf_quality);

    params->Set(NVSDK_NGX_Parameter_DLSS_Denoise_Mode,
                NVSDK_NGX_DLSS_Denoise_Mode_DLUnified);
    // Mitsuba renders high dynamic range images, and the motion vectors that
    // this class expects are given at the input resolution
    params->Set(NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags,
                NVSDK_NGX_DLSS_Feature_Flags_IsHDR |
                    NVSDK_NGX_DLSS_Feature_Flags_MVLowRes);
    params->Set(NVSDK_NGX_Parameter_DLSS_Enable_Output_Subrects, 0);
    params->Set(NVSDK_NGX_Parameter_Use_HW_Depth,
                NVSDK_NGX_DLSS_Depth_Type_Linear);
    // The roughness is packed into the fourth channel of the normals
    params->Set(NVSDK_NGX_Parameter_DLSS_Roughness_Mode,
                NVSDK_NGX_DLSS_Roughness_Mode_Packed);

    NVSDK_NGX_Result rv = ngx.CreateFeature1(
        m_device, NVSDK_NGX_Feature_RayReconstruction, params, &m_handle);

    ngx.DestroyParameters(params);

    if (NVSDK_NGX_FAILED(rv)) {
        release();
        Throw("DLSSDenoiser: failed to create the DLSS feature (0x%x)!",
              (unsigned) rv);
    }

    m_color.init(m_input_size.x(), m_input_size.y(), 4);
    m_depth.init(m_input_size.x(), m_input_size.y(), 1);
    m_diffuse_albedo.init(m_input_size.x(), m_input_size.y(), 4);
    m_specular_albedo.init(m_input_size.x(), m_input_size.y(), 4);
    m_normal_roughness.init(m_input_size.x(), m_input_size.y(), 4);
    m_motion.init(m_input_size.x(), m_input_size.y(), 2);
    m_specular_motion.init(m_input_size.x(), m_input_size.y(), 2);
    m_output.init(m_output_size.x(), m_output_size.y(), 4);
}

MI_VARIANT DLSSDenoiser<Float, Spectrum>::~DLSSDenoiser() { release(); }

MI_VARIANT void DLSSDenoiser<Float, Spectrum>::release() {
    if constexpr (dr::is_cuda_v<Float>) {
        scoped_cuda_context context_guard;

        m_color.destroy();
        m_depth.destroy();
        m_diffuse_albedo.destroy();
        m_specular_albedo.destroy();
        m_normal_roughness.destroy();
        m_motion.destroy();
        m_specular_motion.destroy();
        m_output.destroy();

        std::lock_guard<std::mutex> guard(ngx_mutex);

        if (m_handle) {
            ngx.ReleaseFeature(m_handle);
            m_handle = nullptr;
        }

        if (m_device) {
            unsigned int n = 0;
            ngx.Shutdown1(m_device, n);
            delete m_device;
            m_device = nullptr;
        }
    }
}

MI_VARIANT
typename DLSSDenoiser<Float, Spectrum>::TensorXf
DLSSDenoiser<Float, Spectrum>::operator()(
    const TensorXf &noisy, const TensorXf &albedo, const TensorXf &normals,
    const TensorXf &depth, const TensorXf &specular_albedo,
    const TensorXf &roughness, const TensorXf &flow,
    const TensorXf &specular_flow, const AffineTransform4f &to_sensor,
    const ScalarPoint2f &jitter, bool reset) const {
    using TensorArray = typename TensorXf::Array;

    validate_input(noisy, albedo, normals, depth, specular_albedo, roughness,
                   flow, specular_flow);

    const size_t n_in  = (size_t) m_input_size.x() * m_input_size.y(),
                 n_out = (size_t) m_output_size.x() * m_output_size.y();
    const uint32_t noisy_channels = (uint32_t) noisy.shape(2);

    /// Copy one channel of `src` into one channel of `dst`
    auto copy_channel = [](TensorArray &dst, uint32_t dst_channels,
                           uint32_t dst_channel, const TensorArray &src,
                           uint32_t src_channels, uint32_t src_channel) {
        UInt32 index = dr::arange<UInt32>(dst.size() / dst_channels);
        dr::scatter(dst,
                    dr::gather<Float>(src, index * src_channels + src_channel),
                    index * dst_channels + dst_channel);
    };

    /// Set one channel of `dst` to a constant
    auto fill_channel = [](TensorArray &dst, uint32_t dst_channels,
                           uint32_t dst_channel, Float value) {
        UInt32 index = dr::arange<UInt32>(dst.size() / dst_channels);
        dr::scatter(dst, value, index * dst_channels + dst_channel);
    };

    // ------------- Assemble the inputs expected by DLSS -------------

    // DLSS reads the color from an RGBA texture
    TensorArray color = dr::zeros<TensorArray>(n_in * 4);
    for (uint32_t i = 0; i < 3; ++i)
        copy_channel(color, 4, i, noisy.array(), noisy_channels, i);
    if (noisy_channels == 4)
        copy_channel(color, 4, 3, noisy.array(), 4, 3);
    else
        fill_channel(color, 4, 3, Float(1.f));

    // An albedo is a reflectance, values outside of [0, 1] confuse the network
    TensorArray diffuse_albedo = dr::zeros<TensorArray>(n_in * 4);
    TensorArray albedo_clamped = dr::clip(albedo.array(), 0.f, 1.f);
    for (uint32_t i = 0; i < 3; ++i)
        copy_channel(diffuse_albedo, 4, i, albedo_clamped, 3, i);
    fill_channel(diffuse_albedo, 4, 3, Float(1.f));

    TensorArray spec_albedo = dr::zeros<TensorArray>(n_in * 4);
    if (specular_albedo.ndim() != 0) {
        TensorArray spec_albedo_clamped =
            dr::clip(specular_albedo.array(), 0.f, 1.f);
        for (uint32_t i = 0; i < 3; ++i)
            copy_channel(spec_albedo, 4, i, spec_albedo_clamped, 3, i);
    }
    fill_channel(spec_albedo, 4, 3, Float(1.f));

    // The normals are transformed into the requested frame, the roughness is
    // packed into the fourth channel
    TensorArray normal_roughness = dr::zeros<TensorArray>(n_in * 4);
    {
        UInt32 index = dr::arange<UInt32>(n_in);
        Normal3f n;
        for (size_t i = 0; i < 3; ++i)
            n[i] = dr::gather<Float>(normals.array(), index * 3 + (uint32_t) i);

        n = to_sensor * n;

        for (uint32_t i = 0; i < 3; ++i)
            dr::scatter(normal_roughness, n[i], index * 4 + i);
    }
    if (roughness.ndim() != 0)
        copy_channel(normal_roughness, 4, 3, roughness.array(), 1, 0);

    // Depth and the two motion vector fields are used as-is
    TensorArray depth_data = depth.array();
    TensorArray motion = flow.ndim() != 0 ? flow.array()
                                          : dr::zeros<TensorArray>(n_in * 2);
    TensorArray specular_motion =
        specular_flow.ndim() != 0 ? specular_flow.array()
                                  : dr::zeros<TensorArray>(n_in * 2);

    TensorArray output = dr::empty<TensorArray>(n_out * 4);

    // All arrays must be backed by actual device memory before they can be
    // handed to DLSS. Note that ``dr::eval()`` is not sufficient here: a
    // zero-initialized tensor is represented as a literal constant and stays
    // that way, without ever receiving a data pointer.
    dr::make_opaque(color, diffuse_albedo, spec_albedo, normal_roughness,
                    depth_data, motion, specular_motion, output);

    // ------------- Run DLSS -------------

    scoped_cuda_context context_guard;

    m_color.upload(color.data());
    m_diffuse_albedo.upload(diffuse_albedo.data());
    m_specular_albedo.upload(spec_albedo.data());
    m_normal_roughness.upload(normal_roughness.data());
    m_depth.upload(depth_data.data());
    m_motion.upload(motion.data());
    m_specular_motion.upload(specular_motion.data());

    NVSDK_NGX_Parameter *params = nullptr;
    if (NVSDK_NGX_FAILED(ngx.AllocateParameters(&params)))
        Throw("DLSSDenoiser: failed to allocate NGX parameters!");

    params->Set(NVSDK_NGX_Parameter_Reset, (reset || m_reset) ? 1 : 0);

    params->Set(NVSDK_NGX_Parameter_Jitter_Offset_X, (float) jitter.x());
    params->Set(NVSDK_NGX_Parameter_Jitter_Offset_Y, (float) jitter.y());

    params->Set(NVSDK_NGX_Parameter_Color, m_color.texture_ptr());
    params->Set(NVSDK_NGX_Parameter_Depth, m_depth.texture_ptr());
    params->Set(NVSDK_NGX_Parameter_DiffuseAlbedo,
                m_diffuse_albedo.texture_ptr());
    params->Set(NVSDK_NGX_Parameter_SpecularAlbedo,
                m_specular_albedo.texture_ptr());
    params->Set(NVSDK_NGX_Parameter_GBuffer_Normals,
                m_normal_roughness.texture_ptr());
    params->Set(NVSDK_NGX_Parameter_GBuffer_Roughness,
                m_normal_roughness.texture_ptr());
    params->Set(NVSDK_NGX_Parameter_MotionVectors, m_motion.texture_ptr());
    params->Set(NVSDK_NGX_Parameter_GBuffer_SpecularMvec,
                m_specular_motion.texture_ptr());
    params->Set(NVSDK_NGX_Parameter_Output, m_output.surface_ptr());

    params->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width,
                m_input_size.x());
    params->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height,
                m_input_size.y());

    // Mitsuba images are stored top to bottom, just like the textures that
    // DLSS expects
    params->Set(NVSDK_NGX_Parameter_DLSS_Indicator_Invert_Y_Axis, 0);

    NVSDK_NGX_Result rv = ngx.EvaluateFeature(m_handle, params, nullptr);

    ngx.DestroyParameters(params);

    if (NVSDK_NGX_FAILED(rv))
        Throw("DLSSDenoiser: failed to evaluate DLSS (0x%x)!", (unsigned) rv);

    m_reset = false;

    m_output.download(output.data());

    // ------------- Assemble the result -------------

    TensorArray result = dr::zeros<TensorArray>(n_out * noisy_channels);
    for (uint32_t i = 0; i < 3; ++i)
        copy_channel(result, noisy_channels, i, output, 4, i);

    if (noisy_channels == 4) {
        // DLSS does not upscale the alpha channel, copy it from the nearest
        // pixel of the noisy input instead
        UInt32 index = dr::arange<UInt32>(n_out);
        UInt32 x = index % m_output_size.x(), y = index / m_output_size.x();
        UInt32 source = (y * m_input_size.y() / m_output_size.y()) *
                            m_input_size.x() +
                        (x * m_input_size.x() / m_output_size.x());
        dr::scatter(result, dr::gather<Float>(noisy.array(), source * 4 + 3),
                    index * 4 + 3);
    }

    return TensorXf(std::move(result),
                    { m_output_size.y(), m_output_size.x(), noisy_channels });
}

MI_VARIANT
ref<Bitmap> DLSSDenoiser<Float, Spectrum>::operator()(
    const ref<Bitmap> &noisy, const std::string &albedo_ch,
    const std::string &normals_ch, const std::string &depth_ch,
    const std::string &specular_albedo_ch, const std::string &roughness_ch,
    const std::string &flow_ch, const std::string &specular_flow_ch,
    const AffineTransform4f &to_sensor, const ScalarPoint2f &jitter,
    bool reset, const std::string &noisy_ch) const {
    if (noisy->pixel_format() != Bitmap::PixelFormat::MultiChannel)
        Throw("DLSSDenoiser: the noisy input must use the MultiChannel pixel "
              "format, as DLSS always requires additional guiding layers!");

    // Search for each layer
    std::vector<std::pair<std::string, ref<Bitmap>>> layers = noisy->split();

    auto find_layer = [&](const std::string &name,
                          bool required) -> ref<const Bitmap> {
        if (name.empty()) {
            if (required)
                Throw("DLSSDenoiser: the name of the layer holding the "
                      "guiding information must be specified!");
            return nullptr;
        }
        for (auto &layer : layers) {
            if (layer.first == name)
                return layer.second.get();
        }
        Throw("Could not find layer with channel name '%s' in Bitmap:\n%s",
              name, noisy->to_string());
        return nullptr; // Unreachable, `Throw` does not return
    };

    ref<const Bitmap> noisy_bmp = find_layer(noisy_ch, true),
                      albedo_bmp = find_layer(albedo_ch, true),
                      normals_bmp = find_layer(normals_ch, true),
                      depth_bmp = find_layer(depth_ch, true),
                      spec_albedo_bmp = find_layer(specular_albedo_ch, false),
                      roughness_bmp = find_layer(roughness_ch, false),
                      flow_bmp = find_layer(flow_ch, false),
                      spec_flow_bmp = find_layer(specular_flow_ch, false);

    // Transfer every layer into a TensorXf object
    auto setup_tensor = [](const ref<const Bitmap> &bmp,
                           size_t channel_count) -> TensorXf {
        if (bmp == nullptr)
            return TensorXf();
        return TensorXf(bmp->data(),
                        { bmp->height(), bmp->width(), channel_count });
    };

    size_t noisy_channel_count = noisy_bmp->channel_count();

    TensorXf denoised =
        (*this)(setup_tensor(noisy_bmp, noisy_channel_count),
                setup_tensor(albedo_bmp, 3), setup_tensor(normals_bmp, 3),
                setup_tensor(depth_bmp, 1), setup_tensor(spec_albedo_bmp, 3),
                setup_tensor(roughness_bmp, 1), setup_tensor(flow_bmp, 2),
                setup_tensor(spec_flow_bmp, 2), to_sensor, jitter, reset);

    void *denoised_data =
        jit_malloc_migrate(denoised.data(), JitBackend::None, false);
    ref<Bitmap> output = new Bitmap(
        noisy_bmp->pixel_format(), sj::Type::Float32,
        { denoised.shape(1), denoised.shape(0) }, denoised.shape(2), {});

    jit_sync_thread(); // Wait for `denoised_data` to be ready
    memcpy(output->data(), denoised_data, output->buffer_size());
    jit_free(denoised_data);

    return output;
}

MI_VARIANT std::string DLSSDenoiser<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << "DLSSDenoiser[" << std::endl
        << "  input_size = " << m_input_size << "," << std::endl
        << "  output_size = " << m_output_size << "," << std::endl
        << "  quality = \"" << m_quality << "\"" << std::endl
        << "]";
    return oss.str();
}

MI_VARIANT
void DLSSDenoiser<Float, Spectrum>::validate_input(
    const TensorXf &noisy, const TensorXf &albedo, const TensorXf &normals,
    const TensorXf &depth, const TensorXf &specular_albedo,
    const TensorXf &roughness, const TensorXf &flow,
    const TensorXf &specular_flow) const {
    if (albedo.ndim() == 0)
        Throw("DLSSDenoiser: a diffuse albedo layer must be specified!");
    if (normals.ndim() == 0)
        Throw("DLSSDenoiser: a normals layer must be specified!");
    if (depth.ndim() == 0)
        Throw("DLSSDenoiser: a depth layer must be specified!");

    auto check_resolution = [&](const TensorXf &tensor) {
        if (tensor.ndim() != 0 && (m_input_size.x() != tensor.shape(1) ||
                                   m_input_size.y() != tensor.shape(0)))
            Throw("The denoiser was created for inputs of size %u x %u (width "
                  "x height). At least one of the input arguments does not "
                  "have this size. You must create a new denoiser object for "
                  "inputs of different sizes!",
                  m_input_size.x(), m_input_size.y());
    };
    check_resolution(noisy);
    check_resolution(albedo);
    check_resolution(normals);
    check_resolution(depth);
    check_resolution(specular_albedo);
    check_resolution(roughness);
    check_resolution(flow);
    check_resolution(specular_flow);

    auto check_channels = [](const TensorXf &tensor, size_t expected,
                             const char *name) {
        if (tensor.ndim() != 0 && tensor.shape(2) != expected)
            Throw("DLSSDenoiser: the %s must have exactly %zu channel(s)!",
                  name, expected);
    };
    if (noisy.shape(2) != 3 && noisy.shape(2) != 4)
        Throw("The noisy input must have at least 3 channels and at most 4!");
    check_channels(albedo, 3, "diffuse albedo");
    check_channels(normals, 3, "normals");
    check_channels(depth, 1, "depth");
    check_channels(specular_albedo, 3, "specular albedo");
    check_channels(roughness, 1, "roughness");
    check_channels(flow, 2, "optical flow");
    check_channels(specular_flow, 2, "specular optical flow");
}

MI_INSTANTIATE_CLASS(DLSSDenoiser)

NAMESPACE_END(mitsuba)

#endif // defined(MI_ENABLE_CUDA) && defined(MI_ENABLE_DLSS)
