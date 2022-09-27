#include <mitsuba/render/optixdenoiser.h>
#include <mitsuba/render/optix_api.h>
#include <drjit-core/optix.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
static OptixImage2D optixImage2DfromTensor(
    const typename OptixDenoiser<Float, Spectrum>::TensorXf &tensor,
    OptixPixelFormat pixel_format) {
    return { (CUdeviceptr) tensor.data(),
             (unsigned int) tensor.shape(1),
             (unsigned int) tensor.shape(0),
             (unsigned int) (tensor.shape(1) * tensor.shape(2) * sizeof(float)),
             (unsigned int) (tensor.shape(2) * sizeof(float)),
             pixel_format };
}

MI_VARIANT OptixDenoiser<Float, Spectrum>::OptixDenoiser(
    const ScalarVector2u &input_size, bool albedo, bool normals, bool temporal)
    : m_input_size(input_size), m_options({ albedo, normals }),
      m_temporal(temporal) {
    if constexpr (!dr::is_cuda_v<Float>)
        Throw("OptixDenoiser is only available in CUDA mode!");

    if (normals && !albedo)
        Throw("The denoiser cannot use normals to guide its process without "
              "also providing albedo information!");

    optix_initialize();

    init(input_size, temporal);
}

MI_VARIANT OptixDenoiser<Float, Spectrum>::~OptixDenoiser() {
    if (m_denoiser != nullptr)
        jit_optix_check(optixDenoiserDestroy(m_denoiser));
    jit_free(m_hdr_intensity);
    jit_free(m_state);
    jit_free(m_scratch);
}

MI_VARIANT
typename OptixDenoiser<Float, Spectrum>::TensorXf OptixDenoiser<Float, Spectrum>::operator()(
    const TensorXf &noisy, bool denoise_alpha, const TensorXf *albedo,
    const TensorXf *normals, const Transform4f* normals_transform, const TensorXf *flow,
    const TensorXf *previous_denoised) const {
    using TensorArray = typename TensorXf::Array;

    if ((albedo == nullptr) && m_options.guideAlbedo)
        Throw("The Denoiser was created with albedo guiding enabled. An albedo "
              "layer must be specified!");
    if ((normals == nullptr) && m_options.guideNormal)
        Throw("The Denoiser was created with normals guiding enabled. A normal"
              "layer must be specified!");
    if (((flow == nullptr) || (previous_denoised == nullptr)) && m_temporal)
        Throw("The Denoiser was created with temporal denoising enabled. Both "
              "the optical flow and the denoised previous frame must be "
              "specified!");
    if (m_input_size.x() != noisy.shape(1) ||
        m_input_size.y() != noisy.shape(0))
        Throw("The Denoiser was created for inputs of size %u x %u (width x "
              "height). You must create a new Denoiser object for inputs of "
              "different sizes!",
              m_input_size.x(), m_input_size.y());

    OptixDenoiserLayer layers = {};
    OptixPixelFormat input_pixel_format = (noisy.shape(2) == 3)
                                              ? OPTIX_PIXEL_FORMAT_FLOAT3
                                              : OPTIX_PIXEL_FORMAT_FLOAT4;
    layers.input =
        optixImage2DfromTensor<Float, Spectrum>(noisy, input_pixel_format);

    layers.output = layers.input;
    TensorArray output_data = dr::empty<TensorArray>(noisy.size());
    layers.output.data = output_data.data();

    CUstream stream = jit_cuda_stream();

    OptixDenoiserParams params = {};
    params.blendFactor = 0.0f;
    params.hdrAverageColor = nullptr;
    params.denoiseAlpha = denoise_alpha;
    params.hdrIntensity = m_hdr_intensity;
    jit_optix_check(optixDenoiserComputeIntensity(
        m_denoiser, stream, &layers.input, m_hdr_intensity, m_scratch,
        m_scratch_size));

    dr::schedule(noisy);

    if (m_options.guideAlbedo)
        dr::schedule(*albedo);

    std::unique_ptr<TensorXf> new_normals = nullptr;
    if (m_options.guideNormal) {
        new_normals = std::make_unique<TensorXf>(*normals);

        Normal3f n = dr::empty<Normal3f>(m_input_size.x() * m_input_size.y());
        for (size_t i = 0; i < 3; ++i) {
            UInt32 idx = dr::arange<UInt32>(i, new_normals->size(), 3);
            n[i] = dr::gather<Float>(normals->array(), idx);
        }

        // Apply transform and change from coordinate system with (X=left,
        // Y=up, Z=forward) to a system with (X=right, Y=up, Z=backward)
        if (normals_transform != nullptr)
            n = (*normals_transform) * n;
        n[0] = -n[0];
        n[2] = -n[2];

        for (size_t i = 0; i < 3; ++i) {
            UInt32 idx = dr::arange<UInt32>(i, new_normals->size(), 3);
            dr::scatter(new_normals->array(), n[i], idx);
        }

        dr::schedule(*new_normals);
    }

    if (m_temporal) {
        dr::schedule(*flow);
        dr::schedule(*previous_denoised);
    }

    OptixDenoiserGuideLayer guide_layer = {};

    dr::eval(); // All tensors must be evaluated before passing them to OptiX
    if (m_options.guideAlbedo)
        guide_layer.albedo = optixImage2DfromTensor<Float, Spectrum>(
            *albedo, OPTIX_PIXEL_FORMAT_FLOAT3);
    if (m_options.guideNormal)
        guide_layer.normal = optixImage2DfromTensor<Float, Spectrum>(
            *new_normals, OPTIX_PIXEL_FORMAT_FLOAT3);
    if (m_temporal) {
        guide_layer.flow = optixImage2DfromTensor<Float, Spectrum>(
            *flow, OPTIX_PIXEL_FORMAT_FLOAT2);
        layers.previousOutput = optixImage2DfromTensor<Float, Spectrum>(
            *previous_denoised, input_pixel_format);
    }

    jit_optix_check(optixDenoiserInvoke(m_denoiser, stream, &params, m_state,
                                        m_state_size, &guide_layer, &layers, 1,
                                        0, 0, m_scratch, m_scratch_size));

    size_t shape[3] = { noisy.shape(0), noisy.shape(1), noisy.shape(2) };
    return TensorXf(std::move(output_data), 3, shape);
}

MI_VARIANT
ref<Bitmap> OptixDenoiser<Float, Spectrum>::operator()(
    const ref<Bitmap> &noisy_, bool denoise_alpha, const std::string &albedo_ch,
    const std::string &normals_ch, const Transform4f &normals_transform,
    const std::string &flow_ch, const std::string &previous_denoised_ch,
    const std::string &noisy_ch) const {
    if (noisy_->pixel_format() != Bitmap::PixelFormat::MultiChannel) {
        size_t noisy_tensor_shape[3] = { noisy_->height(), noisy_->width(),
                                         noisy_->channel_count() };
        TensorXf noisy_tensor(noisy_->data(), 3, noisy_tensor_shape);
        TensorXf denoised;
        denoised = (*this)(noisy_tensor, denoise_alpha);

        void *denoised_data =
            jit_malloc_migrate(denoised.data(), AllocType::Host, false);
        jit_sync_thread();

        return new Bitmap((denoised.shape(2) == 3) ? Bitmap::PixelFormat::RGB
                                                   : Bitmap::PixelFormat::RGBA,
                          Struct::Type::Float32,
                          { denoised.shape(1), denoised.shape(0) },
                          denoised.shape(2), {}, (uint8_t *) denoised_data);
    }

    const Bitmap *noisy_bmp = nullptr;
    const Bitmap *albedo_bmp = nullptr;
    const Bitmap *normals_bmp = nullptr;
    const Bitmap *flow_bmp = nullptr;
    const Bitmap *prev_denoised_bmp = nullptr;

    bool found_albedo = albedo_ch == "";
    bool found_normals = normals_ch == "";
    bool found_flow = flow_ch == "";
    bool found_prev_denoised = previous_denoised_ch == "";

    // Search for each layer
    std::vector<std::pair<std::string, ref<Bitmap>>> res = noisy_->split();
    auto find_channel = [](const std::pair<std::string, ref<Bitmap>> &layer,
                           const std::string &channel_name, bool &flag,
                           const Bitmap *&bmp) {
        if (!flag && layer.first == channel_name) {
            flag = true;
            bmp = layer.second.get();
        }
    };
    for (auto &layer : res) {
        if (found_albedo && found_normals && found_flow &&
            found_prev_denoised && noisy_bmp != nullptr)
            break;
        if (noisy_bmp == nullptr && layer.first == noisy_ch)
            noisy_bmp = layer.second.get();

        find_channel(layer, albedo_ch, found_albedo, albedo_bmp);
        find_channel(layer, normals_ch, found_normals, normals_bmp);
        find_channel(layer, flow_ch, found_flow, flow_bmp);
        find_channel(layer, previous_denoised_ch, found_prev_denoised,
                     prev_denoised_bmp);
    }

    // Check that no layer is missing
    auto throw_missing_channel = [&](const std::string &channel) {
        Throw("Could not find layer with channel name '%s' in Bitmap:\n%s",
              channel, noisy_->to_string());
    };
    if (noisy_bmp == nullptr)
        throw_missing_channel(noisy_ch);
    if (!found_albedo)
        throw_missing_channel(albedo_ch);
    if (!found_normals)
        throw_missing_channel(normals_ch);
    if (!found_flow)
        throw_missing_channel(flow_ch);
    if (!found_prev_denoised)
        throw_missing_channel(previous_denoised_ch);

    // Transfer every layer into a TensorXf object
    size_t noisy_channel_count = noisy_bmp->channel_count();
    size_t noisy_tensor_shape[3] = { noisy_bmp->height(), noisy_bmp->width(),
                                     noisy_channel_count };
    TensorXf noisy_tensor(noisy_bmp->data(), 3, noisy_tensor_shape);
    auto setup_tensor = [](const Bitmap *bmp, std::unique_ptr<TensorXf> &tensor,
                           size_t channel_count) {
        if (bmp != nullptr) {
            size_t shape[3] = { bmp->height(), bmp->width(), channel_count };
            tensor = std::make_unique<TensorXf>(bmp->data(), 3, shape);
        }
    };

    std::unique_ptr<TensorXf> albedo_tensor = nullptr;
    setup_tensor(albedo_bmp, albedo_tensor, 3);
    std::unique_ptr<TensorXf> normals_tensor = nullptr;
    setup_tensor(normals_bmp, normals_tensor, 3);
    std::unique_ptr<TensorXf> flow_tensor = nullptr;
    setup_tensor(flow_bmp, flow_tensor, 2);
    std::unique_ptr<TensorXf> prev_denoised_tensor = nullptr;
    setup_tensor(prev_denoised_bmp, prev_denoised_tensor, noisy_channel_count);

    // Generate output
    TensorXf denoised =
        (*this)(noisy_tensor, denoise_alpha, albedo_tensor.get(),
                normals_tensor.get(), &normals_transform, flow_tensor.get(),
                prev_denoised_tensor.get());
    void *denoised_data =
        jit_malloc_migrate(denoised.data(), AllocType::Host, false);
    ref<Bitmap> output = new Bitmap(
        (noisy_channel_count = 3) ? Bitmap::PixelFormat::RGB
                                  : Bitmap::PixelFormat::RGBA,
        Struct::Type::Float32, { denoised.shape(1), denoised.shape(0) },
        denoised.shape(2), {});
    jit_sync_thread(); // Wait for `denoised_data` to be ready
    memcpy(output->data(), denoised_data, output->buffer_size());
    jit_free(denoised_data);

    return output;
}

MI_VARIANT
std::string OptixDenoiser<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << "OptixDenoiser[" << std::endl
        << "  albedo = " << m_options.guideAlbedo << "," << std::endl
        << "  normals = " << m_options.guideNormal << "," << std::endl
        << "  temporal = " << m_temporal << std::endl
        << "]";
    return oss.str();
}

MI_VARIANT
void OptixDenoiser<Float, Spectrum>::init(const ScalarVector2u &input_size,
                                          bool temporal) {
    OptixDeviceContext context = jit_optix_context();
    OptixDenoiserModelKind model_kind = temporal
                                            ? OPTIX_DENOISER_MODEL_KIND_TEMPORAL
                                            : OPTIX_DENOISER_MODEL_KIND_HDR;
    jit_optix_check(
        optixDenoiserCreate(context, model_kind, &m_options, &m_denoiser));

    OptixDenoiserSizes sizes = {};
    jit_optix_check(optixDenoiserComputeMemoryResources(
        m_denoiser, input_size.x(), input_size.y(), &sizes));

    CUstream stream = jit_cuda_stream();
    m_state_size = sizes.stateSizeInBytes;
    m_state = jit_malloc(AllocType::Device, m_state_size);
    m_scratch_size = sizes.withoutOverlapScratchSizeInBytes;
    m_scratch = jit_malloc(AllocType::Device, m_scratch_size);
    jit_optix_check(optixDenoiserSetup(m_denoiser, stream, input_size.x(),
                                       input_size.y(), m_state, m_state_size,
                                       m_scratch, m_scratch_size));
    m_hdr_intensity = jit_malloc(AllocType::Device, sizeof(float));
}

MI_IMPLEMENT_CLASS_VARIANT(OptixDenoiser, Object, "denoiser")
MI_INSTANTIATE_CLASS(OptixDenoiser)

NAMESPACE_END(mitsuba)
