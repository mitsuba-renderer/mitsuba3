#pragma once

#if defined(MI_ENABLE_CUDA)

#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/optix_api.h>
#include <drjit/tensor.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Wrapper for the OptiX AI denoiser
 *
 * The OptiX AI denoiser is wrapped in this object such that it can work
 * directly with Mitsuba types and its conventions.
 * 
 * The denoiser works best when applied to noisy renderings that were produced
 * with a \ref Film which used the `box` \ref ReconstructionFilter. With a
 * filter that spans multiple pixels, the denoiser might identify some local
 * variance as a feature of the scene and will not denoise it.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB OptixDenoiser : public Object {
public:
    MI_IMPORT_TYPES()

    /**
     * \brief Constructs an OptiX denoiser
     *
     * \param input_size
     *      Resolution of noisy images that will be fed to the denoiser.
     *
     * \param albedo
     *      Whether or not albedo information will also be given to the
     *      denoiser.
     *
     * \param normals
     *      Whether or not shading normals information will also be given to the
     *      Denoiser.
     *
     * \return A callable object which will apply the OptiX denoiser.
     */
    OptixDenoiser(const ScalarVector2u &input_size, bool albedo, bool normals,
                  bool temporal);

    OptixDenoiser(const OptixDenoiser &other) = delete;

    OptixDenoiser& operator=(const OptixDenoiser &other) = delete;

    ~OptixDenoiser();

    /**
     * \brief Apply denoiser on inputs which are \ref TensorXf objects.
     *
     * \param noisy
     *      The noisy input. (tensor shape: (width, height, 3 | 4))
     *
     * \param denoise_alpha
     *      Whether or not the alpha channel (if specified in the noisy input)
     *      should be denoised too.
     *      This parameter is optional, by default it is true.
     *
     * \param albedo
     *      Albedo information of the noisy rendering.
     *      This parameter is optional unless the OptixDenoiser was built with
     *      albedo support. (tensor shape: (width, height, 3))
     *
     * \param normals
     *      Shading normal information of the noisy rendering. The normals must
     *      be in the coordinate frame of the sensor which was used to render
     *      the noisy input.
     *      This parameter is optional unless the OptixDenoiser was built with
     *      normals support. (tensor shape: (width, height, 3))
     *
     * \param to_sensor
     *      A \ref Transform4f which is applied to the \c normals parameter
     *      before denoising. This should be used to transform the normals into
     *      the correct coordinate frame.
     *      This parameter is optional, by default no transformation is
     *      applied.
     *
     * \param flow
     *      With temporal denoising, this parameter is the optical flow between
     *      the previous frame and the current one. It should capture the 2D
     *      motion of each individual pixel.
     *      When this parameter is unknown, it can been set to a
     *      zero-initialized TensorXf of the correct size and still produce
     *      convincing results.
     *      This parameter is optional unless the OptixDenoiser was built with
     *      temporal denoising support. (tensor shape: (width, height, 2))
     *
     * \param previous_denoised
     *      With temporal denoising, the previous denoised frame should be
     *      passed here.
     *      For the very first frame, the OptiX documentation recommends
     *      passing the noisy input for this argument.
     *      This parameter is optional unless the OptixDenoiser was built with
     *      temporal denoising support. (tensor shape: (width, height, 3 | 4))
     *
     * \return The denoised input.
     */
    TensorXf operator()(const TensorXf &noisy,
                        bool denoise_alpha = true,
                        const TensorXf &albedo = TensorXf(),
                        const TensorXf &normals = TensorXf(),
                        const Transform4f &to_sensor = Transform4f(),
                        const TensorXf &flow = TensorXf(),
                        const TensorXf &previous_denoised = TensorXf()) const;

    /**
     * \brief Apply denoiser on inputs which are \ref Bitmap objects.
     *
     * \param noisy
     *      The noisy input. When passing additional information like albedo or
     *      normals to the denoiser, this \ref Bitmap object must be a \ref
     *      MultiChannel bitmap.
     *
     * \param denoise_alpha
     *      Whether or not the alpha channel (if specified in the noisy input)
     *      should be denoised too.
     *      This parameter is optional, by default it is true.
     *
     * \param albedo_ch
     *      The name of the channel in the \c noisy parameter which contains
     *      the albedo information of the noisy rendering.
     *      This parameter is optional unless the OptixDenoiser was built with
     *      albedo support.
     *
     * \param normals_ch
     *      The name of the channel in the \c noisy parameter which contains
     *      the shading normal information of the noisy rendering.
     *      The normals must be in the coordinate frame of the sensor which was
     *      used to render the noisy input.
     *      This parameter is optional unless the OptixDenoiser was built with
     *      normals support.
     *
     * \param to_sensor
     *      A \ref Transform4f which is applied to the \c normals parameter
     *      before denoising. This should be used to transform the normals into
     *      the correct coordinate frame.
     *      This parameter is optional, by default no transformation is
     *      applied.
     *
     * \param flow_ch
     *      With temporal denoising, this parameter is name of the channel in
     *      the \c noisy parameter which contains the optical flow between
     *      the previous frame and the current one. It should capture the 2D
     *      motion of each individual pixel.
     *      When this parameter is unknown, it can been set to a
     *      zero-initialized TensorXf of the correct size and still produce
     *      convincing results.
     *      This parameter is optional unless the OptixDenoiser was built with
     *      temporal denoising support.
     *
     * \param previous_denoised_ch
     *      With temporal denoising, this parameter is name of the channel in
     *      the \c noisy parameter which contains the previous denoised frame.
     *      For the very first frame, the OptiX documentation recommends
     *      passing the noisy input for this argument.
     *      This parameter is optional unless the OptixDenoiser was built with
     *      temporal denoising support.
     *
     * \param noisy_ch
     *      The name of the channel in the \c noisy parameter which contains
     *      the shading normal information of the noisy rendering.
     *
     * \return The denoised input.
     */
    ref<Bitmap> operator()(const ref<Bitmap> &noisy,
                           bool denoise_alpha = true,
                           const std::string &albedo_ch = "",
                           const std::string &normals_ch = "",
                           const Transform4f &to_sensor = Transform4f(),
                           const std::string &flow_ch = "",
                           const std::string &previous_denoised_ch = "",
                           const std::string &noisy_ch = "<root>") const;

    virtual std::string to_string() const override;

    MI_DECLARE_CLASS()
private:
    /// Helper function to validate tensor sizes
    void validate_input(const TensorXf &noisy,
                        const TensorXf &albedo,
                        const TensorXf &normals,
                        const TensorXf &flow,
                        const TensorXf &previous_denoised) const;

    ScalarVector2u m_input_size;
    CUdeviceptr m_state;
    uint32_t m_state_size;
    CUdeviceptr m_scratch;
    uint32_t m_scratch_size;
    OptixDenoiserOptions m_options;
    bool m_temporal;
    OptixDenoiserStructPtr m_denoiser;
    CUdeviceptr m_hdr_intensity;
};

MI_EXTERN_CLASS(OptixDenoiser)
NAMESPACE_END(mitsuba)

#endif // defined(MI_ENABLE_CUDA)
