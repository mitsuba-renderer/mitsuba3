#pragma once

#if defined(MI_ENABLE_CUDA) && defined(MI_ENABLE_DLSS)

#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/fwd.h>
#include <drjit/tensor.h>

// Opaque NGX types, so that the DLSS SDK headers do not leak into this file
struct NVSDK_NGX_Handle;
struct NVSDK_NGX_CUDADevice;

NAMESPACE_BEGIN(mitsuba)

/**
 * Wrapper for the NVIDIA DLSS Ray Reconstruction denoiser
 *
 * DLSS Ray Reconstruction (also known as DLSS-D) is a real-time denoiser which
 * additionally performs temporal antialiasing and optional upscaling. It is
 * wrapped in this object such that it can work directly with Mitsuba types and
 * its conventions.
 *
 * In contrast to `OptixDenoiser`, this denoiser is inherently temporal: it
 * expects a sequence of independently rendered frames (typically a single
 * sample per pixel each), the subpixel jitter that was applied to the camera of
 * every frame, and screen-space motion vectors relating consecutive frames. It
 * accumulates information across the frames that are passed to it, which means
 * that the output of the very first frames of a sequence will be considerably
 * noisier than that of later ones.
 *
 * The implementation is loaded from the NVIDIA display driver at runtime. It
 * requires driver version 590 or newer, an RTX GPU, and the DLSS Ray
 * Reconstruction library (``nvngx_dlssd.dll`` on Windows,
 * ``libnvidia-ngx-dlssd.so.*`` on Linux) to be installed in one of the
 * directories which are searched by the NGX driver. Use `is_available()` to
 * query whether all of these requirements are met before constructing an
 * instance of this class. The environment variable ``MI_DLSS_LIBRARY_PATH``
 * can be used to add a directory to that search path.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB DLSSDenoiser : public Object {
public:
    MI_IMPORT_TYPES()

    /**
     * Check whether DLSS Ray Reconstruction can be used on this system
     *
     * This queries the NGX driver for the availability of the feature on the
     * CUDA device which is currently active. It returns ``false`` when Mitsuba
     * was compiled without DLSS support, when the driver is too old, when the
     * GPU is unsupported, or when the DLSS Ray Reconstruction library could not
     * be found.
     */
    static bool is_available();

    /**
     * Constructs a DLSS denoiser
     *
     * Args:
     *     input_size: Resolution of the noisy images that will be fed to the
     *         denoiser.
     *
     *     output_size: Resolution of the denoised images produced by the
     *         denoiser. When it is larger than ``input_size``, DLSS will
     *         upscale the input.
     *         This parameter is optional, by default it is equal to
     *         ``input_size`` (i.e. no upscaling, DLAA mode).
     *
     *     quality: Quality/performance tradeoff of the upscaling step, one of
     *         ``"high"``, ``"balanced"`` or ``"fast"``. This parameter has no
     *         effect when ``output_size`` is equal to ``input_size``.
     *         This parameter is optional, by default it is ``"high"``.
     *
     * Returns:
     *     A callable object which will apply the DLSS denoiser.
     */
    DLSSDenoiser(const ScalarVector2u &input_size,
                 const ScalarVector2u &output_size = ScalarVector2u(0, 0),
                 const std::string &quality = "high");

    DLSSDenoiser(const DLSSDenoiser &other) = delete;

    DLSSDenoiser &operator=(const DLSSDenoiser &other) = delete;

    ~DLSSDenoiser();

    /**
     * Apply denoiser on inputs which are `TensorXf` objects.
     *
     * All input tensors must have the resolution which was passed to the
     * constructor as ``input_size``.
     *
     * Args:
     *     noisy: The noisy input. (tensor shape: (height, width, 3 | 4))
     *
     *     albedo: Diffuse albedo of the noisy rendering. Values are clamped to
     *         the [0, 1] range. (tensor shape: (height, width, 3))
     *
     *     normals: Shading normals of the noisy rendering. They should be given
     *         in the same coordinate frame which was used to compute the
     *         ``flow`` and ``depth`` arguments (typically world space), and can
     *         be transformed with the ``to_sensor`` argument.
     *         (tensor shape: (height, width, 3))
     *
     *     depth: Linear camera-space depth of the noisy rendering, i.e. the
     *         distance of the shading point to the plane of the sensor rather
     *         than the distance to the sensor itself.
     *         (tensor shape: (height, width, 1))
     *
     *     specular_albedo: Specular albedo of the noisy rendering. Leaving this
     *         parameter unspecified degrades the quality of reflections.
     *         This parameter is optional, by default it is zero.
     *         (tensor shape: (height, width, 3))
     *
     *     roughness: Surface roughness of the noisy rendering.
     *         This parameter is optional, by default it is zero.
     *         (tensor shape: (height, width, 1))
     *
     *     flow: Screen-space motion vectors, in pixels of the input resolution,
     *         pointing from the position of a pixel in the current frame to its
     *         position in the previous frame.
     *         This parameter is optional, by default it is zero, which will
     *         produce ghosting whenever the camera or the scene moves.
     *         (tensor shape: (height, width, 2))
     *
     *     specular_flow: Screen-space motion vectors of the virtual image seen
     *         in specular surfaces, in the same units as ``flow``.
     *         This parameter is optional, by default it is zero.
     *         (tensor shape: (height, width, 2))
     *
     *     to_sensor: An `AffineTransform4f` which is applied to the ``normals``
     *         parameter before denoising.
     *         This parameter is optional, by default no transformation is
     *         applied.
     *
     *     jitter: The subpixel offset which was applied to the camera when
     *         rendering the noisy input, in pixels and relative to the center
     *         of a pixel. Every frame of a sequence must use a different
     *         offset, e.g. taken from a low-discrepancy sequence covering
     *         the [-0.5, 0.5] x [-0.5, 0.5] domain.
     *         This parameter is optional, by default it is zero.
     *
     *     reset: Discard the history which was accumulated from the previous
     *         frames. This should be set whenever the frame that is passed to
     *         the denoiser is not temporally related to the previous one.
     *         This parameter is optional, by default it is false. It is
     *         implicitly true for the first invocation.
     *
     * Returns:
     *     The denoised input, at the ``output_size`` resolution and with the
     *     same number of channels as the ``noisy`` argument.
     */
    TensorXf operator()(const TensorXf &noisy,
                        const TensorXf &albedo,
                        const TensorXf &normals,
                        const TensorXf &depth,
                        const TensorXf &specular_albedo = TensorXf(),
                        const TensorXf &roughness = TensorXf(),
                        const TensorXf &flow = TensorXf(),
                        const TensorXf &specular_flow = TensorXf(),
                        const AffineTransform4f &to_sensor = AffineTransform4f(),
                        const ScalarPoint2f &jitter = ScalarPoint2f(0.f),
                        bool reset = false) const;

    /**
     * Apply denoiser on inputs which are `Bitmap` objects.
     *
     * The ``noisy`` parameter must use the `Bitmap.PixelFormat.MultiChannel`
     * pixel format, as the denoiser always requires additional guiding
     * information.
     *
     * Args:
     *     noisy: A multichannel `Bitmap` holding the noisy input as well as all
     *         guiding layers.
     *
     *     albedo_ch: The name of the layer in the ``noisy`` parameter which
     *         contains the diffuse albedo.
     *
     *     normals_ch: The name of the layer in the ``noisy`` parameter which
     *         contains the shading normals.
     *
     *     depth_ch: The name of the layer in the ``noisy`` parameter which
     *         contains the linear camera-space depth.
     *
     *     specular_albedo_ch: The name of the layer in the ``noisy`` parameter
     *         which contains the specular albedo.
     *         This parameter is optional.
     *
     *     roughness_ch: The name of the layer in the ``noisy`` parameter which
     *         contains the surface roughness.
     *         This parameter is optional.
     *
     *     flow_ch: The name of the layer in the ``noisy`` parameter which
     *         contains the screen-space motion vectors.
     *         This parameter is optional.
     *
     *     specular_flow_ch: The name of the layer in the ``noisy`` parameter
     *         which contains the specular screen-space motion vectors.
     *         This parameter is optional.
     *
     *     to_sensor: An `AffineTransform4f` which is applied to the normals
     *         before denoising.
     *         This parameter is optional, by default no transformation is
     *         applied.
     *
     *     jitter: The subpixel camera offset of the noisy input, in pixels.
     *         This parameter is optional, by default it is zero.
     *
     *     reset: Discard the history accumulated from the previous frames.
     *         This parameter is optional, by default it is false.
     *
     *     noisy_ch: The name of the layer in the ``noisy`` parameter which
     *         contains the noisy image to be denoised.
     *
     * Returns:
     *     The denoised input.
     */
    ref<Bitmap> operator()(const ref<Bitmap> &noisy,
                           const std::string &albedo_ch,
                           const std::string &normals_ch,
                           const std::string &depth_ch,
                           const std::string &specular_albedo_ch = "",
                           const std::string &roughness_ch = "",
                           const std::string &flow_ch = "",
                           const std::string &specular_flow_ch = "",
                           const AffineTransform4f &to_sensor = AffineTransform4f(),
                           const ScalarPoint2f &jitter = ScalarPoint2f(0.f),
                           bool reset = false,
                           const std::string &noisy_ch = "<root>") const;

    virtual std::string to_string() const override;

    MI_DECLARE_CLASS(DLSSDenoiser)

private:
    /// A 2D CUDA array along with the texture and surface objects viewing it
    struct CUDATexture {
        void init(uint32_t width, uint32_t height, uint32_t n_channels);
        void destroy();

        /// Copy `n_channels` interleaved floats per pixel from device memory
        void upload(const void *src) const;

        /// Copy `n_channels` interleaved floats per pixel to device memory
        void download(void *dst) const;

        /// Pointers to the handles, in the form in which NGX expects them
        void *texture_ptr() const {
            return const_cast<uint64_t *>(&texture_handle);
        }
        void *surface_ptr() const {
            return const_cast<uint64_t *>(&surface_handle);
        }

        void *array = nullptr;
        uint64_t texture_handle = 0;
        uint64_t surface_handle = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t n_channels = 0;
    };

    /// Helper function to validate tensor sizes
    void validate_input(const TensorXf &noisy, const TensorXf &albedo,
                        const TensorXf &normals, const TensorXf &depth,
                        const TensorXf &specular_albedo,
                        const TensorXf &roughness, const TensorXf &flow,
                        const TensorXf &specular_flow) const;

    /// Release the NGX feature and all CUDA arrays
    void release();

    ScalarVector2u m_input_size;
    ScalarVector2u m_output_size;
    std::string m_quality;

    NVSDK_NGX_Handle *m_handle = nullptr;
    NVSDK_NGX_CUDADevice *m_device = nullptr;

    CUDATexture m_color;
    CUDATexture m_depth;
    CUDATexture m_diffuse_albedo;
    CUDATexture m_specular_albedo;
    CUDATexture m_normal_roughness;
    CUDATexture m_motion;
    CUDATexture m_specular_motion;
    CUDATexture m_output;

    /// DLSS has no history for the first invocation, force a reset
    mutable bool m_reset = true;
};

MI_EXTERN_CLASS(DLSSDenoiser)
NAMESPACE_END(mitsuba)

#endif // defined(MI_ENABLE_CUDA) && defined(MI_ENABLE_DLSS)
