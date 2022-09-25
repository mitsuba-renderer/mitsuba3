#pragma once

#if defined(MI_ENABLE_CUDA)

#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/optix_api.h>
#include <drjit/tensor.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * OptiX Denoiser
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB OptixDenoiser : public Object {
public:
    MI_IMPORT_TYPES()

    OptixDenoiser(const ScalarVector2u &input_size, bool albedo, bool normals,
             bool temporal);

    ~OptixDenoiser();

    TensorXf operator()(const TensorXf &noisy,
                        bool denoise_alpha = true,
                        const TensorXf *albedo = nullptr,
                        const TensorXf *normals = nullptr,
                        const TensorXf *previous_denoised = nullptr,
                        const TensorXf *flow = nullptr);

    ref<Bitmap> operator()(const ref<Bitmap> &noisy,
                           bool denoise_alpha = true,
                           const std::string &albedo_ch = "",
                           const std::string &normals_ch = "",
                           const std::string &flow_ch = "",
                           const std::string &previous_denoised_ch = "",
                           const std::string &noisy_ch = "<root>");

    virtual std::string to_string() const override;

    MI_DECLARE_CLASS()
private:
    ScalarVector2u m_input_size;
    CUdeviceptr m_state;
    uint32_t m_state_size;
    CUdeviceptr m_scratch;
    uint32_t m_scratch_size;
    OptixDenoiserOptions m_options;
    bool m_temporal;
    OptixDenoiserStructPtr m_denoiser;
    CUdeviceptr m_hdr_intensity;
    CUdeviceptr m_output_data;
};

MI_EXTERN_CLASS(OptixDenoiser)

NAMESPACE_END(mitsuba)

#endif // defined(MI_ENABLE_CUDA)
